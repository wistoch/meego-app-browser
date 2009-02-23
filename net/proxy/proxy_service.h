// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/proxy/proxy_server.h"

class GURL;

namespace net {

class ProxyConfigService;
class ProxyInfo;
class ProxyResolver;

// Proxy configuration used to by the ProxyService.
class ProxyConfig {
 public:
  typedef int ID;

  // Indicates an invalid proxy config.
  enum { INVALID_ID = 0 };

  ProxyConfig();
  // Default copy-constructor and assignment operator are OK!

  // Used to numerically identify this configuration.
  ID id() const { return id_; }

  // True if the proxy configuration should be auto-detected.
  bool auto_detect;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url;

  // If non-empty, indicates the proxy server to use, given by:
  //
  //   proxy-uri = [<proxy-scheme>://]<proxy-host>[:"<proxy-port>]
  //
  // If the proxy to use depends on the scheme of the URL, can instead specify
  // a semicolon separated list of:
  //
  //   <url-scheme>"="<proxy-uri>
  //
  // For example:
  //   "http=foopy:80;ftp=foopy2"  -- use HTTP proxy "foopy:80" for http URLs,
  //                                  and HTTP proxy "foopy2:80" for ftp URLs.
  //   "foopy:80"                  -- use HTTP proxy "foopy:80" for all URLs.
  //   "socks4://foopy"            -- use SOCKS v4 proxy "foopy:1080" for all
  //                                  URLs.
  std::string proxy_rules;

  // Indicates a list of hosts that should bypass any proxy configuration.  For
  // these hosts, a direct connection should always be used.
  std::vector<std::string> proxy_bypass;

  // Indicates whether local names (no dots) bypass proxies.
  bool proxy_bypass_local_names;

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

 private:
  static int last_id_;
  int id_;
};

// Contains the information about when to retry a proxy server.
struct ProxyRetryInfo {
  // We should not retry until this time.
  base::TimeTicks bad_until;

  // This is the current delay. If the proxy is still bad, we need to increase
  // this delay.
  base::TimeDelta current_delay;
};

// Map of proxy servers with the associated RetryInfo structures.
// The key is a proxy URI string [<scheme>"://"]<host>":"<port>.
typedef std::map<std::string, ProxyRetryInfo> ProxyRetryInfoMap;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverWinHttp for example.
class ProxyService {
 public:
  // The instance takes ownership of |config_service| and |resolver|.
  ProxyService(ProxyConfigService* config_service,
               ProxyResolver* resolver);

  ~ProxyService();

  // Used internally to handle PAC queries.
  class PacRequest;

  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |pac_request| is cancelled
  // via CancelPacRequest.  |pac_request| is only valid while the completion
  // callback is still pending. NULL can be passed for |pac_request| if
  // the caller will not need to cancel the request.
  //
  // We use the three possible proxy access types in the following order, and
  // we only use one of them (no falling back to other access types if the
  // chosen one doesn't work).
  //   1.  named proxy
  //   2.  PAC URL
  //   3.  WPAD auto-detection
  //
  int ResolveProxy(const GURL& url,
                   ProxyInfo* results,
                   CompletionCallback* callback,
                   PacRequest** pac_request);

  // This method is called after a failure to connect or resolve a host name.
  // It gives the proxy service an opportunity to reconsider the proxy to use.
  // The |results| parameter contains the results returned by an earlier call
  // to ResolveProxy.  The semantics of this call are otherwise similar to
  // ResolveProxy.
  //
  // NULL can be passed for |pac_request| if the caller will not need to
  // cancel the request.
  //
  // Returns ERR_FAILED if there is not another proxy config to try.
  //
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* results,
                                CompletionCallback* callback,
                                PacRequest** pac_request);

  // Call this method with a non-null |pac_request| to cancel the PAC request.
  void CancelPacRequest(PacRequest* pac_request);

  // Create a proxy service using the specified settings. If |pi| is NULL then
  // the system's default proxy settings will be used (on Windows this will
  // use IE's settings).
  static ProxyService* Create(const ProxyInfo* pi);

  // Create a proxy service that always fails to fetch the proxy configuration,
  // so it falls back to direct connect.
  static ProxyService* CreateNull();

  // Set the ProxyScriptFetcher dependency. This is needed if the ProxyResolver
  // is of type ProxyResolverWithoutFetch. ProxyService takes ownership of
  // |proxy_script_fetcher|.
  void SetProxyScriptFetcher(ProxyScriptFetcher* proxy_script_fetcher) {
    proxy_script_fetcher_.reset(proxy_script_fetcher);
  }

 private:
  friend class PacRequest;

