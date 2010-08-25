// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_

#include "chrome/browser/chromeos/login/owner_manager.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace chromeos {
class MockKeyLoadObserver : public NotificationObserver {
 public:
  MockKeyLoadObserver()
      : success_expected_(false),
        quit_on_observe_(true),
        observed_(false) {
    registrar_.Add(
        this,
        NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED,
        NotificationService::AllSources());
    registrar_.Add(
        this,
        NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
        NotificationService::AllSources());
  }

  virtual ~MockKeyLoadObserver() {
    EXPECT_TRUE(observed_);
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    LOG(INFO) << "Observed key fetch event";
    if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
      EXPECT_TRUE(success_expected_);
      observed_ = true;
      if (quit_on_observe_)
        MessageLoop::current()->Quit();
    } else if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED) {
      EXPECT_FALSE(success_expected_);
      observed_ = true;
      if (quit_on_observe_)
        MessageLoop::current()->Quit();
    }
  }

  void ExpectKeyFetchSuccess(bool should_succeed) {
    success_expected_ = should_succeed;
  }

  void SetQuitOnKeyFetch(bool should_quit) { quit_on_observe_ = should_quit; }

 private:
  NotificationRegistrar registrar_;
  bool success_expected_;
  bool quit_on_observe_;
  bool observed_;
  DISALLOW_COPY_AND_ASSIGN(MockKeyLoadObserver);
};

class MockKeyUser : public OwnerManager::Delegate {
 public:
  explicit MockKeyUser(const OwnerManager::KeyOpCode expected)
      : expected_(expected) {
  }

  virtual ~MockKeyUser() {}

  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::string& payload) {
    MessageLoop::current()->Quit();
    EXPECT_EQ(expected_, return_code);
  }

  const OwnerManager::KeyOpCode expected_;
 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyUser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_
