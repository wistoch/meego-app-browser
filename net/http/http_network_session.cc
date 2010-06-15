// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/url_security_manager.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

namespace {

// Total limit of sockets.
int g_max_sockets = 256;

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
int g_max_sockets_per_group = 6;

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server = 32;

uint16 g_fixed_http_port = 0;
uint16 g_fixed_https_port = 0;

}  // namespace

HttpNetworkSession::HttpNetworkSession(
    NetworkChangeNotifier* network_change_notifier,
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    ClientSocketFactory* client_socket_factory,
    SSLConfigService* ssl_config_service,
    SpdySessionPool* spdy_session_pool,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    HttpNetworkDelegate* network_delegate,
    NetLog* net_log)
    : network_change_notifier_(network_change_notifier),
      // TODO(vandebo) when we've completely converted to pools, the base TCP
      // pool name should get changed to TCP instead of Transport.
      tcp_pool_histograms_(new ClientSocketPoolHistograms("Transport")),
      http_proxy_pool_histograms_(new ClientSocketPoolHistograms("HTTPProxy")),
      tcp_for_socks_pool_histograms_(
          new ClientSocketPoolHistograms("TCPforSOCKS")),
      socks_pool_histograms_(new ClientSocketPoolHistograms("SOCK")),
      tcp_socket_pool_(new TCPClientSocketPool(
          g_max_sockets, g_max_sockets_per_group, tcp_pool_histograms_,
          host_resolver, client_socket_factory, network_change_notifier_,
          net_log)),
      socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      spdy_session_pool_(spdy_session_pool),
      http_auth_handler_factory_(http_auth_handler_factory),
      network_delegate_(network_delegate),
      net_log_(net_log) {
  DCHECK(proxy_service);
  DCHECK(ssl_config_service);
}

HttpNetworkSession::~HttpNetworkSession() {
}

const scoped_refptr<TCPClientSocketPool>&
HttpNetworkSession::GetSocketPoolForHTTPProxy(const HostPortPair& http_proxy) {
  HTTPProxySocketPoolMap::const_iterator it =
      http_proxy_socket_pool_.find(http_proxy);
  if (it != http_proxy_socket_pool_.end())
    return it->second;

  std::pair<HTTPProxySocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pool_.insert(
          std::make_pair(
              http_proxy,
              new TCPClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  http_proxy_pool_histograms_, host_resolver_, socket_factory_,
                  network_change_notifier_,
                  net_log_)));

  return ret.first->second;
}

const scoped_refptr<SOCKSClientSocketPool>&
HttpNetworkSession::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  SOCKSSocketPoolMap::const_iterator it = socks_socket_pool_.find(socks_proxy);
  if (it != socks_socket_pool_.end())
    return it->second;

  std::pair<SOCKSSocketPoolMap::iterator, bool> ret =
      socks_socket_pool_.insert(
          std::make_pair(
              socks_proxy,
              new SOCKSClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  socks_pool_histograms_, host_resolver_,
                  new TCPClientSocketPool(g_max_sockets_per_proxy_server,
                                          g_max_sockets_per_group,
                                          tcp_for_socks_pool_histograms_,
                                          host_resolver_, socket_factory_,
                                          network_change_notifier_,
                                          net_log_),
                  network_change_notifier_, net_log_)));

  return ret.first->second;
}

// static
void HttpNetworkSession::set_max_sockets_per_group(int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  g_max_sockets_per_group = socket_count;
}

// static
uint16 HttpNetworkSession::fixed_http_port() {
  return g_fixed_http_port;
}

// static
void HttpNetworkSession::set_fixed_http_port(uint16 port) {
  g_fixed_http_port = port;
}

// static
uint16 HttpNetworkSession::fixed_https_port() {
  return g_fixed_https_port;
}

// static
void HttpNetworkSession::set_fixed_https_port(uint16 port) {
  g_fixed_https_port = port;
}

}  //  namespace net
