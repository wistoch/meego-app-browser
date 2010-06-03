// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_request_context.h"

#include "build/build_config.h"

#include "base/file_path.h"
#if defined(OS_WIN)
#include "base/nss_util.h"
#endif
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/static_cookie_policy.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#if defined(OS_WIN)
#include "net/socket/ssl_client_socket_nss_factory.h"
#endif
#include "webkit/glue/webkit_glue.h"

TestShellRequestContext::TestShellRequestContext() : cache_thread_("cache") {
  Init(FilePath(), net::HttpCache::NORMAL, false);
}

TestShellRequestContext::TestShellRequestContext(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) : cache_thread_("cache") {
  Init(cache_path, cache_mode, no_proxy);
}

void TestShellRequestContext::Init(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  cookie_policy_ = new net::StaticCookiePolicy();

  // hard-code A-L and A-C for test shells
  accept_language_ = "en-us,en";
  accept_charset_ = "iso-8859-1,*,utf-8";

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Use no proxy to avoid ProxyConfigServiceLinux.
  // Enabling use of the ProxyConfigServiceLinux requires:
  // -Calling from a thread with a TYPE_UI MessageLoop,
  // -If at all possible, passing in a pointer to the IO thread's MessageLoop,
  // -Keep in mind that proxy auto configuration is also
  //  non-functional on linux in this context because of v8 threading
  //  issues.
  // TODO(port): rename "linux" to some nonspecific unix.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));
#else
  // Use the system proxy settings.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      net::ProxyService::CreateSystemProxyConfigService(NULL, NULL));
#endif
  host_resolver_ = net::CreateSystemHostResolver(NULL);
  proxy_service_ = net::ProxyService::Create(proxy_config_service.release(),
                                             false, NULL, NULL, NULL, NULL);
  ssl_config_service_ = net::SSLConfigService::CreateSystemSSLConfigService();

  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault();

#if defined(OS_WIN)
  // Use NSS for SSL on Windows.  TODO(wtc): this should eventually be hidden
  // inside DefaultClientSocketFactory::CreateSSLClientSocket.
  net::ClientSocketFactory::SetSSLClientSocketFactory(
      net::SSLClientSocketNSSFactory);
  // We want to be sure to init NSPR on the main thread.
  base::EnsureNSPRInit();
#endif

  if (!cache_path.empty())
    CHECK(cache_thread_.StartWithOptions(
              base::Thread::Options(MessageLoop::TYPE_IO, 0)));

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
      cache_path, 0, cache_thread_.message_loop_proxy());

  net::HttpCache* cache =
      new net::HttpCache(NULL, host_resolver_, proxy_service_,
                         ssl_config_service_, http_auth_handler_factory_,
                         NULL, backend);

  cache->set_mode(cache_mode);
  http_transaction_factory_ = cache;

  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
}

TestShellRequestContext::~TestShellRequestContext() {
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
  delete static_cast<net::StaticCookiePolicy*>(cookie_policy_);
}

const std::string& TestShellRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}
