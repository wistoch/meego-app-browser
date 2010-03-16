// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/proxy/init_proxy_resolver.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher.h"
#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_config_service_mac.h"
#include "net/proxy/proxy_resolver_mac.h"
#elif defined(OS_LINUX)
#include "net/proxy/proxy_config_service_linux.h"
#endif
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/single_threaded_proxy_resolver.h"
#include "net/url_request/url_request_context.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

static const size_t kMaxNumNetLogEntries = 100;

// Config getter that fails every time.
class ProxyConfigServiceNull : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  virtual int GetProxyConfig(ProxyConfig* config) {
    return ERR_NOT_IMPLEMENTED;
  }
};

// Proxy resolver that fails every time.
class ProxyResolverNull : public ProxyResolver {
 public:
  ProxyResolverNull() : ProxyResolver(false /*expects_pac_bytes*/) {}

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) {
    return ERR_NOT_IMPLEMENTED;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

 private:
  virtual int SetPacScript(const GURL& /*pac_url*/,
                           const std::string& /*pac_bytes*/,
                           CompletionCallback* /*callback*/) {
    return ERR_NOT_IMPLEMENTED;
  }
};

// ProxyService::PacRequest ---------------------------------------------------

class ProxyService::PacRequest
    : public base::RefCounted<ProxyService::PacRequest> {
 public:
  PacRequest(ProxyService* service,
             const GURL& url,
             ProxyInfo* results,
             CompletionCallback* user_callback,
             const BoundNetLog& net_log)
      : service_(service),
        user_callback_(user_callback),
        ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
            this, &PacRequest::QueryComplete)),
        results_(results),
        url_(url),
        resolve_job_(NULL),
        config_id_(ProxyConfig::INVALID_ID),
        net_log_(net_log) {
    DCHECK(user_callback);
  }

  // Starts the resolve proxy request.
  int Start() {
    DCHECK(!was_cancelled());
    DCHECK(!is_started());

    config_id_ = service_->config_.id();

    return resolver()->GetProxyForURL(
        url_, results_, &io_callback_, &resolve_job_, net_log_);
  }

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_;
  }

  void StartAndCompleteCheckingForSynchronous() {
    int rv = service_->TryToCompleteSynchronously(url_, results_);
    if (rv == ERR_IO_PENDING)
      rv = Start();
    if (rv != ERR_IO_PENDING)
      QueryComplete(rv);
  }

  void CancelResolveJob() {
    DCHECK(is_started());
    // The request may already be running in the resolver.
    resolver()->CancelRequest(resolve_job_);
    resolve_job_ = NULL;
    DCHECK(!is_started());
  }

  void Cancel() {
    net_log_.AddEvent(NetLog::TYPE_CANCELLED);

    if (is_started())
      CancelResolveJob();

    // Mark as cancelled, to prevent accessing this again later.
    service_ = NULL;
    user_callback_ = NULL;
    results_ = NULL;

    net_log_.EndEvent(NetLog::TYPE_PROXY_SERVICE);
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const { return user_callback_ == NULL; }

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int QueryDidComplete(int result_code) {
    DCHECK(!was_cancelled());

    // Make a note in the results which configuration was in use at the
    // time of the resolve.
    results_->config_id_ = config_id_;

    // Reset the state associated with in-progress-resolve.
    resolve_job_ = NULL;
    config_id_ = ProxyConfig::INVALID_ID;

    return service_->DidFinishResolvingProxy(results_, result_code, net_log_);
  }

  BoundNetLog* net_log() { return &net_log_; }

 private:
  friend class base::RefCounted<ProxyService::PacRequest>;

  ~PacRequest() {}

  // Callback for when the ProxyResolver request has completed.
  void QueryComplete(int result_code) {
    result_code = QueryDidComplete(result_code);

    // Remove this completed PacRequest from the service's pending list.
    /// (which will probably cause deletion of |this|).
    CompletionCallback* callback = user_callback_;
    service_->RemovePendingRequest(this);

    callback->Run(result_code);
  }

  ProxyResolver* resolver() const { return service_->resolver_.get(); }

  // Note that we don't hold a reference to the ProxyService. Outstanding
  // requests are cancelled during ~ProxyService, so this is guaranteed
  // to be valid throughout our lifetime.
  ProxyService* service_;
  CompletionCallback* user_callback_;
  CompletionCallbackImpl<PacRequest> io_callback_;
  ProxyInfo* results_;
  GURL url_;
  ProxyResolver::RequestHandle resolve_job_;
  ProxyConfig::ID config_id_;  // The config id when the resolve was started.
  BoundNetLog net_log_;
};

