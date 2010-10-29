// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/common/net/url_fetcher_protect.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/service/service_process.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_unittest.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  explicit TestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy)
          : io_message_loop_proxy_(io_message_loop_proxy) {
  }
  virtual URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_;
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }

 protected:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

 private:
  ~TestURLRequestContextGetter() {}

  scoped_refptr<URLRequestContext> context_;
};

class TestCloudPrintURLFetcher : public CloudPrintURLFetcher {
 public:
  explicit TestCloudPrintURLFetcher(
      base::MessageLoopProxy* io_message_loop_proxy)
          : io_message_loop_proxy_(io_message_loop_proxy) {
  }

  virtual URLRequestContextGetter* GetRequestContextGetter() {
    return new TestURLRequestContextGetter(io_message_loop_proxy_.get());
  }
 private:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
};

class CloudPrintURLFetcherTest : public testing::Test,
                                 public CloudPrintURLFetcher::Delegate {
 public:
  CloudPrintURLFetcherTest() : fetcher_(NULL) { }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url, const std::string& retry_policy);

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const URLFetcher* source,
      const GURL& url,
      const URLRequestStatus& status,
      int response_code,
      const ResponseCookies& cookies,
      const std::string& data);

  virtual void OnRequestAuthError() {
    ADD_FAILURE();
  }

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_message_loop_proxy_;
  }

 protected:
  virtual void SetUp() {
    testing::Test::SetUp();

    io_message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  MessageLoopForIO io_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  std::string retry_policy_;
  Time start_time_;
  scoped_refptr<CloudPrintURLFetcher> fetcher_;
};

class CloudPrintURLFetcherBasicTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherBasicTest()
      : handle_raw_response_(false), handle_raw_data_(false) { }
  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const URLFetcher* source,
      const GURL& url,
      const URLRequestStatus& status,
      int response_code,
      const ResponseCookies& cookies,
      const std::string& data);

  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);

  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  void SetHandleRawResponse(bool handle_raw_response) {
    handle_raw_response_ = handle_raw_response;
  }
  void SetHandleRawData(bool handle_raw_data) {
    handle_raw_data_ = handle_raw_data;
  }
 private:
  bool handle_raw_response_;
  bool handle_raw_data_;
};

// Version of CloudPrintURLFetcherTest that tests overload protection.
class CloudPrintURLFetcherOverloadTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherOverloadTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);

 private:
  int response_count_;
};

// Version of CloudPrintURLFetcherTest that tests backoff protection.
class CloudPrintURLFetcherRetryBackoffTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherRetryBackoffTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);

  virtual void OnRequestGiveUp();

 private:
  int response_count_;
};


void CloudPrintURLFetcherTest::CreateFetcher(const GURL& url,
                                             const std::string& retry_policy) {
  fetcher_ = new TestCloudPrintURLFetcher(io_message_loop_proxy());
  retry_policy_ = retry_policy;
  start_time_ = Time::Now();
  fetcher_->StartGetRequest(url, this, "", retry_policy_);
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherTest::HandleRawResponse(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawResponse(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());

  fetcher_ = NULL;

  if (handle_raw_response_) {
    // If the current message loop is not the IO loop, it will be shut down when
    // the main loop returns and this thread subsequently goes out of scope.
    io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawData(
    const URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  // We should never get here if we returned true in HandleRawResponse
  EXPECT_FALSE(handle_raw_response_);
  if (handle_raw_data_) {
    io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleJSONData(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  // We should never get here if we returned true in one of the above methods.
  EXPECT_FALSE(handle_raw_response_);
  EXPECT_FALSE(handle_raw_data_);
  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherOverloadTest::HandleRawData(const URLFetcher* source,
                                                const GURL& url,
                                                const std::string& data) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  response_count_++;
  if (response_count_ < 20) {
    fetcher_->StartGetRequest(url, this, "", retry_policy_);
  } else {
    // We have already sent 20 requests continuously. And we expect that
    // it takes more than 1 second due to the overload pretection settings.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherRetryBackoffTest::HandleRawData(const URLFetcher* source,
                                                    const GURL& url,
                                                    const std::string& data) {
  response_count_++;
  // First attempt + 11 retries = 12 total responses.
  EXPECT_LE(response_count_, 12);
  return CloudPrintURLFetcher::RETRY_REQUEST;
}

void CloudPrintURLFetcherRetryBackoffTest::OnRequestGiveUp() {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  // It takes more than 1 second to finish all 11 requests.
  EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

TEST_F(CloudPrintURLFetcherBasicTest, HandleRawResponse) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  SetHandleRawResponse(true);

  CreateFetcher(test_server.GetURL("echo"), "DummyRetryPolicy");
  MessageLoop::current()->Run();
}

TEST_F(CloudPrintURLFetcherBasicTest, HandleRawData) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  SetHandleRawData(true);
  CreateFetcher(test_server.GetURL("echo"), "DummyRetryPolicy");
  MessageLoop::current()->Run();
}

TEST_F(CloudPrintURLFetcherOverloadTest, Protect) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  std::string retry_policy = "OverloadTestPolicy";
  URLFetcherProtectManager* manager = URLFetcherProtectManager::GetInstance();
  URLFetcherProtectEntry* entry =
      new URLFetcherProtectEntry(200, 3, 11, 1, 2.0, 0, 256);
  manager->Register(retry_policy, entry);

  CreateFetcher(url, retry_policy);

  MessageLoop::current()->Run();
}

TEST_F(CloudPrintURLFetcherRetryBackoffTest, GiveUp) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  std::string retry_policy = "BackoffTestPolicy";
  URLFetcherProtectManager* manager = URLFetcherProtectManager::GetInstance();
  URLFetcherProtectEntry* entry =
      new URLFetcherProtectEntry(200, 3, 11, 1, 2.0, 0, 256);
  manager->Register(retry_policy, entry);

  CreateFetcher(url, retry_policy);

  MessageLoop::current()->Run();
}

}  // namespace.
