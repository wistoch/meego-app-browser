// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_

#include "base/hash_tables.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/common/content_settings.h"
#include "ipc/ipc_message.h"
#include "webkit/database/database_connections.h"
#include "webkit/database/database_tracker.h"

class ResourceMessageFilter;

class DatabaseDispatcherHost
    : public base::RefCountedThreadSafe<DatabaseDispatcherHost>,
      public webkit_database::DatabaseTracker::Observer {
 public:
  DatabaseDispatcherHost(webkit_database::DatabaseTracker* db_tracker,
                         ResourceMessageFilter* resource_message_filter);
  void Init(base::ProcessHandle process_handle);
  void Shutdown();

  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

  // VFS message handlers (IO thread)
  void OnDatabaseOpenFile(const string16& vfs_file_name,
                          int desired_flags,
                          int32 message_id);
  void OnDatabaseDeleteFile(const string16& vfs_file_name,
                            const bool& sync_dir,
                            int32 message_id);
  void OnDatabaseGetFileAttributes(const string16& vfs_file_name,
                                   int32 message_id);
  void OnDatabaseGetFileSize(const string16& vfs_file_name,
                             int32 message_id);

  // Database tracker message handlers (IO thread)
  void OnDatabaseOpened(const string16& origin_identifier,
                        const string16& database_name,
                        const string16& description,
                        int64 estimated_size);
  void OnDatabaseModified(const string16& origin_identifier,
                          const string16& database_name);
  void OnDatabaseClosed(const string16& origin_identifier,
                        const string16& database_name);

  // DatabaseTracker::Observer callbacks (file thread)
  virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                     const string16& database_name,
                                     int64 database_size,
                                     int64 space_available);
  virtual void OnDatabaseScheduledForDeletion(const string16& origin_identifier,
                                              const string16& database_name);

 private:
  void AddObserver();
  void RemoveObserver();

  void ReceivedBadMessage(uint32 msg_type);
  void SendMessage(IPC::Message* message);

  // VFS message handlers (file thread)
  void DatabaseOpenFile(const string16& vfs_file_name,
                        int desired_flags,
                        int32 message_id);
  void DatabaseDeleteFile(const string16& vfs_file_name,
                          bool sync_dir,
                          int32 message_id,
                          int reschedule_count);
  void DatabaseGetFileAttributes(const string16& vfs_file_name,
                                 int32 message_id);
  void DatabaseGetFileSize(const string16& vfs_file_name,
                           int32 message_id);

  // Database tracker message handlers (file thread)
  void DatabaseOpened(const string16& origin_identifier,
                      const string16& database_name,
                      const string16& description,
                      int64 estimated_size);
  void DatabaseModified(const string16& origin_identifier,
                        const string16& database_name);
  void DatabaseClosed(const string16& origin_identifier,
                      const string16& database_name);

  // Called once we decide whether to allow or block an open file request.
  void OnDatabaseOpenFileAllowed(const string16& vfs_file_name,
                                 int desired_flags,
                                 int32 message_id);
  void OnDatabaseOpenFileBlocked(int32 message_id);

  // The database tracker for the current profile.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  // The resource message filter that owns us.
  ResourceMessageFilter* resource_message_filter_;

  // The handle of this process.
  base::ProcessHandle process_handle_;

  // True if and only if this instance was added as an observer
  // to DatabaseTracker.
  bool observer_added_;

  // If true, all messages that are normally processed by this class
  // will be silently discarded. This field should be set to true
  // only when the corresponding renderer process is about to go away.
  bool shutdown_;

  // Keeps track of all DB connections opened by this renderer
  webkit_database::DatabaseConnections database_connections_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_
