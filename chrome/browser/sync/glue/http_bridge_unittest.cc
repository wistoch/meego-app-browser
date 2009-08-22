// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "base/thread.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::HttpBridge;

namespace {
// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const char16 kDocRoot[] = L"chrome/test/data";
}

class HttpBridgeTest : public testing::Test {
 public:
  HttpBridgeTest() : io_thread_("HttpBridgeTest IO thread"),
      fake_default_request_context_(NULL) {
  }

  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  virtual void TearDown() {
    io_thread_loop()->ReleaseSoon(FROM_HERE, fake_default_request_context_);
    io_thread_.Stop();
    fake_default_request_context_ = NULL;
  }

  HttpBridge* BuildBridge() {
    if (!fake_default_request_context_) {
      fake_default_request_context_ = new TestURLRequestContext();
      fake_default_request_context_->AddRef();
    }
    HttpBridge* bridge = new HttpBridge(
        new HttpBridge::RequestContext(fake_default_request_context_),
        io_thread_.message_loop());
    bridge->use_io_loop_for_testing_ = true;
    return bridge;
  }

  MessageLoop* io_thread_loop() { return io_thread_.message_loop(); }
 private:
  // A make-believe "default" request context, as would be returned by
  // Profile::GetDefaultRequestContext().  Created lazily by BuildBridge.
  TestURLRequestContext* fake_default_request_context_;

  // Separate thread for IO used by the HttpBridge.
  base::Thread io_thread_;
};

// An HttpBridge that doesn't actually make network requests and just calls
// back with dummy response info.
class ShuntedHttpBridge : public HttpBridge {
 public:
  ShuntedHttpBridge(const URLRequestContext* baseline_context,
                    MessageLoop* io_loop, HttpBridgeTest* test)
      : HttpBridge(new HttpBridge::RequestContext(baseline_context),
                   io_loop), test_(test) { }
 protected:
  virtual void MakeAsynchronousPost() {
    ASSERT_TRUE(MessageLoop::current() == test_->io_thread_loop());
    // We don't actually want to make a request for this test, so just callback
    // as if it completed.
    test_->io_thread_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ShuntedHttpBridge::CallOnURLFetchComplete));
  }
 private:
  void CallOnURLFetchComplete() {
    ASSERT_TRUE(MessageLoop::current() == test_->io_thread_loop());
    // We return no cookies and a dummy content response.
    ResponseCookies cookies;

    std::string response_content = "success!";
    OnURLFetchComplete(NULL, GURL("www.google.com"), URLRequestStatus(),
                       200, cookies, response_content);
  }
  HttpBridgeTest* test_;
};

// Test the HttpBridge without actually making any network requests.
TEST_F(HttpBridgeTest, TestMakeSynchronousPostShunted) {
  scoped_refptr<TestURLRequestContext> ctx(new TestURLRequestContext());
  scoped_refptr<HttpBridge> http_bridge(new ShuntedHttpBridge(
      ctx, io_thread_loop(), this));
  http_bridge->SetUserAgent("bob");
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);
  EXPECT_EQ(0, http_bridge->GetResponseCookieCount());

  EXPECT_EQ(8, http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string("success!"),
            std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge, using default UA string and
// no request cookies.
TEST_F(HttpBridgeTest, TestMakeSynchronousPostLiveWithPayload) {
  scoped_refptr<HTTPTestServer> server = HTTPTestServer::CreateServer(kDocRoot,
                                                                      NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload = "this should be echoed back";
  GURL echo = server->TestServerPage("echo");
  http_bridge->SetURL(echo.spec().c_str(), echo.IntPort());
  http_bridge->SetPostPayload("application/x-www-form-urlencoded",
                              payload.length() + 1, payload.c_str());
  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);
  EXPECT_EQ(0, http_bridge->GetResponseCookieCount());
  EXPECT_EQ(payload.length() + 1, http_bridge->GetResponseContentLength());
  EXPECT_EQ(payload, std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge, using custom UA string and
// multiple request cookies. Cookies should not come back.
TEST_F(HttpBridgeTest, TestMakeSynchronousPostLiveComprehensive) {
  scoped_refptr<HTTPTestServer> server = HTTPTestServer::CreateServer(kDocRoot,
                                                                      NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = server->TestServerPage("echoall");
  http_bridge->SetUserAgent("bob");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());
  http_bridge->AddCookieForRequest("foo=bar");
  http_bridge->AddCookieForRequest("baz=boo");
  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);
  EXPECT_EQ(0, http_bridge->GetResponseCookieCount());
  std::string response = http_bridge->GetResponseContent();

  EXPECT_EQ(std::string::npos, response.find("Cookie:"));
  // TODO(chron): Renable this check and figure out why headers
  // aren't echoing right.
  // This is currently broken in header parsing from the echo
  // server for some reason.
  // EXPECT_NE(std::string::npos, response.find("User-Agent: bob"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

// TODO(chron): Renable this check and figure out why headers
// aren't echoing right.
// Test over the wire whether our extra request headers come back.
// Use default UA string and a test payload.
TEST_F(HttpBridgeTest, DISABLED_TestExtraRequestHeaders) {
  scoped_refptr<HTTPTestServer> server = HTTPTestServer::CreateServer(kDocRoot,
                                                                      NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = server->TestServerPage("echoall");

  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());
  http_bridge->SetExtraRequestHeaders("test:fnord");

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);
  EXPECT_EQ(0, http_bridge->GetResponseCookieCount());
  std::string response = http_bridge->GetResponseContent();

  EXPECT_NE(std::string::npos, response.find("fnord"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

#endif  // CHROME_PERSONALIZATION