  ProxyResolver* resolver() { return resolver_.get(); }
  base::Thread* pac_thread() { return pac_thread_.get(); }

  // Identifies the proxy configuration.
  ProxyConfig::ID config_id() const { return config_.id(); }

  // Checks to see if the proxy configuration changed, and then updates config_
  // to reference the new configuration.
  void UpdateConfig();

  // Tries to update the configuration if it hasn't been checked in a while.
  void UpdateConfigIfOld();

  // Returns true if this ProxyService is downloading a PAC script on behalf
  // of ProxyResolverWithoutFetch. Resolve requests will be frozen until
  // the fetch has completed.
  bool IsFetchingPacScript() const {
    return in_progress_fetch_config_id_ != ProxyConfig::INVALID_ID;
  }

  // Callback for when the PAC script has finished downloading.
  void OnScriptFetchCompletion(int result);
  
  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  // (ProxyResolver runs on PAC thread.)
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Starts the PAC thread if it isn't already running.
  void InitPacThread();

  // Starts the next request from |pending_requests_| is possible.
  // |recent_req| is the request that just got added, or NULL.
  void ProcessPendingRequests(PacRequest* recent_req);

  // Removes the front entry of the requests queue. |expected_req| is our
  // expectation of what the front of the request queue is; it is only used by
  // DCHECK for verification purposes.
  void RemoveFrontOfRequestQueue(PacRequest* expected_req);

  // Called to indicate that a PacRequest completed.  The |config_id| parameter
  // indicates the proxy configuration that was queried.  |result_code| is OK
  // if the PAC file could be downloaded and executed.  Otherwise, it is an
  // error code, indicating a bad proxy configuration.
  void DidCompletePacRequest(int config_id, int result_code);

  // Returns true if the URL passed in should not go through the proxy server.
  // 1. If the bypass proxy list contains the string <local> and the URL
  //    passed in is a local URL, i.e. a URL without a DOT (.)
  // 2. The URL matches one of the entities in the proxy bypass list.
  bool ShouldBypassProxyForURL(const GURL& url);

  scoped_ptr<ProxyConfigService> config_service_;
  scoped_ptr<ProxyResolver> resolver_;
  scoped_ptr<base::Thread> pac_thread_;

  // We store the proxy config and a counter that is incremented each time
  // the config changes.
  ProxyConfig config_;

  // Indicates that the configuration is bad and should be ignored.
  bool config_is_bad_;

  // false if the ProxyService has not been initialized yet.
  bool config_has_been_updated_;

  // The time when the proxy configuration was last read from the system.
  base::TimeTicks config_last_update_time_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // FIFO queue of pending/inprogress requests.
  typedef std::deque<scoped_refptr<PacRequest> > PendingRequestsQueue;
  PendingRequestsQueue pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be NULL if our ProxyResolver has no need for
  // external PAC script fetching.
  scoped_ptr<ProxyScriptFetcher> proxy_script_fetcher_;

  // Callback for when |proxy_script_fetcher_| is done.
  CompletionCallbackImpl<ProxyService> proxy_script_fetcher_callback_;

  // The ID of the configuration for which we last downloaded a PAC script.
  // If no PAC script has been fetched yet, will be ProxyConfig::INVALID_ID.
  ProxyConfig::ID fetched_pac_config_id_;

  // The error corresponding with |fetched_pac_config_id_|, or OK.
  int fetched_pac_error_;

  // The ID of the configuration for which we are currently downloading the
  // PAC script. If no fetch is in progress, will be ProxyConfig::INVALID_ID.
  ProxyConfig::ID in_progress_fetch_config_id_;

  // The results of the current in progress fetch, or empty string.
  std::string in_progress_fetch_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// This class is used to hold a list of proxies returned by GetProxyForUrl or
// manually configured. It handles proxy fallback if multiple servers are
// specified.
class ProxyList {
 public:
  // Initializes the proxy list to a string containing one or more proxy servers
  // delimited by a semicolon.
  void Set(const std::string& proxy_uri_list);

  // Remove all proxies known to be bad from the proxy list.
  void RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Delete any entry which doesn't have one of the specified proxy schemes.
  // |scheme_bit_field| is a bunch of ProxyServer::Scheme bitwise ORed together.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  // Returns the first valid proxy server in the list.
  ProxyServer Get() const;

  // Set the list by parsing the pac result |pac_string|.
  // Some examples for |pac_string|:
  //   "DIRECT"
  //   "PROXY foopy1"
  //   "PROXY foopy1; SOCKS4 foopy2:1188"
  void SetFromPacString(const std::string& pac_string);

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy".
  std::string ToPacString() const;

  // Marks the current proxy server as bad and deletes it from the list.  The
  // list of known bad proxies is given by proxy_retry_info.  Returns true if
  // there is another server available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info);

