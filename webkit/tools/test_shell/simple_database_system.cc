// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/tools/test_shell/simple_database_system.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/file_util.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/database/vfs_backend.h"

using webkit_database::DatabaseTracker;
using webkit_database::DatabaseUtil;
using webkit_database::VfsBackend;

SimpleDatabaseSystem* SimpleDatabaseSystem::instance_ = NULL;

SimpleDatabaseSystem* SimpleDatabaseSystem::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

SimpleDatabaseSystem::SimpleDatabaseSystem() {
  temp_dir_.CreateUniqueTempDir();
  db_tracker_ = new DatabaseTracker(temp_dir_.path());
  DCHECK(!instance_);
  instance_ = this;
}

SimpleDatabaseSystem::~SimpleDatabaseSystem() {
  instance_ = NULL;
}

base::PlatformFile SimpleDatabaseSystem::OpenFile(
      const string16& vfs_file_name, int desired_flags,
      base::PlatformFile* dir_handle) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  FilePath file_name =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (file_name.empty()) {
    VfsBackend::OpenTempFileInDirectory(
        db_tracker_->DatabaseDirectory(), desired_flags,
        base::GetCurrentProcessHandle(), &file_handle, dir_handle);
  } else {
    VfsBackend::OpenFile(file_name, desired_flags,
                         base::GetCurrentProcessHandle(), &file_handle,
                         dir_handle);
  }

  return file_handle;
}

int SimpleDatabaseSystem::DeleteFile(
    const string16& vfs_file_name, bool sync_dir) {
  // We try to delete the file multiple times, because that's what the default
  // VFS does (apparently deleting a file can sometimes fail on Windows).
  // We sleep for 10ms between retries for the same reason.
  const int kNumDeleteRetries = 3;
  int num_retries = 0;
  int error_code = SQLITE_OK;
  FilePath file_name =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  do {
    error_code = VfsBackend::DeleteFile(file_name, sync_dir);
  } while ((++num_retries < kNumDeleteRetries) &&
           (error_code == SQLITE_IOERR_DELETE) &&
           (PlatformThread::Sleep(10), 1));

  return error_code;
}

long SimpleDatabaseSystem::GetFileAttributes(const string16& vfs_file_name) {
  return VfsBackend::GetFileAttributes(
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name));
}

long long SimpleDatabaseSystem::GetFileSize(const string16& vfs_file_name) {
  return VfsBackend::GetFileSize(
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name));
}

void SimpleDatabaseSystem::DatabaseOpened(const string16& origin_identifier,
                                          const string16& database_name,
                                          const string16& description,
                                          int64 estimated_size) {
  int64 database_size = 0;
  int64 space_available = 0;
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size, &space_available);
  OnDatabaseSizeChanged(origin_identifier, database_name,
                        database_size, space_available);
}

void SimpleDatabaseSystem::DatabaseModified(const string16& origin_identifier,
                                            const string16& database_name) {
  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void SimpleDatabaseSystem::DatabaseClosed(const string16& origin_identifier,
                                          const string16& database_name) {
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
}

void SimpleDatabaseSystem::OnDatabaseSizeChanged(
    const string16& origin_identifier,
    const string16& database_name,
    int64 database_size,
    int64 space_available) {
  WebKit::WebDatabase::updateDatabaseSize(
      origin_identifier, database_name, database_size, space_available);
}

void SimpleDatabaseSystem::databaseOpened(const WebKit::WebDatabase& database) {
  DatabaseOpened(database.securityOrigin().databaseIdentifier(),
                 database.name(), database.displayName(),
                 database.estimatedSize());
}

void SimpleDatabaseSystem::databaseModified(
    const WebKit::WebDatabase& database) {
  DatabaseModified(database.securityOrigin().databaseIdentifier(),
                   database.name());
}

void SimpleDatabaseSystem::databaseClosed(const WebKit::WebDatabase& database) {
  DatabaseClosed(database.securityOrigin().databaseIdentifier(),
                 database.name());
}

void SimpleDatabaseSystem::ClearAllDatabases() {
  db_tracker_->CloseTrackerDatabaseAndClearCaches();
  file_util::Delete(db_tracker_->DatabaseDirectory(), true);
}
