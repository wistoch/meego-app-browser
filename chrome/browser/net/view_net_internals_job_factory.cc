// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/view_net_internals_job_factory.h"

#include <sstream>

#include "base/format_macros.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_util.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/proxy/proxy_service.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"

namespace {

const char kViewHttpCacheSubPath[] = "view-cache";

PassiveLogCollector* GetPassiveLogCollector(URLRequestContext* context) {
  // Really this is the same as:
  // g_browser_process->io_thread()->globals()->
  //     net_log.get()
  // (But we can't access g_browser_process from the IO thread).
  ChromeNetLog* chrome_net_log = static_cast<ChromeNetLog*>(
      static_cast<ChromeURLRequestContext*>(context)->net_log());
  return chrome_net_log->passive_collector();
}

PassiveLogCollector::RequestTracker* GetURLRequestTracker(
    URLRequestContext* context) {
  return GetPassiveLogCollector(context)->url_request_tracker();
}

PassiveLogCollector::RequestTracker* GetSocketStreamTracker(
    URLRequestContext* context) {
  return GetPassiveLogCollector(context)->socket_stream_tracker();
}

std::string GetDetails(const GURL& url) {
  DCHECK(ViewNetInternalsJobFactory::IsSupportedURL(url));
  size_t start = strlen(chrome::kNetworkViewInternalsURL);
  if (start >= url.spec().size())
    return std::string();
  return url.spec().substr(start);
}

GURL MakeURL(const std::string& details) {
  return GURL(std::string(chrome::kNetworkViewInternalsURL) + details);
}

// A job subclass that implements a protocol to inspect the internal
// state of the network stack.
class ViewNetInternalsJob : public URLRequestSimpleJob {
 public:

  explicit ViewNetInternalsJob(URLRequest* request)
      : URLRequestSimpleJob(request) {}

  // URLRequestSimpleJob methods:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

  // Overridden methods from URLRequestJob:
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

 private:
  ~ViewNetInternalsJob() {}

  // Returns true if the current request is for a "view-cache" URL.
  // If it is, then |key| is assigned the particular cache URL of the request.
  bool GetViewCacheKeyForRequest(std::string* key) const;
};

//------------------------------------------------------------------------------
// Format helpers.
//------------------------------------------------------------------------------

void OutputTextInPre(const std::string& text, std::string* out) {
  out->append("<pre>");
  out->append(EscapeForHTML(text));
  out->append("</pre>");
}

// Appends an input button to |data| with text |title| that sends the command
// string |command| back to the browser, and then refreshes the page.
void DrawCommandButton(const std::string& title,
                       const std::string& command,
                       std::string* data) {
  StringAppendF(data, "<input type=\"button\" value=\"%s\" "
               "onclick=\"DoCommand('%s')\" />",
               title.c_str(),
               command.c_str());
}

//------------------------------------------------------------------------------
// URLRequestContext helpers.
//------------------------------------------------------------------------------

net::HostResolverImpl* GetHostResolverImpl(URLRequestContext* context) {
  return context->host_resolver()->GetAsHostResolverImpl();
}

net::HostCache* GetHostCache(URLRequestContext* context) {
  if (GetHostResolverImpl(context))
    return GetHostResolverImpl(context)->cache();
  return NULL;
}

//------------------------------------------------------------------------------
// Subsection definitions.
//------------------------------------------------------------------------------

class SubSection {
 public:
  // |name| is the URL path component for this subsection.
  // |title| is the textual description.
  SubSection(SubSection* parent, const char* name, const char* title)
      : parent_(parent),
        name_(name),
        title_(title) {
  }

  virtual ~SubSection() {
    STLDeleteContainerPointers(children_.begin(), children_.end());
  }

  // Outputs the subsection's contents to |out|.
  virtual void OutputBody(URLRequestContext* context, std::string* out) {}

  // Outputs this subsection, and all of its children.
  void OutputRecursive(URLRequestContext* context, std::string* out) {
    if (!is_root()) {
      // Canonicalizing the URL escapes characters which cause problems in HTML.
      std::string section_url = MakeURL(GetFullyQualifiedName()).spec();

      // Print the heading.
      StringAppendF(
          out,
          "<div>"
          "<span class=subsection_title>%s</span> "
          "<span class=subsection_name>(<a href='%s'>%s</a>)<span>"
          "</div>",
          EscapeForHTML(title_).c_str(),
          section_url.c_str(),
          EscapeForHTML(section_url).c_str());

      out->append("<div class=subsection_body>");
    }

    OutputBody(context, out);

    for (size_t i = 0; i < children_.size(); ++i)
      children_[i]->OutputRecursive(context, out);

    if (!is_root())
      out->append("</div>");
  }