 private:
  // List of proxies.
  std::vector<ProxyServer> proxies_;
};

// This object holds proxy information returned by ResolveProxy.
class ProxyInfo {
 public:
  ProxyInfo();
  // Default copy-constructor and assignment operator are OK!

  // Use the same proxy server as the given |proxy_info|.
  void Use(const ProxyInfo& proxy_info);

  // Use a direct connection.
  void UseDirect();

  // Use a specific proxy server, of the form:
  //   proxy-uri = [<scheme> "://"] <hostname> [":" <port>]
  // This may optionally be a semi-colon delimited list of <proxy-uri>.
  // It is OK to have LWS between entries.
  void UseNamedProxy(const std::string& proxy_uri_list);

  // Parse from the given PAC result.
  void UsePacString(const std::string& pac_string) {
    proxy_list_.SetFromPacString(pac_string);
  }

  // Returns true if this proxy info specifies a direct connection.
  bool is_direct() const { return proxy_list_.Get().is_direct(); }

  // Returns the first valid proxy server.
  ProxyServer proxy_server() const { return proxy_list_.Get(); }

  // See description in ProxyList::ToPacString().
  std::string ToPacString();

  // Marks the current proxy as bad. Returns true if there is another proxy
  // available to try in proxy list_.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info) {
    return proxy_list_.Fallback(proxy_retry_info);
  }

  // Remove all proxies known to be bad from the proxy list.
  void RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info) {
    proxy_list_.RemoveBadProxies(proxy_retry_info);
  }

  // Delete any entry which doesn't have one of the specified proxy schemes.
  void RemoveProxiesWithoutScheme(int scheme_bit_field) {
    proxy_list_.RemoveProxiesWithoutScheme(scheme_bit_field);
  }

 private:
  friend class ProxyService;

  // If proxy_list_ is set to empty, then a "direct" connection is indicated.
  ProxyList proxy_list_;

  // This value identifies the proxy config used to initialize this object.
  ProxyConfig::ID config_id_;

  // This flag is false when the proxy configuration was known to be bad when
  // this proxy info was initialized.  In such cases, we know that if this
  // proxy info does not yield a connection that we might want to reconsider
  // the proxy config given by config_id_.
  bool config_was_tried_;
};

// Synchronously fetch the system's proxy configuration settings. Called on
// the IO Thread.
class ProxyConfigService {
 public:
  virtual ~ProxyConfigService() {}

  // Get the proxy configuration.  Returns OK if successful or an error code if
  // otherwise.  |config| should be in its initial state when this method is
  // called.
  virtual int GetProxyConfig(ProxyConfig* config) = 0;
};

// Synchronously resolve the proxy for a URL, using a PAC script. Called on the
// PAC Thread.
class ProxyResolver {
 public:

  // If a subclass sets |does_fetch| to false, then the owning ProxyResolver
  // will download PAC scripts on our behalf, and notify changes with
  // SetPacScript(). Otherwise the subclass is expected to fetch the
  // PAC script internally, and SetPacScript() will go unused.
  ProxyResolver(bool does_fetch) : does_fetch_(does_fetch) {}

  virtual ~ProxyResolver() {}

  // Query the proxy auto-config file (specified by |pac_url|) for the proxy to
  // use to load the given |query_url|.  Returns OK if successful or an error
  // code otherwise.
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results) = 0;

  // Called whenever the PAC script has changed, with the contents of the
  // PAC script. |bytes| may be empty string if there was a fetch error.
  virtual void SetPacScript(const std::string& bytes) {
    // Must override SetPacScript() if |does_fetch_ = true|.
    NOTREACHED();
  }

  bool does_fetch() const { return does_fetch_; }

 protected:
  bool does_fetch_;
};

// Wrapper for invoking methods on a ProxyService synchronously.
class SyncProxyServiceHelper
    : public base::RefCountedThreadSafe<SyncProxyServiceHelper> {
 public:
  SyncProxyServiceHelper(MessageLoop* io_message_loop,
                         ProxyService* proxy_service);

  int ResolveProxy(const GURL& url, ProxyInfo* proxy_info);
  int ReconsiderProxyAfterError(const GURL& url, ProxyInfo* proxy_info);

 private:
  void StartAsyncResolve(const GURL& url);
  void StartAsyncReconsider(const GURL& url);

  void OnCompletion(int result);

  MessageLoop* io_message_loop_;
  ProxyService* proxy_service_;

  base::WaitableEvent event_;
  CompletionCallbackImpl<SyncProxyServiceHelper> callback_;
  ProxyInfo proxy_info_;
  int result_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVICE_H_

