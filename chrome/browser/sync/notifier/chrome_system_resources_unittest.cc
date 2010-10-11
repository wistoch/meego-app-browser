// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <string>

#include "base/message_loop.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

using ::testing::_;

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

void ShouldNotRun() {
  ADD_FAILURE();
}

class ChromeSystemResourcesTest : public testing::Test {
 public:
  // Used as a callback.
  void IncrementCounter() {
    ++counter_;
  }

  // Used as a callback.
  void ExpectTrue(bool result) {
    EXPECT_TRUE(result);
  }

 protected:
  ChromeSystemResourcesTest() :
      chrome_system_resources_(&mock_state_writer_),
      counter_(0) {}

  virtual ~ChromeSystemResourcesTest() {}

  void ScheduleShouldNotRun() {
    chrome_system_resources_.ScheduleImmediately(
        invalidation::NewPermanentCallback(&ShouldNotRun));
    chrome_system_resources_.ScheduleWithDelay(
        invalidation::TimeDelta::FromSeconds(0),
        invalidation::NewPermanentCallback(&ShouldNotRun));
  }

  // Needed by |chrome_system_resources_|.
  MessageLoop message_loop_;
  MockStateWriter mock_state_writer_;
  ChromeSystemResources chrome_system_resources_;
  int counter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSystemResourcesTest);
};

// Make sure current_time() doesn't crash or leak.
TEST_F(ChromeSystemResourcesTest, CurrentTime) {
  invalidation::Time current_time =
      chrome_system_resources_.current_time();
  LOG(INFO) << "current_time returned: " << current_time.ToInternalValue();
}

// Make sure Log() doesn't crash or leak.
TEST_F(ChromeSystemResourcesTest, Log) {
  chrome_system_resources_.Log(ChromeSystemResources::INFO_LEVEL,
                               __FILE__, __LINE__, "%s %d",
                               "test string", 5);
}

TEST_F(ChromeSystemResourcesTest, ScheduleBeforeStart) {
  ScheduleShouldNotRun();
  chrome_system_resources_.StartScheduler();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAfterStop) {
  chrome_system_resources_.StartScheduler();
  chrome_system_resources_.StopScheduler();
  ScheduleShouldNotRun();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAndStop) {
  chrome_system_resources_.StartScheduler();
  ScheduleShouldNotRun();
  chrome_system_resources_.StopScheduler();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAndDestroy) {
  chrome_system_resources_.StartScheduler();
  ScheduleShouldNotRun();
}

TEST_F(ChromeSystemResourcesTest, ScheduleImmediately) {
  chrome_system_resources_.StartScheduler();
  chrome_system_resources_.ScheduleImmediately(
      invalidation::NewPermanentCallback(
          this, &ChromeSystemResourcesTest::IncrementCounter));
  EXPECT_EQ(0, counter_);
  message_loop_.RunAllPending();
  EXPECT_EQ(1, counter_);
}

TEST_F(ChromeSystemResourcesTest, ScheduleOnListenerThread) {
  chrome_system_resources_.StartScheduler();
  chrome_system_resources_.ScheduleOnListenerThread(
      invalidation::NewPermanentCallback(
          this, &ChromeSystemResourcesTest::IncrementCounter));
  EXPECT_FALSE(chrome_system_resources_.IsRunningOnInternalThread());
  EXPECT_EQ(0, counter_);
  message_loop_.RunAllPending();
  EXPECT_EQ(1, counter_);
}

TEST_F(ChromeSystemResourcesTest, ScheduleWithZeroDelay) {
  chrome_system_resources_.StartScheduler();
  chrome_system_resources_.ScheduleWithDelay(
      invalidation::TimeDelta::FromSeconds(0),
      invalidation::NewPermanentCallback(
          this, &ChromeSystemResourcesTest::IncrementCounter));
  EXPECT_EQ(0, counter_);
  message_loop_.RunAllPending();
  EXPECT_EQ(1, counter_);
}

// TODO(akalin): Figure out how to test with a non-zero delay.

TEST_F(ChromeSystemResourcesTest, WriteState) {
  EXPECT_CALL(mock_state_writer_, WriteState(_));

  // Explicitness hack here to work around broken callback
  // implementations.
  ChromeSystemResourcesTest* run_object = this;
  void (ChromeSystemResourcesTest::*run_function)(bool) =
      &ChromeSystemResourcesTest::ExpectTrue;

  chrome_system_resources_.WriteState(
      "state", invalidation::NewPermanentCallback(run_object, run_function));
}

}  // namespace
}  // namespace notifier
