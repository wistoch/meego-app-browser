// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

class HttpProxyClientSocketPoolTest : public ClientSocketPoolTest {
 protected:
  HttpProxyClientSocketPoolTest()
      : ignored_tcp_socket_params_(new TCPSocketParams(
            HostPortPair("proxy", 80), MEDIUM, GURL(), false)),
        tcp_histograms_(new ClientSocketPoolHistograms("MockTCP")),
        tcp_socket_pool_(new MockTCPClientSocketPool(kMaxSockets,
            kMaxSocketsPerGroup, tcp_histograms_, &tcp_client_socket_factory_)),
        http_auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        session_(new HttpNetworkSession(new MockHostResolver,
                                        ProxyService::CreateNull(),
                                        &socket_factory_,
                                        new SSLConfigServiceDefaults,
                                        new SpdySessionPool(),
                                        http_auth_handler_factory_.get(),
                                        NULL,
                                        NULL)),
        notunnel_socket_params_(new HttpProxySocketParams(
              ignored_tcp_socket_params_, GURL("http://host"),
              HostPortPair("host", 80), NULL, false)),
        tunnel_socket_params_(new HttpProxySocketParams(
              ignored_tcp_socket_params_, GURL("http://host"),
              HostPortPair("host", 80), session_, true)),
        http_proxy_histograms_(
            new ClientSocketPoolHistograms("HttpProxyUnitTest")),
        pool_(new HttpProxyClientSocketPool(kMaxSockets, kMaxSocketsPerGroup,
            http_proxy_histograms_, NULL, tcp_socket_pool_, NULL)) {
  }

  void AddAuthToCache() {
    const string16 kFoo(ASCIIToUTF16("foo"));
    const string16 kBar(ASCIIToUTF16("bar"));
    session_->auth_cache()->Add(GURL("http://proxy/"), "MyRealm1", "Basic",
                                "Basic realm=MyRealm1", kFoo, kBar, "/");
  }

  int StartRequest(const std::string& group_name, RequestPriority priority) {
    return StartRequestUsingPool(
        pool_, group_name, priority, tunnel_socket_params_);
  }

  scoped_refptr<TCPSocketParams> ignored_tcp_socket_params_;
  scoped_refptr<ClientSocketPoolHistograms> tcp_histograms_;
  MockClientSocketFactory tcp_client_socket_factory_;
  scoped_refptr<MockTCPClientSocketPool> tcp_socket_pool_;

  MockClientSocketFactory socket_factory_;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  scoped_refptr<HttpNetworkSession> session_;
  scoped_refptr<HttpProxySocketParams> notunnel_socket_params_;
  scoped_refptr<HttpProxySocketParams> tunnel_socket_params_;
  scoped_refptr<ClientSocketPoolHistograms> http_proxy_histograms_;
  scoped_refptr<HttpProxyClientSocketPool> pool_;
};

TEST_F(HttpProxyClientSocketPoolTest, NoTunnel) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(&data);

  ClientSocketHandle handle;
  int rv = handle.Init("a", notunnel_socket_params_, LOW, NULL, pool_,
                       BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_F(HttpProxyClientSocketPoolTest, NeedAuth) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
      // No credentials.
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle.socket());
  EXPECT_FALSE(tunnel_socket->IsConnected());
}

TEST_F(HttpProxyClientSocketPoolTest, HaveAuth) {
  MockWrite writes[] = {
      MockWrite(false,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(false, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(false, 0));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_F(HttpProxyClientSocketPoolTest, AsyncHaveAuth) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_F(HttpProxyClientSocketPoolTest, TCPError) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(true, ERR_CONNECTION_CLOSED));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(HttpProxyClientSocketPoolTest, TunnelUnexpectedClose) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Conn"),
      MockRead(true, ERR_CONNECTION_CLOSED),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(HttpProxyClientSocketPoolTest, TunnelSetupError) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 304 Not Modified\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));

  tcp_client_socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", tunnel_socket_params_, LOW, &callback, pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

// It would be nice to also test the timeouts in HttpProxyClientSocketPool.

}  // namespace

}  // namespace net
