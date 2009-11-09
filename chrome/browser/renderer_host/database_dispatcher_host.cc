// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_dispatcher_host.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"
#include "webkit/database/vfs_backend.h"

using webkit_database::DatabaseTracker;
using webkit_database::VfsBackend;

const int kNumDeleteRetries = 2;
const int kDelayDeleteRetryMs = 100;

DatabaseDispatcherHost::DatabaseDispatcherHost(
    DatabaseTracker* db_tracker,
    IPC::Message::Sender* message_sender,
    base::ProcessHandle process_handle)
    : db_tracker_(db_tracker),
      message_sender_(message_sender),
      process_handle_(process_handle),
      observer_added_(false),
      shutdown_(false) {
  DCHECK(db_tracker_);
  DCHECK(message_sender_);
}

void DatabaseDispatcherHost::Shutdown() {
  shutdown_ = true;
  message_sender_ = NULL;
  if (observer_added_) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::RemoveObserver));
  }
}

void DatabaseDispatcherHost::AddObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  db_tracker_->AddObserver(this);
}

void DatabaseDispatcherHost::RemoveObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  db_tracker_->RemoveObserver(this);
}

FilePath DatabaseDispatcherHost::GetDBFileFullPath(
    const FilePath& vfs_file_name) {
  // 'vfs_file_name' can be one of 3 things:
  // 1. Empty string: It means the VFS wants to open a temp file. In this case
  //    we need to return the path to the directory that stores all databases.
  // 2. origin_identifier/database_name: In this case, we need to extract
  //    'origin_identifier' and 'database_name' and pass them to
  //    DatabaseTracker::GetFullDBFilePath().
  // 3. origin_identifier/database_name-suffix: '-suffix' could be '-journal',
  //    for example. In this case, we need to extract 'origin_identifier' and
  //    'database_name-suffix' and pass them to
  //    DatabaseTracker::GetFullDBFilePath(). 'database_name-suffix' is not
  //    a database name as expected by DatabaseTracker::GetFullDBFilePath(),
  //    but due to its implementation, it's OK to pass in 'database_name-suffix'
  //    too.
  //
  // We also check that the given string doesn't contain invalid characters
  // that would result in a DB file stored outside of the directory where
  // all DB files are supposed to be stored.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (vfs_file_name.empty())
    return db_tracker_->DatabaseDirectory();

  std::wstring str = vfs_file_name.ToWStringHack();
  size_t slashIndex = str.find('/');
  if (slashIndex == std::wstring::npos)
    return FilePath();  // incorrect format
  std::wstring origin_identifier = str.substr(0, slashIndex);
  std::wstring database_name =
      str.substr(slashIndex + 1, str.length() - slashIndex);
  if ((origin_identifier.find('\\') != std::wstring::npos) ||
      (origin_identifier.find('/') != std::wstring::npos) ||
      (origin_identifier.find(':') != std::wstring::npos) ||
      (database_name.find('\\') != std::wstring::npos) ||
      (database_name.find('/') != std::wstring::npos) ||
      (database_name.find(':') != std::wstring::npos)) {
    return FilePath();
  }

  return db_tracker_->GetFullDBFilePath(
      WideToUTF16(origin_identifier), WideToUTF16(database_name));
}

bool DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(!shutdown_);
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DatabaseDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpenFile, OnDatabaseOpenFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseDeleteFile, OnDatabaseDeleteFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileAttributes,
                        OnDatabaseGetFileAttributes)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileSize,
                        OnDatabaseGetFileSize)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpened, OnDatabaseOpened)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseModified, OnDatabaseModified)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseClosed, OnDatabaseClosed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DatabaseDispatcherHost::ReceivedBadMessage(uint16 msg_type) {
  BrowserRenderProcessHost::BadMessageTerminateProcess(
      msg_type, process_handle_);
}

// Scheduled by the file thread on the IO thread.
// Sends back to the renderer process the given message.
void DatabaseDispatcherHost::SendMessage(IPC::Message* message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!shutdown_)
    message_sender_->Send(message);
  else
    delete message;
}

void DatabaseDispatcherHost::OnDatabaseOpenFile(const FilePath& vfs_file_name,
                                                int desired_flags,
                                                int32 message_id) {
  if (!observer_added_) {
    observer_added_ = true;
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::AddObserver));
  }

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseOpenFile,
                        vfs_file_name,
                        desired_flags,
                        message_id));
}

static void SetOpenFileResponseParams(
    ViewMsg_DatabaseOpenFileResponse_Params* params,
    base::PlatformFile file_handle,
    base::PlatformFile dir_handle) {
#if defined(OS_WIN)
  params->file_handle = file_handle;
#elif defined(OS_POSIX)
  params->file_handle = base::FileDescriptor(file_handle, true);
  params->dir_handle = base::FileDescriptor(dir_handle, true);
#endif
}