// ProxyService ---------------------------------------------------------------

ProxyService::ProxyService(ProxyConfigService* config_service,
                           ProxyResolver* resolver,
                           NetworkChangeNotifier* network_change_notifier,
                           const BoundNetLog& init_proxy_resolver_log)
    : config_service_(config_service),
      resolver_(resolver),
      next_config_id_(1),
      should_use_proxy_resolver_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(init_proxy_resolver_callback_(
          this, &ProxyService::OnInitProxyResolverComplete)),
      init_proxy_resolver_log_(init_proxy_resolver_log),
      network_change_notifier_(network_change_notifier) {
  // Register to receive network change notifications.
  if (network_change_notifier_)
    network_change_notifier_->AddObserver(this);
}

// static
ProxyService* ProxyService::Create(
    ProxyConfigService* proxy_config_service,
    bool use_v8_resolver,
    URLRequestContext* url_request_context,
    NetworkChangeNotifier* network_change_notifier,
    NetLog* net_log,
    MessageLoop* io_loop) {
  ProxyResolver* proxy_resolver;

  if (use_v8_resolver) {
    // Send javascript errors and alerts to LOG(INFO).
    HostResolver* host_resolver = url_request_context->host_resolver();
    ProxyResolverJSBindings* js_bindings =
        ProxyResolverJSBindings::CreateDefault(host_resolver, io_loop);

    proxy_resolver = new ProxyResolverV8(js_bindings);
  } else {
    proxy_resolver = CreateNonV8ProxyResolver();
  }

  // Wrap the (synchronous) ProxyResolver implementation in a single-threaded
  // runner. This will dispatch requests to a threadpool of size 1.
  proxy_resolver = new SingleThreadedProxyResolver(proxy_resolver);

  ProxyService* proxy_service = new ProxyService(
      proxy_config_service, proxy_resolver, network_change_notifier,
      BoundNetLog::Make(net_log, NetLog::SOURCE_INIT_PROXY_RESOLVER));

  if (proxy_resolver->expects_pac_bytes()) {
    // Configure PAC script downloads to be issued using |url_request_context|.
    DCHECK(url_request_context);
    proxy_service->SetProxyScriptFetcher(
        ProxyScriptFetcher::Create(url_request_context));
  }

  return proxy_service;
}

// static
ProxyService* ProxyService::CreateFixed(const ProxyConfig& pc) {
  return Create(new ProxyConfigServiceFixed(pc), false, NULL, NULL,
                NULL, NULL);
}

// static
ProxyService* ProxyService::CreateNull() {
  // Use a configuration fetcher and proxy resolver which always fail.
  return new ProxyService(new ProxyConfigServiceNull,
                          new ProxyResolverNull,
                          NULL,
                          BoundNetLog());
}

int ProxyService::ResolveProxy(const GURL& raw_url,
                               ProxyInfo* result,
                               CompletionCallback* callback,
                               PacRequest** pac_request,
                               const BoundNetLog& net_log) {
  DCHECK(callback);

  net_log.BeginEvent(NetLog::TYPE_PROXY_SERVICE);

  // Strip away any reference fragments and the username/password, as they
  // are not relevant to proxy resolution.
  GURL url = SimplifyUrlForRequest(raw_url);

  // Check if the request can be completed right away. This is the case when
  // using a direct connection, or when the config is bad.
  UpdateConfigIfOld(net_log);
  int rv = TryToCompleteSynchronously(url, result);
  if (rv != ERR_IO_PENDING)
    return DidFinishResolvingProxy(result, rv, net_log);

  scoped_refptr<PacRequest> req =
      new PacRequest(this, url, result, callback, net_log);

  bool resolver_is_ready = !IsInitializingProxyResolver();

  if (resolver_is_ready) {
    // Start the resolve request.
    rv = req->Start();
    if (rv != ERR_IO_PENDING)
      return req->QueryDidComplete(rv);
  } else {
    req->net_log()->BeginEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);
  }

  DCHECK_EQ(ERR_IO_PENDING, rv);
  DCHECK(!ContainsPendingRequest(req));
  pending_requests_.push_back(req);

  // Completion will be notifed through |callback|, unless the caller cancels
  // the request using |pac_request|.
  if (pac_request)
    *pac_request = req.get();
  return rv;  // ERR_IO_PENDING
}

