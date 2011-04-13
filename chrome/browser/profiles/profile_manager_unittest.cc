// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/system_monitor/system_monitor.h"

class ProfileManagerTest : public testing::Test {
 protected:
  ProfileManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create a local_state PrefService.
    browser::RegisterLocalState(&test_local_state_);
    TestingBrowserProcess* testing_browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    testing_browser_process->SetPrefService(&test_local_state_);
  }

  virtual void TearDown() {
    TestingBrowserProcess* testing_browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    testing_browser_process->SetPrefService(NULL);
  }

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  TestingPrefService test_local_state_;
};

TEST_F(ProfileManagerTest, CreateProfile) {
  FilePath source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("profiles"));
  source_path = source_path.Append(FILE_PATH_LITERAL("sample"));

  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  scoped_ptr<Profile> profile;

  // Successfully create a profile.
  profile.reset(ProfileManager::CreateProfile(dest_path));
  ASSERT_TRUE(profile.get());

  profile.reset();

#ifdef NDEBUG
  // In Release mode, we always try to always return a profile.  In debug,
  // these cases would trigger DCHECKs.

  // The profile already exists when we call CreateProfile.  Just load it.
  profile.reset(ProfileManager::CreateProfile(dest_path));
  ASSERT_TRUE(profile.get());
#endif
}

TEST_F(ProfileManagerTest, DefaultProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  ui::SystemMonitor dummy;
  ProfileManager profile_manager;
  std::string profile_dir("my_user");

  cl->AppendSwitch(switches::kTestType);

  FilePath expected_default =
      FilePath().AppendASCII(chrome::kNotSignedInProfile);
  EXPECT_EQ(expected_default.value(),
            profile_manager.GetCurrentProfileDir().value());
}

#if defined(OS_CHROMEOS)
// This functionality only exists on Chrome OS.
TEST_F(ProfileManagerTest, LoggedInProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  ui::SystemMonitor dummy;
  ProfileManager profile_manager;
  std::string profile_dir("my_user");

  cl->AppendSwitchASCII(switches::kLoginProfile, profile_dir);
  cl->AppendSwitch(switches::kTestType);

  FilePath expected_default =
      FilePath().AppendASCII(chrome::kNotSignedInProfile);
  EXPECT_EQ(expected_default.value(),
            profile_manager.GetCurrentProfileDir().value());

  profile_manager.Observe(NotificationType::LOGIN_USER_CHANGED,
                          NotificationService::AllSources(),
                          NotificationService::NoDetails());
  FilePath expected_logged_in(profile_dir);
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager.GetCurrentProfileDir().value());
  VLOG(1) << temp_dir_.path().Append(
      profile_manager.GetCurrentProfileDir()).value();
}

#endif

TEST_F(ProfileManagerTest, CreateAndUseTwoProfiles) {
  FilePath source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("profiles"));
  source_path = source_path.Append(FILE_PATH_LITERAL("sample"));

  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  scoped_ptr<Profile> profile1;
  scoped_ptr<Profile> profile2;

  // Successfully create the profiles.
  profile1.reset(ProfileManager::CreateProfile(dest_path1));
  ASSERT_TRUE(profile1.get());

  profile2.reset(ProfileManager::CreateProfile(dest_path2));
  ASSERT_TRUE(profile2.get());

  // Force lazy-init of some profile services to simulate use.
  EXPECT_TRUE(profile1->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_TRUE(profile1->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetHistoryService(Profile::EXPLICIT_ACCESS));

  // Make sure any pending tasks run before we destroy the profiles.
  message_loop_.RunAllPending();

  profile1.reset();
  profile2.reset();

  // Make sure history cleans up correctly.
  message_loop_.RunAllPending();
}
