// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/socket/client_socket_factory.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_network_transaction.h"
#include "net/spdy/spdy_session.h"

namespace net {

//-----------------------------------------------------------------------------

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    NetworkChangeNotifier* network_change_notifier,
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service,
    HttpAuthHandlerFactory* http_auth_handler_factory) {
  DCHECK(proxy_service);

  return new HttpNetworkLayer(ClientSocketFactory::GetDefaultFactory(),
                              network_change_notifier,
                              host_resolver, proxy_service, ssl_config_service,
                              http_auth_handler_factory);
}

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HttpNetworkSession* session) {
  DCHECK(session);

  return new HttpNetworkLayer(session);
}

//-----------------------------------------------------------------------------
bool HttpNetworkLayer::force_spdy_ = false;

HttpNetworkLayer::HttpNetworkLayer(
    ClientSocketFactory* socket_factory,
    NetworkChangeNotifier* network_change_notifier,
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service,
    HttpAuthHandlerFactory* http_auth_handler_factory)
    : socket_factory_(socket_factory),
      network_change_notifier_(network_change_notifier),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      session_(NULL),
      http_auth_handler_factory_(http_auth_handler_factory),
      suspended_(false) {
  DCHECK(proxy_service_);
  DCHECK(ssl_config_service_.get());
}

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
      network_change_notifier_(NULL),
      ssl_config_service_(NULL),
      session_(session),
      http_auth_handler_factory_(NULL),
      suspended_(false) {
  DCHECK(session_.get());
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

int HttpNetworkLayer::CreateTransaction(scoped_ptr<HttpTransaction>* trans) {
  if (suspended_)
    return ERR_NETWORK_IO_SUSPENDED;

  if (force_spdy_)
    trans->reset(new SpdyNetworkTransaction(GetSession()));
  else
    trans->reset(new HttpNetworkTransaction(GetSession()));
  return OK;
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend && session_)
    session_->Flush();
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  if (!session_) {
    DCHECK(proxy_service_);
    session_ = new HttpNetworkSession(
        network_change_notifier_, host_resolver_, proxy_service_,
        socket_factory_, ssl_config_service_,
        http_auth_handler_factory_);
    // These were just temps for lazy-initializing HttpNetworkSession.
    network_change_notifier_ = NULL;
    host_resolver_ = NULL;
    proxy_service_ = NULL;
    socket_factory_ = NULL;
    http_auth_handler_factory_ = NULL;
  }
  return session_;
}

// static
void HttpNetworkLayer::EnableSpdy(const std::string& mode) {
  static const char kDisableSSL[] = "no-ssl";
  static const char kDisableCompression[] = "no-compress";
  static const char kEnableNPN[] = "npn";

  std::vector<std::string> spdy_options;
  SplitString(mode, ',', &spdy_options);

  // Force spdy mode (use SpdyNetworkTransaction for all http requests).
  force_spdy_ = true;

  for (std::vector<std::string>::iterator it = spdy_options.begin();
       it != spdy_options.end(); ++it) {
    const std::string& option = *it;

    // Disable SSL
    if (option == kDisableSSL) {
      SpdySession::SetSSLMode(false);
    } else if (option == kDisableCompression) {
      spdy::SpdyFramer::set_enable_compression_default(false);
    } else if (option == kEnableNPN) {
      // Except for the first element, the order is irrelevant.  First element
      // specifies the fallback in case nothing matches
      // (SSLClientSocket::kNextProtoNoOverlap).  Otherwise, the SSL library
      // will choose the first overlapping protocol in the server's list, since
      // it presumedly has a better understanding of which protocol we should
      // use, therefore the rest of the ordering here is not important.
      HttpNetworkTransaction::SetNextProtos(
          "\008http/1.1\007http1.1\006spdy/1\004spdy");
      force_spdy_ = false;
    } else if (option.empty() && it == spdy_options.begin()) {
      continue;
    } else {
      LOG(DFATAL) << "Unrecognized spdy option: " << option;
    }
  }
}

}  // namespace net