  // Returns the SubSection contained by |this| with fully qualified name
  // |dotted_name|, or NULL if none was found.
  SubSection* FindSubSectionByName(const std::string& dotted_name) {
    if (dotted_name == "")
      return this;

    std::string child_name;
    std::string child_sub_name;

    size_t split_pos = dotted_name.find('.');
    if (split_pos == std::string::npos) {
      child_name = dotted_name;
      child_sub_name = std::string();
    } else {
      child_name = dotted_name.substr(0, split_pos);
      child_sub_name = dotted_name.substr(split_pos + 1);
    }

    for (size_t i = 0; i < children_.size(); ++i) {
      if (children_[i]->name_ == child_name)
        return children_[i]->FindSubSectionByName(child_sub_name);
    }

    return NULL;  // Not found.
  }

  std::string GetFullyQualifiedName() {
    if (!parent_)
      return name_;

    std::string parent_name = parent_->GetFullyQualifiedName();
    if (parent_name.empty())
      return name_;

    return parent_name + "." + name_;
  }

  bool is_root() const {
    return parent_ == NULL;
  }

 protected:
  typedef std::vector<SubSection*> SubSectionList;

  void AddSubSection(SubSection* subsection) {
    children_.push_back(subsection);
  }

  SubSection* parent_;
  std::string name_;
  std::string title_;

  SubSectionList children_;
};

class ProxyServiceCurrentConfigSubSection : public SubSection {
 public:
  explicit ProxyServiceCurrentConfigSubSection(SubSection* parent)
      : SubSection(parent, "config", "Current configuration") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    DrawCommandButton("Force reload", "reload-proxy-config", out);

    net::ProxyService* proxy_service = context->proxy_service();
    if (proxy_service->config_has_been_initialized()) {
      // net::ProxyConfig defines an operator<<.
      std::ostringstream stream;
      stream << proxy_service->config();
      OutputTextInPre(stream.str(), out);
    } else {
      out->append("<i>Not yet initialized</i>");
    }
  }
};

class ProxyServiceLastInitLogSubSection : public SubSection {
 public:
  explicit ProxyServiceLastInitLogSubSection(SubSection* parent)
      : SubSection(parent, "init_log", "Last initialized load log") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    net::ProxyService* proxy_service = context->proxy_service();
    OutputTextInPre(net::NetLogUtil::PrettyPrintAsEventTree(
        proxy_service->init_proxy_resolver_log().entries(), 0), out);
  }
};

class ProxyServiceBadProxiesSubSection : public SubSection {
 public:
  explicit ProxyServiceBadProxiesSubSection(SubSection* parent)
      : SubSection(parent, "bad_proxies", "Bad Proxies") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    net::ProxyService* proxy_service = context->proxy_service();
    const net::ProxyRetryInfoMap& bad_proxies_map =
        proxy_service->proxy_retry_info();

    DrawCommandButton("Clear", "clear-badproxies", out);

    out->append("<table border=1>");
    out->append("<tr><th>Bad proxy server</th>"
                "<th>Remaining time until retry (ms)</th></tr>");

    for (net::ProxyRetryInfoMap::const_iterator it = bad_proxies_map.begin();
         it != bad_proxies_map.end(); ++it) {
      const std::string& proxy_uri = it->first;
      const net::ProxyRetryInfo& retry_info = it->second;

      // Note that ttl_ms may be negative, for the cases where entries have
      // expired but not been garbage collected yet.
      int ttl_ms = static_cast<int>(
          (retry_info.bad_until - base::TimeTicks::Now()).InMilliseconds());

      // Color expired entries blue.
      if (ttl_ms > 0)
        out->append("<tr>");
      else
        out->append("<tr style='color:blue'>");

      StringAppendF(out, "<td>%s</td><td>%d</td>",
                    EscapeForHTML(proxy_uri).c_str(),
                    ttl_ms);

      out->append("</tr>");
    }
    out->append("</table>");
  }
};