// Scheduled by the IO thread on the file thread.
// Opens the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the file handle.
void DatabaseDispatcherHost::DatabaseOpenFile(const FilePath& vfs_file_name,
                                              int desired_flags,
                                              int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  base::PlatformFile target_handle = base::kInvalidPlatformFileValue;
  base::PlatformFile target_dir_handle = base::kInvalidPlatformFileValue;
  FilePath db_file_name = GetDBFileFullPath(vfs_file_name);
  if (!db_file_name.empty()) {
    FilePath db_dir = db_tracker_->DatabaseDirectory();
    VfsBackend::OpenFile(db_file_name, db_dir, desired_flags,
                         process_handle_, &target_handle, &target_dir_handle);
  }

  ViewMsg_DatabaseOpenFileResponse_Params response_params;
  SetOpenFileResponseParams(&response_params, target_handle, target_dir_handle);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseOpenFileResponse(
                            message_id, response_params)));
}

void DatabaseDispatcherHost::OnDatabaseDeleteFile(const FilePath& vfs_file_name,
                                                  const bool& sync_dir,
                                                  int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseDeleteFile,
                        vfs_file_name,
                        sync_dir,
                        message_id,
                        kNumDeleteRetries));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
void DatabaseDispatcherHost::DatabaseDeleteFile(const FilePath& vfs_file_name,
                                                bool sync_dir,
                                                int32 message_id,
                                                int reschedule_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  FilePath db_file_name = GetDBFileFullPath(vfs_file_name);
  if (!db_file_name.empty()) {
    FilePath db_dir = db_tracker_->DatabaseDirectory();
    error_code = VfsBackend::DeleteFile(db_file_name, db_dir, sync_dir);
    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      ChromeThread::PostDelayedTask(
          ChromeThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &DatabaseDispatcherHost::DatabaseDeleteFile,
                            vfs_file_name,
                            sync_dir,
                            message_id,
                            reschedule_count - 1),
          kDelayDeleteRetryMs);
      return;
    }
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseDeleteFileResponse(
                            message_id, error_code)));
}

void DatabaseDispatcherHost::OnDatabaseGetFileAttributes(
    const FilePath& vfs_file_name,
    int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileAttributes,
                        vfs_file_name,
                        message_id));
}

// Scheduled by the IO thread on the file thread.
// Gets the attributes of the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileAttributes(
    const FilePath& vfs_file_name,
    int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int32 attributes = -1;
  FilePath db_file_name = GetDBFileFullPath(vfs_file_name);
  if (!db_file_name.empty())
    attributes = VfsBackend::GetFileAttributes(db_file_name);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseGetFileAttributesResponse(
                            message_id, attributes)));
}

void DatabaseDispatcherHost::OnDatabaseGetFileSize(
  const FilePath& vfs_file_name, int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileSize,
                        vfs_file_name,
                        message_id));
}

// Scheduled by the IO thread on the file thread.
// Gets the size of the given file, then schedules a task
// on the IO thread's message loop to send an IPC back to
// the corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileSize(const FilePath& vfs_file_name,
                                                 int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int64 size = 0;
  FilePath db_file_name = GetDBFileFullPath(vfs_file_name);
  if (!db_file_name.empty())
    size = VfsBackend::GetFileSize(db_file_name);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseGetFileSizeResponse(
                            message_id, size)));
}

void DatabaseDispatcherHost::OnDatabaseOpened(const string16& origin_identifier,
                                              const string16& database_name,
                                              const string16& description,
                                              int64 estimated_size) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseOpened,
                        origin_identifier,
                        database_name,
                        description,
                        estimated_size));
}

void DatabaseDispatcherHost::DatabaseOpened(const string16& origin_identifier,
                                            const string16& database_name,
                                            const string16& description,
                                            int64 estimated_size) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int64 database_size = 0;
  int64 space_available = 0;
  AddAccessedOrigin(origin_identifier);
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size, &space_available);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseUpdateSize(
                            origin_identifier, database_name,
                            database_size, space_available)));
}

void DatabaseDispatcherHost::OnDatabaseModified(
    const string16& origin_identifier,
    const string16& database_name) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseModified,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::DatabaseModified(const string16& origin_identifier,
                                              const string16& database_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (!HasAccessedOrigin(origin_identifier)) {
    ReceivedBadMessage(ViewHostMsg_DatabaseModified::ID);
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void DatabaseDispatcherHost::OnDatabaseClosed(const string16& origin_identifier,
                                              const string16& database_name) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseClosed,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::DatabaseClosed(const string16& origin_identifier,
                                            const string16& database_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (!HasAccessedOrigin(origin_identifier)) {
    ReceivedBadMessage(ViewHostMsg_DatabaseClosed::ID);
    return;
  }

  db_tracker_->DatabaseClosed(origin_identifier, database_name);
}

void DatabaseDispatcherHost::OnDatabaseSizeChanged(
    const string16& origin_identifier,
    const string16& database_name,
    int64 database_size,
    int64 space_available) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (HasAccessedOrigin(origin_identifier)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &DatabaseDispatcherHost::SendMessage,
                          new ViewMsg_DatabaseUpdateSize(
                              origin_identifier, database_name,
                              database_size, space_available)));
  }
}

void DatabaseDispatcherHost::AddAccessedOrigin(
    const string16& origin_identifier) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  accessed_origins_.insert(origin_identifier);
}

bool DatabaseDispatcherHost::HasAccessedOrigin(
    const string16& origin_identifier) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  return (accessed_origins_.find(origin_identifier) != accessed_origins_.end());
}