int ProxyService::TryToCompleteSynchronously(const GURL& url,
                                             ProxyInfo* result) {
  result->config_id_ = config_.id();

  DCHECK(config_.id() != ProxyConfig::INVALID_ID);

  if (should_use_proxy_resolver_ || IsInitializingProxyResolver()) {
    // May need to go through ProxyResolver for this.
    return ERR_IO_PENDING;
  }

  // Use the manual proxy settings.
  config_.proxy_rules().Apply(url, result);
  return OK;
}

ProxyService::~ProxyService() {
  // Unregister to receive network change notifications.
  if (network_change_notifier_)
    network_change_notifier_->RemoveObserver(this);

  // Cancel any inprogress requests.
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    (*it)->Cancel();
  }

  // Make sure that InitProxyResolver gets destroyed BEFORE the
  // CapturingNetLog it is using is deleted.
  init_proxy_resolver_.reset();
}

void ProxyService::SuspendAllPendingRequests() {
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    PacRequest* req = it->get();
    if (req->is_started()) {
      req->CancelResolveJob();

      req->net_log()->BeginEvent(
          NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);
    }
  }
}

void ProxyService::ResumeAllPendingRequests() {
  DCHECK(!IsInitializingProxyResolver());

  // Make a copy in case |this| is deleted during the synchronous completion
  // of one of the requests. If |this| is deleted then all of the PacRequest
  // instances will be Cancel()-ed.
  PendingRequests pending_copy = pending_requests_;

  for (PendingRequests::iterator it = pending_copy.begin();
       it != pending_copy.end();
       ++it) {
    PacRequest* req = it->get();
    if (!req->is_started() && !req->was_cancelled()) {
      req->net_log()->EndEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);

      // Note that we re-check for synchronous completion, in case we are
      // no longer using a ProxyResolver (can happen if we fell-back to manual).
      req->StartAndCompleteCheckingForSynchronous();
    }
  }
}

void ProxyService::OnInitProxyResolverComplete(int result) {
  DCHECK(init_proxy_resolver_.get());
  DCHECK(config_.MayRequirePACResolver());
  DCHECK(!should_use_proxy_resolver_);
  init_proxy_resolver_.reset();

  should_use_proxy_resolver_ = result == OK;

  if (result != OK) {
    LOG(INFO) << "Failed configuring with PAC script, falling-back to manual "
                 "proxy servers.";
  }

  // Resume any requests which we had to defer until the PAC script was
  // downloaded.
  ResumeAllPendingRequests();
}

int ProxyService::ReconsiderProxyAfterError(const GURL& url,
                                            ProxyInfo* result,
                                            CompletionCallback* callback,
                                            PacRequest** pac_request,
                                            const BoundNetLog& net_log) {
  // Check to see if we have a new config since ResolveProxy was called.  We
  // want to re-run ResolveProxy in two cases: 1) we have a new config, or 2) a
  // direct connection failed and we never tried the current config.

  bool re_resolve = result->config_id_ != config_.id();
  if (!re_resolve) {
    UpdateConfig(net_log);
    if (result->config_id_ != config_.id()) {
      // A new configuration!
      re_resolve = true;
    }
  }
  if (re_resolve) {
    // If we have a new config or the config was never tried, we delete the
    // list of bad proxies and we try again.
    proxy_retry_info_.clear();
    return ResolveProxy(url, result, callback, pac_request, net_log);
  }

  // We don't have new proxy settings to try, try to fallback to the next proxy
  // in the list.
  bool did_fallback = result->Fallback(&proxy_retry_info_);

  // Return synchronous failure if there is nothing left to fall-back to.
  // TODO(eroman): This is a yucky API, clean it up.
  return did_fallback ? OK : ERR_FAILED;
}

void ProxyService::CancelPacRequest(PacRequest* req) {
  DCHECK(req);
  req->Cancel();
  RemovePendingRequest(req);
}

bool ProxyService::ContainsPendingRequest(PacRequest* req) {
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  return pending_requests_.end() != it;
}

void ProxyService::RemovePendingRequest(PacRequest* req) {
  DCHECK(ContainsPendingRequest(req));
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  pending_requests_.erase(it);
}