class ProxyServiceSubSection : public SubSection {
 public:
  explicit ProxyServiceSubSection(SubSection* parent)
      : SubSection(parent, "proxyservice", "ProxyService") {
    AddSubSection(new ProxyServiceCurrentConfigSubSection(this));
    AddSubSection(new ProxyServiceLastInitLogSubSection(this));
    AddSubSection(new ProxyServiceBadProxiesSubSection(this));
  }
};

class HostResolverCacheSubSection : public SubSection {
 public:
  explicit HostResolverCacheSubSection(SubSection* parent)
      : SubSection(parent, "hostcache", "HostCache") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    const net::HostCache* host_cache = GetHostCache(context);

    if (!host_cache || host_cache->caching_is_disabled()) {
      out->append("<i>Caching is disabled.</i>");
      return;
    }

    DrawCommandButton("Clear", "clear-hostcache", out);

    StringAppendF(
        out,
        "<ul><li>Size: %" PRIuS "</li>"
        "<li>Capacity: %" PRIuS "</li>"
        "<li>Time to live (ms) for success entries: %" PRId64"</li>"
        "<li>Time to live (ms) for failure entries: %" PRId64"</li></ul>",
        host_cache->size(),
        host_cache->max_entries(),
        host_cache->success_entry_ttl().InMilliseconds(),
        host_cache->failure_entry_ttl().InMilliseconds());

    out->append("<table border=1>"
                "<tr>"
                "<th>Host</th>"
                "<th>Address family</th>"
                "<th>Address list</th>"
                "<th>Time to live (ms)</th>"
                "</tr>");

    for (net::HostCache::EntryMap::const_iterator it =
             host_cache->entries().begin();
         it != host_cache->entries().end();
         ++it) {
      const net::HostCache::Key& key = it->first;
      const net::HostCache::Entry* entry = it->second.get();

      std::string address_family_str =
          AddressFamilyToString(key.address_family);

      // Note that ttl_ms may be negative, for the cases where entries have
      // expired but not been garbage collected yet.
      int ttl_ms = static_cast<int>(
          (entry->expiration - base::TimeTicks::Now()).InMilliseconds());

      // Color expired entries blue.
      if (ttl_ms > 0) {
        out->append("<tr>");
      } else {
        out->append("<tr style='color:blue'>");
      }

      // Stringify all of the addresses in the address list, separated
      // by newlines (br).
      std::string address_list_html;

      if (entry->error != net::OK) {
        address_list_html = "<span style='font-weight: bold; color:red'>" +
                            EscapeForHTML(net::ErrorToString(entry->error)) +
                            "</span>";
      } else {
        const struct addrinfo* current_address = entry->addrlist.head();
        while (current_address) {
          if (!address_list_html.empty())
            address_list_html += "<br>";
          address_list_html += EscapeForHTML(
              net::NetAddressToString(current_address));
          current_address = current_address->ai_next;
        }
      }

      StringAppendF(out,
                    "<td>%s</td><td>%s</td><td>%s</td>"
                    "<td>%d</td></tr>",
                    EscapeForHTML(key.hostname).c_str(),
                    EscapeForHTML(address_family_str).c_str(),
                    address_list_html.c_str(),
                    ttl_ms);
    }

    out->append("</table>");
  }

  static std::string AddressFamilyToString(net::AddressFamily address_family) {
    switch (address_family) {
      case net::ADDRESS_FAMILY_IPV4:
        return "IPV4";
      case net::ADDRESS_FAMILY_IPV6:
        return "IPV6";
      case net::ADDRESS_FAMILY_UNSPECIFIED:
        return "UNSPECIFIED";
      default:
        NOTREACHED();
        return "???";
    }
  }
};

class HostResolverTraceSubSection : public SubSection {
 public:
  explicit HostResolverTraceSubSection(SubSection* parent)
      : SubSection(parent, "trace", "Trace of requests") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    net::HostResolverImpl* resolver = GetHostResolverImpl(context);
    if (!resolver) {
      out->append("<i>Tracing is not supported by this resolver.</i>");
      return;
    }

    DrawCommandButton("Clear", "clear-hostresolver-trace", out);

    if (resolver->IsRequestsTracingEnabled()) {
      DrawCommandButton("Disable tracing", "hostresolver-trace-disable", out);
    } else {
      DrawCommandButton("Enable tracing", "hostresolver-trace-enable", out);
    }

