// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include "base/file_util.h"
#include "base/test_file_util.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/browser/sync/util/query_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::UserSettings;

static const wchar_t* kV10UserSettingsDB = L"Version10Settings.sqlite3";
static const wchar_t* kOldStyleSyncDataDB = L"OldStyleSyncData.sqlite3";

class UserSettingsTest : public testing::Test {
 public:
  UserSettingsTest() : sync_data_("Some sync data") { }
  void SetUpVersion10Databases() {
    CleanUpVersion10Databases();
    sqlite3* primer_handle = NULL;
    ASSERT_EQ(SQLITE_OK, SqliteOpen(kV10UserSettingsDB,
        &primer_handle));
    FilePath old_sync_data(kOldStyleSyncDataDB);

    ASSERT_EQ(sync_data_.length(), file_util::WriteFile(
        old_sync_data, sync_data_.data(), sync_data_.length()));

    // Create settings table.
    ExecOrDie(primer_handle, "CREATE TABLE settings"
              " (email, key, value, "
              "  PRIMARY KEY(email, key) ON CONFLICT REPLACE)");

    // Create and populate version table.
    ExecOrDie(primer_handle, "CREATE TABLE db_version ( version )");
    ExecOrDie(primer_handle, "INSERT INTO db_version values ( ? )", 10);
    // Create shares table.
    ExecOrDie(primer_handle, "CREATE TABLE shares"
              " (email, share_name, file_name,"
              "  PRIMARY KEY(email, share_name) ON CONFLICT REPLACE)");
    // Populate a share.
    ExecOrDie(primer_handle, "INSERT INTO shares values ( ?, ?, ?)",
              "foo@foo.com", "foo@foo.com", WideToUTF8(kOldStyleSyncDataDB));
    sqlite3_close(primer_handle);
  }

  void CleanUpVersion10Databases() {
    ASSERT_TRUE(file_util::DieFileDie(FilePath(kV10UserSettingsDB), false));
    ASSERT_TRUE(file_util::DieFileDie(FilePath(kOldStyleSyncDataDB), false));
    ASSERT_TRUE(file_util::DieFileDie(FilePath(L"SyncData.sqlite3"), false));
  }

  const std::string& sync_data() const { return sync_data_; }

 private:
  std::string sync_data_;
};

TEST_F(UserSettingsTest, MigrateFromV10ToV11) {
  SetUpVersion10Databases();
  {
    // Create a UserSettings, which should trigger migration code.
    // We do this inside a scoped block so it closes itself and we can poke
    // around to see what happened later.
    UserSettings settings;
    settings.Init(kV10UserSettingsDB);
  }

  // Now poke around using sqlite to see if UserSettings migrated properly.
  sqlite3* handle = NULL;
  ASSERT_EQ(SQLITE_OK, SqliteOpen(kV10UserSettingsDB, &handle));
  ScopedStatement version_query(PrepareQuery(handle,
      "SELECT version FROM db_version"));
  ASSERT_EQ(SQLITE_ROW, sqlite3_step(version_query.get()));

  const int version = sqlite3_column_int(version_query.get(), 0);
  EXPECT_EQ(11, version);
  EXPECT_FALSE(file_util::PathExists(kOldStyleSyncDataDB));

  std::wstring path(syncable::DirectoryManager::GetSyncDataDatabaseFilename());

  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(path, &contents));
  EXPECT_EQ(sync_data(), contents);
}
