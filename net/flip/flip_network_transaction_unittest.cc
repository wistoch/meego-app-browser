// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>  // ceil

#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/ssl_info.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/flip/flip_network_transaction.h"
#include "net/flip/flip_protocol.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace flip {

using namespace net;

// Create a proxy service which fails on all requests (falls back to direct).
ProxyService* CreateNullProxyService() {
  return ProxyService::CreateNull();
}

// Helper to manage the lifetimes of the dependencies for a
// HttpNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(CreateNullProxyService()),
        ssl_config_service(new SSLConfigServiceDefaults) {}

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
};

ProxyService* CreateFixedProxyService(const std::string& proxy) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules.ParseFromString(proxy);
  return ProxyService::CreateFixed(proxy_config);
}


HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service);
}

class FlipNetworkTransactionTest : public PlatformTest {
 public:
  virtual void SetUp() {
    // Disable compression on this test.
    FlipFramer::set_enable_compression_default(false);
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

 protected:
  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);

  struct SimpleGetHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
  };

  SimpleGetHelperResult SimpleGetHelper(MockRead data_reads[]) {
    SimpleGetHelperResult out;

    SessionDependencies session_deps;
    scoped_ptr<FlipNetworkTransaction> trans(
        new FlipNetworkTransaction(
            CreateSession(&session_deps)));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    StaticMockSocket data(data_reads, NULL);
    session_deps.socket_factory.AddMockSocket(&data);

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback, NULL);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    out.status_line = response->headers->GetStatusLine();

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    return out;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);
};

//-----------------------------------------------------------------------------

// Verify FlipNetworkTransaction constructor.
TEST_F(FlipNetworkTransactionTest, Constructor) {
  SessionDependencies session_deps;
  scoped_refptr<net::HttpNetworkSession> session =
      CreateSession(&session_deps);
  scoped_ptr<HttpTransaction> trans(new FlipNetworkTransaction(session));
}

TEST_F(FlipNetworkTransactionTest, Connect) {
  unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,                                        // header
    0x00, 0x00, 0x00, 0x45,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,                                        // 4 headers
    0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
    0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
    0x00, 0x03, '2', '0', '0',                                     // "200"
    0x00, 0x03, 'u', 'r', 'l',                                     // "url"
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "HTTP/1.1"
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
  };
  unsigned char body_frame[] = {
    0x00, 0x00, 0x00, 0x01,                                        // header
    0x00, 0x00, 0x00, 0x06,
    'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
  };
  unsigned char fin_frame[] = {
    0x80, 0x01, 0x00, 0x03,                                        // header
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  MockRead data_reads[] = {
    MockRead(true, reinterpret_cast<char*>(syn_reply), sizeof(syn_reply)),
    MockRead(true, reinterpret_cast<char*>(body_frame), sizeof(body_frame)),
    MockRead(true, reinterpret_cast<char*>(fin_frame), sizeof(fin_frame)),
    MockRead(true, 0, 0),  // EOF
  };

  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

}  // namespace net