    std::vector<net::NetLog::Entry> entries;
    if (resolver->GetRequestsTrace(&entries)) {
      out->append(
          "<p>To make sense of this trace, process it with the Python script "
          "formatter.py at "
          "<a href='http://src.chromium.org/viewvc/chrome/trunk/src/net/tools/"
          "dns_trace_formatter/'>net/tools/dns_trace_formatter</a></p>");
      OutputTextInPre(net::NetLogUtil::PrettyPrintAsEventTree(entries, 0),
                      out);
    } else {
      out->append("<p><i>No trace information, must enable tracing.</i></p>");
    }
  }
};

class HostResolverSubSection : public SubSection {
 public:
  explicit HostResolverSubSection(SubSection* parent)
      : SubSection(parent, "hostresolver", "HostResolver") {
    AddSubSection(new HostResolverCacheSubSection(this));
    AddSubSection(new HostResolverTraceSubSection(this));
  }
};

// Helper for the URLRequest "outstanding" and "live" sections.
void OutputURLAndLoadLog(const PassiveLogCollector::RequestInfo& request,
                         std::string* out) {
  out->append("<li>");
  out->append("<nobr>");
  out->append(EscapeForHTML(request.url));
  out->append("</nobr>");
  OutputTextInPre(
      net::NetLogUtil::PrettyPrintAsEventTree(
          request.entries,
          request.num_entries_truncated),
      out);
  out->append("</li>");
}

class URLRequestLiveSubSection : public SubSection {
 public:
  explicit URLRequestLiveSubSection(SubSection* parent)
      : SubSection(parent, "outstanding", "Outstanding requests") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    PassiveLogCollector::RequestInfoList requests =
        GetURLRequestTracker(context)->GetLiveRequests();

    out->append("<ol>");
    for (size_t i = 0; i < requests.size(); ++i) {
      // Reverse the list order, so we dispay from most recent to oldest.
      size_t index = requests.size() - i - 1;
      OutputURLAndLoadLog(requests[index], out);
    }
    out->append("</ol>");
  }
};

class URLRequestRecentSubSection : public SubSection {
 public:
  explicit URLRequestRecentSubSection(SubSection* parent)
      : SubSection(parent, "recent", "Recently completed requests") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    PassiveLogCollector::RequestInfoList recent =
        GetURLRequestTracker(context)->GetRecentlyDeceased();

    DrawCommandButton("Clear", "clear-urlrequest-graveyard", out);

    out->append("<ol>");
    for (size_t i = 0; i < recent.size(); ++i) {
      // Reverse the list order, so we dispay from most recent to oldest.
      size_t index = recent.size() - i - 1;
      OutputURLAndLoadLog(recent[index], out);
    }
    out->append("</ol>");
  }
};

class URLRequestSubSection : public SubSection {
 public:
  explicit URLRequestSubSection(SubSection* parent)
      : SubSection(parent, "urlrequest", "URLRequest") {
    AddSubSection(new URLRequestLiveSubSection(this));
    AddSubSection(new URLRequestRecentSubSection(this));
  }
};

class HttpCacheStatsSubSection : public SubSection {
 public:
  explicit HttpCacheStatsSubSection(SubSection* parent)
      : SubSection(parent, "stats", "Statistics") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    ViewCacheHelper::GetStatisticsHTML(context, out);
  }
};

class HttpCacheSection : public SubSection {
 public:
  explicit HttpCacheSection(SubSection* parent)
      : SubSection(parent, "httpcache", "HttpCache") {
    AddSubSection(new HttpCacheStatsSubSection(this));
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    // Advertise the view-cache URL (too much data to inline it).
    out->append("<p><a href='/");
    out->append(kViewHttpCacheSubPath);
    out->append("'>View all cache entries</a></p>");
  }
};

class SocketStreamLiveSubSection : public SubSection {
 public:
  explicit SocketStreamLiveSubSection(SubSection* parent)
      : SubSection(parent, "live", "Live SocketStreams") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    PassiveLogCollector::RequestInfoList sockets =
        GetSocketStreamTracker(context)->GetLiveRequests();

    out->append("<ol>");
    for (size_t i = 0; i < sockets.size(); ++i) {
      // Reverse the list order, so we dispay from most recent to oldest.
      size_t index = sockets.size() - i - 1;
      OutputURLAndLoadLog(sockets[index], out);
    }
    out->append("</ol>");
  }
};

