// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class GoogleUpdateTest : public PlatformTest {
};

// bug: http://crbug.com/70092
TEST_F(GoogleUpdateTest, DISABLED_StatsConstent) {
  // Stats are off by default.
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is ON.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(true));
  EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is OFF.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(false));
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
}

#if defined(OS_WIN)

TEST_F(GoogleUpdateTest, LastRunTime) {
  // Querying the value that does not exists should fail.
  EXPECT_TRUE(GoogleUpdateSettings::RemoveLastRunTime());
  EXPECT_EQ(-1, GoogleUpdateSettings::GetLastRunTime());
  // Setting and querying the last update time in fast sequence
  // should give 0 days.
  EXPECT_TRUE(GoogleUpdateSettings::SetLastRunTime());
  EXPECT_EQ(0, GoogleUpdateSettings::GetLastRunTime());
}

#endif  // defined(OS_WIN)
