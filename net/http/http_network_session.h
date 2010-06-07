// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include <map>
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_client_auth_cache.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/http_auth_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_settings_storage.h"

namespace net {

class ClientSocketFactory;
class HttpAuthHandlerFactory;
class HttpNetworkSessionPeer;
class NetworkChangeNotifier;
class SpdySessionPool;

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession : public base::RefCounted<HttpNetworkSession> {
 public:
  HttpNetworkSession(
      NetworkChangeNotifier* network_change_notifier,
      HostResolver* host_resolver,
      ProxyService* proxy_service,
      ClientSocketFactory* client_socket_factory,
      SSLConfigService* ssl_config_service,
      SpdySessionPool* spdy_session_pool,
      HttpAuthHandlerFactory* http_auth_handler_factory,
      NetLog* net_log);

  HttpAuthCache* auth_cache() { return &auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  const HttpAlternateProtocols& alternate_protocols() const {
    return alternate_protocols_;
  }
  HttpAlternateProtocols* mutable_alternate_protocols() {
    return &alternate_protocols_;
  }

  // Access to the SpdySettingsStorage
  const SpdySettingsStorage& spdy_settings() const {
    return spdy_settings_;
  }
  SpdySettingsStorage* mutable_spdy_settings() {
    return &spdy_settings_;
  }

  // TCP sockets come from the tcp_socket_pool().
  const scoped_refptr<TCPClientSocketPool>& tcp_socket_pool() {
    return tcp_socket_pool_;
  }

  const scoped_refptr<SOCKSClientSocketPool>& GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy);

  const scoped_refptr<TCPClientSocketPool>& GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy);

  // SSL sockets come from the socket_factory().
  ClientSocketFactory* socket_factory() { return socket_factory_; }
  HostResolver* host_resolver() { return host_resolver_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  const scoped_refptr<SpdySessionPool>& spdy_session_pool() {
    return spdy_session_pool_;
  }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }

  static void set_max_sockets_per_group(int socket_count);

  static uint16 fixed_http_port();
  static void set_fixed_http_port(uint16 port);

  static uint16 fixed_https_port();
  static void set_fixed_https_port(uint16 port);

 private:
  typedef std::map<HostPortPair, scoped_refptr<TCPClientSocketPool> >
      HTTPProxySocketPoolMap;
  typedef std::map<HostPortPair, scoped_refptr<SOCKSClientSocketPool> >
      SOCKSSocketPoolMap;

  friend class base::RefCounted<HttpNetworkSession>;
  friend class HttpNetworkSessionPeer;

  ~HttpNetworkSession();

  HttpAuthCache auth_cache_;
  SSLClientAuthCache ssl_client_auth_cache_;
  HttpAlternateProtocols alternate_protocols_;
  NetworkChangeNotifier* const network_change_notifier_;
  scoped_refptr<ClientSocketPoolHistograms> tcp_pool_histograms_;
  scoped_refptr<ClientSocketPoolHistograms> http_proxy_pool_histograms_;
  scoped_refptr<ClientSocketPoolHistograms> tcp_for_socks_pool_histograms_;
  scoped_refptr<ClientSocketPoolHistograms> socks_pool_histograms_;
  scoped_refptr<TCPClientSocketPool> tcp_socket_pool_;
  HTTPProxySocketPoolMap http_proxy_socket_pool_;
  SOCKSSocketPoolMap socks_socket_pool_;
  ClientSocketFactory* socket_factory_;
  scoped_refptr<HostResolver> host_resolver_;
  scoped_refptr<ProxyService> proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<SpdySessionPool> spdy_session_pool_;
  HttpAuthHandlerFactory* http_auth_handler_factory_;
  NetLog* net_log_;
  SpdySettingsStorage spdy_settings_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