class SocketStreamRecentSubSection : public SubSection {
 public:
  explicit SocketStreamRecentSubSection(SubSection* parent)
      : SubSection(parent, "recent", "Recently completed SocketStreams") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    PassiveLogCollector::RequestInfoList recent =
    GetSocketStreamTracker(context)->GetRecentlyDeceased();

    DrawCommandButton("Clear", "clear-socketstream-graveyard", out);

    out->append("<ol>");
    for (size_t i = 0; i < recent.size(); ++i) {
      // Reverse the list order, so we dispay from most recent to oldest.
      size_t index = recent.size() - i - 1;
      OutputURLAndLoadLog(recent[index], out);
    }
    out->append("</ol>");
  }
};

class SocketStreamSubSection : public SubSection {
 public:
  explicit SocketStreamSubSection(SubSection* parent)
      : SubSection(parent, "socketstream", "SocketStream") {
    AddSubSection(new SocketStreamLiveSubSection(this));
    AddSubSection(new SocketStreamRecentSubSection(this));
  }
};

class AllSubSections : public SubSection {
 public:
  AllSubSections() : SubSection(NULL, "", "") {
    AddSubSection(new ProxyServiceSubSection(this));
    AddSubSection(new HostResolverSubSection(this));
    AddSubSection(new URLRequestSubSection(this));
    AddSubSection(new HttpCacheSection(this));
    AddSubSection(new SocketStreamSubSection(this));
  }
};

bool HandleCommand(const std::string& command,
                   URLRequestContext* context) {
  if (StartsWithASCII(command, "full-logging-", true)) {
    bool enable_full_logging = (command == "full-logging-enable");
    GetURLRequestTracker(context)->SetUnbounded(enable_full_logging);
    GetSocketStreamTracker(context)->SetUnbounded(enable_full_logging);
    return true;
  }

  if (StartsWithASCII(command, "hostresolver-trace-", true)) {
    bool enable_tracing = (command == "hostresolver-trace-enable");
    if (GetHostResolverImpl(context)) {
      GetHostResolverImpl(context)->EnableRequestsTracing(enable_tracing);
    }
  }

  if (command == "clear-urlrequest-graveyard") {
    GetURLRequestTracker(context)->ClearRecentlyDeceased();
    return true;
  }

  if (command == "clear-socketstream-graveyard") {
    GetSocketStreamTracker(context)->ClearRecentlyDeceased();
    return true;
  }

  if (command == "clear-hostcache") {
    net::HostCache* host_cache = GetHostCache(context);
    if (host_cache)
      host_cache->clear();
    return true;
  }

  if (command == "clear-badproxies") {
    context->proxy_service()->ClearBadProxiesCache();
    return true;
  }

  if (command == "clear-hostresolver-trace") {
    if (GetHostResolverImpl(context))
      GetHostResolverImpl(context)->ClearRequestsTrace();
  }

  if (command == "reload-proxy-config") {
    context->proxy_service()->ForceReloadProxyConfig();
    return true;
  }

  return false;
}

// Process any query strings in the request (for actions like toggling
// full logging.
void ProcessQueryStringCommands(URLRequestContext* context,
                                const std::string& query) {
  if (!StartsWithASCII(query, "commands=", true)) {
    // Not a recognized format.
    return;
  }

  std::string commands_str = query.substr(strlen("commands="));
  commands_str = UnescapeURLComponent(commands_str, UnescapeRule::NORMAL);

  // The command list is comma-separated.
  std::vector<std::string> commands;
  SplitString(commands_str, ',', &commands);

  for (size_t i = 0; i < commands.size(); ++i)
    HandleCommand(commands[i], context);
}

// Appends some HTML controls to |data| that allow the user to enable full
// logging, and clear some of the already logged data.
void DrawControlsHeader(URLRequestContext* context, std::string* data) {
  bool is_full_logging_enabled =
      GetURLRequestTracker(context)->IsUnbounded() &&
      GetSocketStreamTracker(context)->IsUnbounded();

  data->append("<div style='margin-bottom: 10px'>");

  if (is_full_logging_enabled) {
    DrawCommandButton("Disable full logging", "full-logging-disable", data);
  } else {
    DrawCommandButton("Enable full logging", "full-logging-enable", data);
  }

  DrawCommandButton("Clear all data",
                    // Send a list of comma separated commands:
                    "clear-badproxies,"
                    "clear-hostcache,"
                    "clear-urlrequest-graveyard,"
                    "clear-socketstream-graveyard,"
                    "clear-hostresolver-trace",
                    data);

  data->append("</div>");
}