int ProxyService::DidFinishResolvingProxy(ProxyInfo* result,
                                          int result_code,
                                          const BoundNetLog& net_log) {
  // Log the result of the proxy resolution.
  if (result_code == OK) {
    // When full logging is enabled, dump the proxy list.
    if (net_log.HasListener()) {
      net_log.AddString(
          std::string("Resolved proxy list: ") + result->ToPacString());
    }
    result->DeprioritizeBadProxies(proxy_retry_info_);
  } else {
    net_log.AddStringLiteral(
        "Got an error from proxy resolver, falling-back to DIRECT.");
    net_log.AddErrorCode(result_code);

    // Fall-back to direct when the proxy resolver fails. This corresponds
    // with a javascript runtime error in the PAC script.
    //
    // This implicit fall-back to direct matches Firefox 3.5 and
    // Internet Explorer 8. For more information, see:
    //
    // http://www.chromium.org/developers/design-documents/proxy-settings-fallback
    result->UseDirect();
    result_code = OK;
  }

  net_log.EndEvent(NetLog::TYPE_PROXY_SERVICE);
  return result_code;
}

void ProxyService::SetProxyScriptFetcher(
    ProxyScriptFetcher* proxy_script_fetcher) {
  if (init_proxy_resolver_.get()) {
    // We need to be careful to first cancel |init_proxy_resolver_|, since it
    // holds a pointer to the old proxy script fetcher we are about to delete.

    DCHECK(IsInitializingProxyResolver());
    init_proxy_resolver_.reset();
    proxy_script_fetcher_.reset(proxy_script_fetcher);

    // Restart the initialization, using the new proxy script fetcher.
    StartInitProxyResolver();
  } else {
    proxy_script_fetcher_.reset(proxy_script_fetcher);
  }
}

ProxyScriptFetcher* ProxyService::GetProxyScriptFetcher() const {
  return proxy_script_fetcher_.get();
}

void ProxyService::ResetConfigService(
    ProxyConfigService* new_proxy_config_service) {
  config_service_.reset(new_proxy_config_service);
  UpdateConfig(NULL);
}

void ProxyService::PurgeMemory() {
  if (resolver_.get())
    resolver_->PurgeMemory();
}

void ProxyService::ForceReloadProxyConfig() {
  // Mark the current configuration as being un-initialized, then force it to
  // start updating (normally this would happen lazily during the next
  // call to ResolveProxy()).
  config_.set_id(ProxyConfig::INVALID_ID);
  UpdateConfig(NULL);
}

// static
ProxyConfigService* ProxyService::CreateSystemProxyConfigService(
    MessageLoop* io_loop, MessageLoop* file_loop) {
#if defined(OS_WIN)
  return new ProxyConfigServiceWin();
#elif defined(OS_MACOSX)
  return new ProxyConfigServiceMac();
#elif defined(OS_LINUX)
  ProxyConfigServiceLinux* linux_config_service
      = new ProxyConfigServiceLinux();

  // Assume we got called from the UI loop, which runs the default
  // glib main loop, so the current thread is where we should be
  // running gconf calls from.
  MessageLoop* glib_default_loop = MessageLoopForUI::current();

  // The file loop should be a MessageLoopForIO on Linux.
  DCHECK_EQ(MessageLoop::TYPE_IO, file_loop->type());

  // Synchronously fetch the current proxy config (since we are
  // running on glib_default_loop). Additionally register for
  // notifications (delivered in either |glib_default_loop| or
  // |file_loop|) to keep us updated when the proxy config changes.
  linux_config_service->SetupAndFetchInitialConfig(glib_default_loop, io_loop,
      static_cast<MessageLoopForIO*>(file_loop));

  return linux_config_service;
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return new ProxyConfigServiceNull();
#endif
}

// static
ProxyResolver* ProxyService::CreateNonV8ProxyResolver() {
#if defined(OS_WIN)
  return new ProxyResolverWinHttp();
#elif defined(OS_MACOSX)
  return new ProxyResolverMac();
#else
  LOG(WARNING) << "PAC support disabled because there is no fallback "
                  "non-V8 implementation";
  return new ProxyResolverNull();
#endif
}

