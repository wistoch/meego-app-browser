// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for master preferences related methods.

#include "base/scoped_ptr.h"
#include "base/file_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MasterPreferencesTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }
};
}  // namespace

TEST(MasterPreferencesTest, ParseDistroParams) {
  std::wstring prefs_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs_file));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"show_welcome_page\": true,\n"
    "     \"import_search_engine\": true,\n"
    "     \"import_history\": true,\n"
    "     \"import_bookmarks\": true,\n"
    "     \"import_home_page\": true,\n"
    "     \"create_all_shortcuts\": true,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"make_chrome_default\": true,\n"
    "     \"make_chrome_default_for_user\": true,\n"
    "     \"system_level\": true,\n"
    "     \"verbose_logging\": true,\n"
    "     \"require_eula\": true,\n"
    "     \"alternate_shortcut_text\": true,\n"
    "     \"oem_bubble\": true,\n"
    "     \"chrome_shortcut_icon_index\": 1,\n"
    "     \"ping_delay\": 40\n"
    "  },\n"
    "  \"blah\": {\n"
    "     \"import_history\": false\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file, text, sizeof(text)));
  scoped_ptr<DictionaryValue> prefs(
      installer_util::ParseDistributionPreferences(
      FilePath::FromWStringHack(prefs_file)));
  EXPECT_TRUE(prefs.get() != NULL);
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroSkipFirstRunPref));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroShowWelcomePage));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportSearchPref));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportHistoryPref));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportBookmarksPref));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportHomePagePref));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kCreateAllShortcuts));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDoNotLaunchChrome));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kMakeChromeDefault));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kMakeChromeDefaultForUser));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kSystemLevel));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kVerboseLogging));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kRequireEula));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kAltShortcutText));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kAltFirstRunBubble));
  int icon_index = 0;
  EXPECT_TRUE(installer_util::GetDistroIntegerPreference(prefs.get(),
      installer_util::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 1);
  int ping_delay = 90;
  EXPECT_TRUE(installer_util::GetDistroIntegerPreference(prefs.get(),
      installer_util::master_preferences::kDistroPingDelay, &ping_delay));
  EXPECT_EQ(ping_delay, 40);
  EXPECT_TRUE(file_util::Delete(prefs_file, false));
}

TEST(MasterPreferencesTest, ParseMissingDistroParams) {
  std::wstring prefs_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs_file));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"import_search_engine\": true,\n"
    "     \"import_bookmarks\": false,\n"
    "     \"create_all_shortcuts\": true,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"chrome_shortcut_icon_index\": \"bac\"\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file, text, sizeof(text)));
  scoped_ptr<DictionaryValue> prefs(
      installer_util::ParseDistributionPreferences(
      FilePath::FromWStringHack(prefs_file)));
  EXPECT_TRUE(prefs.get() != NULL);
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroSkipFirstRunPref));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroShowWelcomePage));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportSearchPref));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportHistoryPref));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportBookmarksPref));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportHomePagePref));

  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kCreateAllShortcuts));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDoNotLaunchChrome));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kMakeChromeDefault));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kMakeChromeDefaultForUser));

  int icon_index = 0;
  EXPECT_FALSE(installer_util::GetDistroIntegerPreference(prefs.get(),
      installer_util::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 0);
  int ping_delay = 90;
  EXPECT_FALSE(installer_util::GetDistroIntegerPreference(prefs.get(),
      installer_util::master_preferences::kDistroPingDelay, &ping_delay));
  EXPECT_EQ(ping_delay, 90);
  EXPECT_TRUE(file_util::Delete(prefs_file, false));
}

TEST(MasterPreferencesTest, FirstRunTabs) {
  std::wstring prefs_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs_file));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"something here\": true\n"
    "  },\n"
    "  \"first_run_tabs\": [\n"
    "     \"http://google.com/f1\",\n"
    "     \"https://google.com/f2\",\n"
    "     \"new_tab_page\"\n"
    "  ]\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file, text, sizeof(text)));
  scoped_ptr<DictionaryValue> prefs(
      installer_util::ParseDistributionPreferences(
      FilePath::FromWStringHack(prefs_file)));
  EXPECT_TRUE(prefs.get() != NULL);

  typedef std::vector<std::wstring> TabsVector;
  TabsVector tabs = installer_util::GetFirstRunTabs(prefs.get());
  ASSERT_EQ(3, tabs.size());
  EXPECT_EQ(L"http://google.com/f1", tabs[0]);
  EXPECT_EQ(L"https://google.com/f2", tabs[1]);
  EXPECT_EQ(L"new_tab_page", tabs[2]);
  EXPECT_TRUE(file_util::Delete(prefs_file, false));
}