bool ViewNetInternalsJob::GetData(std::string* mime_type,
                                  std::string* charset,
                                  std::string* data) const {
  mime_type->assign("text/html");
  charset->assign("UTF-8");

  URLRequestContext* context =
      static_cast<URLRequestContext*>(request_->context());

  data->clear();

  // Use a different handler for "view-cache/*" subpaths.
  std::string cache_key;
  if (GetViewCacheKeyForRequest(&cache_key)) {
    GURL url = MakeURL(kViewHttpCacheSubPath + std::string("/"));
    ViewCacheHelper::GetEntryInfoHTML(cache_key, context, url.spec(), data);
    return true;
  }

  // Handle any query arguments as a command request, then redirect back to
  // the same URL stripped of query parameters. The redirect happens as part
  // of IsRedirectResponse().
  if (request_->url().has_query()) {
    ProcessQueryStringCommands(context, request_->url().query());
    return true;
  }

  std::string details = GetDetails(request_->url());

  data->append("<!DOCTYPE HTML>"
               "<html><head><title>Network internals</title>"
               "<style>"
               "body { font-family: sans-serif; font-size: 0.8em; }\n"
               "tt, code, pre { font-family: WebKitHack, monospace; }\n"
               ".subsection_body { margin: 10px 0 10px 2em; }\n"
               ".subsection_title { font-weight: bold; }\n"
               "</style>"
               "<script>\n"

               // Unfortunately we can't do XHR from chrome://net-internals
               // because the chrome:// protocol restricts access.
               //
               // So instead, we will send commands by doing a form
               // submission (which as a side effect will reload the page).
               "function DoCommand(command) {\n"
               "  document.getElementById('cmd').value = command;\n"
               "  document.getElementById('cmdsender').submit();\n"
               "}\n"

               "</script>\n"
               "</head><body>"
               "<form action='' method=GET id=cmdsender>"
               "<input type='hidden' id=cmd name='commands'>"
               "</form>"
               "<p><a href='http://dev.chromium.org/"
               "developers/design-documents/view-net-internals'>"
               "Help: how do I use this?</a></p>");

  DrawControlsHeader(context, data);

  SubSection* all = Singleton<AllSubSections>::get();
  SubSection* section = all;

  // Display only the subsection tree asked for.
  if (!details.empty())
    section = all->FindSubSectionByName(details);

  if (section) {
    section->OutputRecursive(context, data);
  } else {
    data->append("<i>Nothing found for \"");
    data->append(EscapeForHTML(details));
    data->append("\"</i>");
  }

  data->append("</body></html>");

  return true;
}

bool ViewNetInternalsJob::IsRedirectResponse(GURL* location,
                                             int* http_status_code) {
  if (request_->url().has_query() && !GetViewCacheKeyForRequest(NULL)) {
    // Strip the query parameters.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 307;
    return true;
  }
  return false;
}

bool ViewNetInternalsJob::GetViewCacheKeyForRequest(
    std::string* key) const {
  std::string path = GetDetails(request_->url());
  if (!StartsWithASCII(path, kViewHttpCacheSubPath, true))
    return false;

  if (path.size() > strlen(kViewHttpCacheSubPath) &&
      path[strlen(kViewHttpCacheSubPath)] != '/')
    return false;

  if (key && path.size() > strlen(kViewHttpCacheSubPath) + 1)
    *key = path.substr(strlen(kViewHttpCacheSubPath) + 1);

  return true;
}

}  // namespace

// static
bool ViewNetInternalsJobFactory::IsSupportedURL(const GURL& url) {
  // Note that kNetworkViewInternalsURL is terminated by a '/'.
  return StartsWithASCII(url.spec(),
                         chrome::kNetworkViewInternalsURL,
                         true /*case_sensitive*/);
}

// static
URLRequestJob* ViewNetInternalsJobFactory::CreateJobForRequest(
    URLRequest* request) {
  return new ViewNetInternalsJob(request);
}