void ProxyService::UpdateConfig(const BoundNetLog& net_log) {
  bool is_first_update = !config_has_been_initialized();

  ProxyConfig latest;

  // Fetch the proxy settings.
  TimeTicks start_time = TimeTicks::Now();
  net_log.BeginEvent(
      NetLog::TYPE_PROXY_SERVICE_POLL_CONFIG_SERVICE_FOR_CHANGES);
  int rv = config_service_->GetProxyConfig(&latest);
  net_log.EndEvent(NetLog::TYPE_PROXY_SERVICE_POLL_CONFIG_SERVICE_FOR_CHANGES);
  TimeTicks end_time = TimeTicks::Now();

  // Record how long the call to config_service_->GetConfig() above took.
  // On some setups of Windows, we have reports that querying the system
  // proxy settings can take multiple seconds (http://crbug.com/12189).
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.ProxyPollConfigurationTime",
                             end_time - start_time,
                             TimeDelta::FromMilliseconds(1),
                             TimeDelta::FromSeconds(30),
                             50);

  if (rv != OK) {
    if (is_first_update) {
      // Default to direct-connection if the first fetch fails.
      LOG(INFO) << "Failed initial proxy configuration fetch.";
      SetConfig(ProxyConfig());
    }
    return;
  }
  config_last_update_time_ = TimeTicks::Now();

  if (!is_first_update && latest.Equals(config_))
    return;

  SetConfig(latest);
}

void ProxyService::SetConfig(const ProxyConfig& config) {
  config_ = config;

  // Increment the ID to reflect that the config has changed.
  config_.set_id(next_config_id_++);

  // Reset state associated with latest config.
  proxy_retry_info_.clear();

  // Cancel any PAC fetching / ProxyResolver::SetPacScript() which was
  // in progress for the previous configuration.
  init_proxy_resolver_.reset();
  should_use_proxy_resolver_ = false;

  // Start downloading + testing the PAC scripts for this new configuration.
  if (config_.MayRequirePACResolver()) {
    // Since InitProxyResolver will be playing around with the proxy resolver
    // as it tests the parsing of various PAC scripts, make sure there is
    // nothing in-flight in |resolver_|. These paused requests are resumed by
    // OnInitProxyResolverComplete().
    SuspendAllPendingRequests();

    // Calls OnInitProxyResolverComplete() on completion.
    StartInitProxyResolver();
  }
}

void ProxyService::StartInitProxyResolver() {
  DCHECK(!init_proxy_resolver_.get());

  init_proxy_resolver_.reset(
      new InitProxyResolver(resolver_.get(), proxy_script_fetcher_.get()));

  int rv = init_proxy_resolver_->Init(
      config_, &init_proxy_resolver_callback_,
      init_proxy_resolver_log_);

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyService::UpdateConfigIfOld(const BoundNetLog& net_log) {
  // The overhead of calling ProxyConfigService::GetProxyConfig is very low.
  const TimeDelta kProxyConfigMaxAge = TimeDelta::FromSeconds(5);

  // Periodically check for a new config.
  if (!config_has_been_initialized() ||
      (TimeTicks::Now() - config_last_update_time_) > kProxyConfigMaxAge)
    UpdateConfig(net_log);
}


void ProxyService::OnIPAddressChanged() {
  DCHECK(network_change_notifier_);

  // Mark the current configuration as being un-initialized.
  //
  // This will force us to re-fetch the configuration (and re-run all of
  // the initialization steps) on the next ResolveProxy() request, as part
  // of UpdateConfigIfOld().
  config_.set_id(ProxyConfig::INVALID_ID);
}

SyncProxyServiceHelper::SyncProxyServiceHelper(MessageLoop* io_message_loop,
                                               ProxyService* proxy_service)
    : io_message_loop_(io_message_loop),
      proxy_service_(proxy_service),
      event_(false, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          this, &SyncProxyServiceHelper::OnCompletion)) {
  DCHECK(io_message_loop_ != MessageLoop::current());
}

int SyncProxyServiceHelper::ResolveProxy(const GURL& url,
                                         ProxyInfo* proxy_info,
                                         const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncResolve, url, net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

int SyncProxyServiceHelper::ReconsiderProxyAfterError(
    const GURL& url, ProxyInfo* proxy_info, const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncReconsider, url, net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

void SyncProxyServiceHelper::StartAsyncResolve(const GURL& url,
                                               const BoundNetLog& net_log) {
  result_ = proxy_service_->ResolveProxy(
      url, &proxy_info_, &callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::StartAsyncReconsider(const GURL& url,
                                                  const BoundNetLog& net_log) {
  result_ = proxy_service_->ReconsiderProxyAfterError(
      url, &proxy_info_, &callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::OnCompletion(int rv) {
  result_ = rv;
  event_.Signal();
}

}  // namespace net
