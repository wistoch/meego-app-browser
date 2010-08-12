// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "base/scoped_temp_dir.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_authenticator2_unittest.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/signaling_task.h"
#include "chrome/test/test_notification_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

// TestNotificationTracker doesn't do a deep copy on the notification details.
// We have to in order to read it out, or we have a bad ptr, since the details
// are a reference on the stack.
class TokenAvailableTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenAvailableDetails& get_last_token_details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_AVAILABLE) {
      Details<const TokenService::TokenAvailableDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenAvailableDetails details_;
};

class TokenFailedTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenRequestFailedDetails& get_last_token_details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_REQUEST_FAILED) {
      Details<const TokenService::TokenRequestFailedDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenRequestFailedDetails details_;
};

class TokenServiceTest : public testing::Test {
 public:
  TokenServiceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
  }

  virtual void SetUp() {
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif
    credentials_.sid = "sid";
    credentials_.lsid = "lsid";
    credentials_.token = "token";
    credentials_.data = "data";

    ASSERT_TRUE(temp_.CreateUniqueTempDir());
    ASSERT_TRUE(db_thread_.Start());

    // Testing profile responsible for deleting the temp dir.
    profile_.reset(new TestingProfile(temp_.Take()));
    profile_->CreateWebDataService(false);
    WaitForDBLoadCompletion();

    success_tracker_.ListenFor(NotificationType::TOKEN_AVAILABLE,
                               Source<TokenService>(&service_));
    failure_tracker_.ListenFor(NotificationType::TOKEN_REQUEST_FAILED,
                               Source<TokenService>(&service_));

    service_.Initialize("test", profile_.get());
    service_.UpdateCredentials(credentials_);

    URLFetcher::set_factory(NULL);
  }

  virtual void TearDown() {
    // You have to destroy the profile before the db_thread_ stops.
    if (profile_.get()) {
      profile_.reset(NULL);
    }

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  void WaitForDBLoadCompletion() {
    // The WebDB does all work on the DB thread. This will add an event
    // to the end of the DB thread, so when we reach this task, all DB
    // operations should be complete.
    WaitableEvent done(false, false);
    ChromeThread::PostTask(
        ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
    done.Wait();

    // Notifications should be returned from the DB thread onto the UI thread.
    message_loop_.RunAllPending();
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;  // Mostly so DCHECKS pass.
  ChromeThread db_thread_;  // WDS on here

  TokenService service_;
  TokenAvailableTracker success_tracker_;
  TokenFailedTracker failure_tracker_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  scoped_ptr<TestingProfile> profile_;
  ScopedTempDir temp_;
};

TEST_F(TokenServiceTest, SanityCheck) {
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_EQ(service_.GetLsid(), "lsid");
  EXPECT_FALSE(service_.HasTokenForService("nonexistant service"));
}

TEST_F(TokenServiceTest, NoToken) {
  EXPECT_FALSE(service_.HasTokenForService("nonexistant service"));
  EXPECT_EQ(service_.GetTokenForService("nonexistant service"), std::string());
}

TEST_F(TokenServiceTest, NotificationSuccess) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());

  TokenService::TokenAvailableDetails details =
      success_tracker_.get_last_token_details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
}

TEST_F(TokenServiceTest, NotificationFailed) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::REQUEST_CANCELED;
  service_.OnIssueAuthTokenFailure(GaiaConstants::kSyncService, error);
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(1U, failure_tracker_.size());

  TokenService::TokenRequestFailedDetails details =
      failure_tracker_.get_last_token_details();

  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, OnTokenSuccessUpdate) {
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token2");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token2");

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "");
}

TEST_F(TokenServiceTest, OnTokenSuccess) {
  // Don't "start fetching", just go ahead and issue the callback.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");

  // Check the second service.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token2");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token2");

  // It didn't change.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
}

TEST_F(TokenServiceTest, ResetSimple) {
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasLsid());

  service_.ResetCredentialsInMemory();

  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasLsid());
}

TEST_F(TokenServiceTest, ResetComplex) {
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  service_.StartFetchingTokens();
  // You have to call delegates by hand with the test fetcher,
  // Let's pretend only one returned.

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "eraseme");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService),
            "eraseme");
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  service_.ResetCredentialsInMemory();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  // Now start using it again.
  service_.UpdateCredentials(credentials_);
  service_.StartFetchingTokens();

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token2");

  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token2");
}

TEST_F(TokenServiceTest, FullIntegration) {
  MockFactory<MockFetcher> factory;
  std::string result = "SID=sid\nLSID=lsid\nAuth=auth\n";
  factory.set_results(result);
  URLFetcher::set_factory(&factory);
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  service_.StartFetchingTokens();
  URLFetcher::set_factory(NULL);

  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kTalkService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), result);
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), result);

  service_.ResetCredentialsInMemory();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryBasic) {
  // Validate that the method sets proper data in notifications and map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(db_tokens.empty());
  EXPECT_TRUE(memory_tokens.empty());
  EXPECT_EQ(0U, success_tracker_.size());

  db_tokens[GaiaConstants::kSyncService] = "token";
  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_EQ(1U, success_tracker_.size());

  TokenService::TokenAvailableDetails details =
      success_tracker_.get_last_token_details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "token");
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryAdvanced) {
  // LoadTokensIntoMemory should avoid setting tokens already in the
  // token map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  db_tokens["ignore"] = "token";

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(memory_tokens.empty());
  db_tokens[GaiaConstants::kSyncService] = "pepper";

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
  EXPECT_EQ(1U, success_tracker_.size());
  success_tracker_.Reset();

  // SyncService token is already in memory. Pretend we got it off
  // the disk as well, but an older token.
  db_tokens[GaiaConstants::kSyncService] = "ignoreme";
  db_tokens[GaiaConstants::kTalkService] = "tomato";
  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);

  EXPECT_EQ(2U, memory_tokens.size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kTalkService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kTalkService], "tomato");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
}

TEST_F(TokenServiceTest, WebDBLoadIntegration) {
  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();
  EXPECT_EQ(0U, success_tracker_.size());

  // Should result in DB write.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker_.size());

  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  // Clean slate.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_FALSE(service_.HasLsid());
}

TEST_F(TokenServiceTest, MultipleLoadResetIntegration) {
  // Should result in DB write.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  service_.LoadTokensFromDB();  // Should do nothing.
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_FALSE(service_.HasLsid());

  // Reset it one more time so there's no surprises.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
}
