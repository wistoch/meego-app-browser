// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_factory.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"
#include "net/base/ssl_test_util.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

const net::SSLConfig kDefaultSSLConfig;

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
  }

  void StartOKServer() {
    bool success = server_.Start(net::TestServerLauncher::ProtoHTTP,
        server_.kHostName, server_.kOKHTTPSPort,
        FilePath(), server_.GetOKCertPath());
    ASSERT_TRUE(success);
  }

  void StartMismatchedServer() {
    bool success = server_.Start(net::TestServerLauncher::ProtoHTTP,
        server_.kMismatchedHostName, server_.kOKHTTPSPort,
        FilePath(), server_.GetOKCertPath());
    ASSERT_TRUE(success);
  }

  void StartExpiredServer() {
    bool success = server_.Start(net::TestServerLauncher::ProtoHTTP,
        server_.kHostName, server_.kBadHTTPSPort,
        FilePath(), server_.GetExpiredCertPath());
    ASSERT_TRUE(success);
  }

 protected:
  net::ClientSocketFactory* socket_factory_;
  net::TestServerLauncher server_;
};

//-----------------------------------------------------------------------------

#if defined(OS_MACOSX)
#define MAYBE_Connect DISABLED_Connect
#define MAYBE_ConnectExpired DISABLED_ConnectExpired
#define MAYBE_ConnectMismatched DISABLED_ConnectMismatched
#define MAYBE_Read DISABLED_Read
#define MAYBE_Read_SmallChunks DISABLED_Read_SmallChunks
#define MAYBE_Read_Interrupted DISABLED_Read_Interrupted
#else
#define MAYBE_Connect Connect
#define MAYBE_ConnectExpired ConnectExpired
#define MAYBE_ConnectMismatched ConnectMismatched
#define MAYBE_Read Read
#define MAYBE_Read_SmallChunks Read_SmallChunks
#define MAYBE_Read_Interrupted Read_Interrupted
#endif

TEST_F(SSLClientSocketTest, MAYBE_Connect) {
  StartOKServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kHostName, server_.kOKHTTPSPort,
                            &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
          server_.kHostName, kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  EXPECT_TRUE(sock->IsConnected());

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, MAYBE_ConnectExpired) {
  StartExpiredServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kHostName, server_.kBadHTTPSPort,
                            &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
          server_.kHostName, kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());

    rv = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_DATE_INVALID, rv);
  }

  EXPECT_TRUE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, MAYBE_ConnectMismatched) {
  StartMismatchedServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kMismatchedHostName, server_.kOKHTTPSPort,
                            &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
          server_.kMismatchedHostName, kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  if (rv != net::ERR_CERT_COMMON_NAME_INVALID) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());

    rv = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, rv);
  }

  // The Windows code happens to keep the connection
  // open now in spite of an error.  The designers of
  // this API intended to also allow the connection
  // to be closed on error, in which case the caller
  // should call ReconnectIgnoringLastError, but
  // that's currently unimplemented.
  EXPECT_TRUE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, MAYBE_Read) {
  StartOKServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kHostName, server_.kOKHTTPSPort,
                            &addr, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             server_.kHostName,
                                             kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
  }

  char buf[4096];
  for (;;) {
    rv = sock->Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, MAYBE_Read_SmallChunks) {
  StartOKServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kHostName, server_.kOKHTTPSPort,
                            &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
          server_.kHostName, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
  }

  char buf[1];
  for (;;) {
    rv = sock->Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, MAYBE_Read_Interrupted) {
  StartOKServer();

  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve(server_.kHostName, server_.kOKHTTPSPort,
                            &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
          server_.kHostName, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
  }

  // Do a partial read and then exit.  This test should not crash!
  char buf[512];
  rv = sock->Read(buf, sizeof(buf), &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(rv, 0);
}

