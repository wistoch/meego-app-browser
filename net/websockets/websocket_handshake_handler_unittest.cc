// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_util.h"
#include "net/websockets/websocket_handshake_handler.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const char* const kCookieHeaders[] = {
  "cookie", "cookie2"
};

const char* const kSetCookieHeaders[] = {
  "set-cookie", "set-cookie2"
};

}

namespace net {

TEST(WebSocketHandshakeRequestHandlerTest, SimpleRequest) {
  WebSocketHandshakeRequestHandler handler;

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeRequestMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeRequestHandlerTest, ReplaceRequestCookies) {
  WebSocketHandshakeRequestHandler handler;

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-websocket-test=1\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  handler.AppendHeaderIfMissing("Cookie",
                                "WK-websocket-test=1; "
                                "WK-websocket-test-httponly=1");

  static const char* kHandshakeRequestExpectedMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-websocket-test=1; WK-websocket-test-httponly=1\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_EQ(kHandshakeRequestExpectedMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeResponseHandlerTest, SimpleResponse) {
  WebSocketHandshakeResponseHandler handler;

  static const char* kHandshakeResponseMessage =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_TRUE(handler.ParseRawResponse(kHandshakeResponseMessage,
                                       strlen(kHandshakeResponseMessage)));

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeResponseMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest, ReplaceResponseCookies) {
  WebSocketHandshakeResponseHandler handler;

  static const char* kHandshakeResponseMessage =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Set-Cookie: WK-websocket-test-1\r\n"
      "Set-Cookie: WK-websocket-test-httponly=1; HttpOnly\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_TRUE(handler.ParseRawResponse(kHandshakeResponseMessage,
                                       strlen(kHandshakeResponseMessage)));
  std::vector<std::string> cookies;
  handler.GetHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders), &cookies);
  ASSERT_EQ(2U, cookies.size());
  EXPECT_EQ("WK-websocket-test-1", cookies[0]);
  EXPECT_EQ("WK-websocket-test-httponly=1; HttpOnly", cookies[1]);
  handler.RemoveHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders));

  static const char* kHandshakeResponseExpectedMessage =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeHandlerTest, RequestResponse) {
  WebSocketHandshakeRequestHandler request_handler;

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));

  GURL url("ws://example.com/demo");
  std::string challenge;
  const HttpRequestInfo& request_info =
      request_handler.GetRequestInfo(url, &challenge);

  EXPECT_EQ(url, request_info.url);
  EXPECT_EQ("GET", request_info.method);
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Upgrade"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Connection"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Sec-WebSocket-Key1"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Sec-WebSocket-Key2"));
  std::string value;
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Host", &value));
  EXPECT_EQ("example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Origin", &value));
  EXPECT_EQ("http://example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Sec-WebSocket-Protocol",
                                                   &value));
  EXPECT_EQ("sample", value);

  const char expected_challenge[] = "\x31\x6e\x41\x13\x0f\x7e\xd6\x3c^n:ds[4U";

  EXPECT_EQ(expected_challenge, challenge);

  static const char* kHandshakeResponseHeader =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n";

  std::string raw_headers =
      HttpUtil::AssembleRawHeaders(kHandshakeResponseHeader,
                                   strlen(kHandshakeResponseHeader));
  HttpResponseInfo response_info;
  response_info.headers = new HttpResponseHeaders(raw_headers);

  EXPECT_TRUE(StartsWithASCII(response_info.headers->GetStatusLine(),
                              "HTTP/1.1 101 ", false));
  EXPECT_FALSE(response_info.headers->HasHeader("Upgrade"));
  EXPECT_FALSE(response_info.headers->HasHeader("Connection"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Origin",
                                                    "http://example.com"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Location",
                                                    "ws://example.com/demo"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Protocol",
                                                    "sample"));

  WebSocketHandshakeResponseHandler response_handler;
  EXPECT_TRUE(response_handler.ParseResponseInfo(response_info, challenge));
  EXPECT_TRUE(response_handler.HasResponse());

  static const char* kHandshakeResponseExpectedMessage =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

}  // namespace net
