// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_path_watcher/file_path_watcher.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "base/win/object_watcher.h"

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate,
                            public base::win::ObjectWatcher::Delegate {
 public:
  FilePathWatcherImpl() : delegate_(NULL), handle_(INVALID_HANDLE_VALUE) {}

  virtual bool Watch(const FilePath& path, FilePathWatcher::Delegate* delegate);
  virtual void Cancel();

  // Callback from MessageLoopForIO.
  virtual void OnObjectSignaled(HANDLE object);

 private:
  virtual ~FilePathWatcherImpl();

  // Setup a watch handle for directory |dir|. Returns true if no fatal error
  // occurs. |handle| will receive the handle value if |dir| is watchable,
  // otherwise INVALID_HANDLE_VALUE.
  static bool SetupWatchHandle(const FilePath& dir, HANDLE* handle)
      WARN_UNUSED_RESULT;

  // (Re-)Initialize the watch handle.
  bool UpdateWatch() WARN_UNUSED_RESULT;

  // Destroy the watch handle.
  void DestroyWatch();

  // Delegate to notify upon changes.
  scoped_refptr<FilePathWatcher::Delegate> delegate_;

  // Path we're supposed to watch (passed to delegate).
  FilePath target_;

  // Handle for FindFirstChangeNotification.
  HANDLE handle_;

  // ObjectWatcher to watch handle_ for events.
  base::win::ObjectWatcher watcher_;

  // Keep track of the last modified time of the file.  We use nulltime
  // to represent the file not existing.
  base::Time last_modified_;

  // The time at which we processed the first notification with the
  // |last_modified_| time stamp.
  base::Time first_notification_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherImpl);
};

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                FilePathWatcher::Delegate* delegate) {
  DCHECK(target_.value().empty());  // Can only watch one path.
  delegate_ = delegate;
  target_ = path;

  if (!UpdateWatch())
    return false;

  watcher_.StartWatching(handle_, this);

  return true;
}

void FilePathWatcherImpl::Cancel() {
  // Switch to the file thread if necessary so we can stop |watcher_|.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &FilePathWatcherImpl::Cancel));
    return;
  }

  if (handle_ != INVALID_HANDLE_VALUE)
    DestroyWatch();
}

void FilePathWatcherImpl::OnObjectSignaled(HANDLE object) {
  DCHECK(object == handle_);
  // Make sure we stay alive through the body of this function.
  scoped_refptr<FilePathWatcherImpl> keep_alive(this);

  if (!UpdateWatch()) {
    delegate_->OnError();
    return;
  }

  // Check whether the event applies to |target_| and notify the delegate.
  base::PlatformFileInfo file_info;
  bool file_exists = file_util::GetFileInfo(target_, &file_info);
  if (file_exists && (last_modified_.is_null() ||
      last_modified_ != file_info.last_modified)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = base::Time::Now();
    delegate_->OnFilePathChanged(target_);
  } else if (file_exists && !first_notification_.is_null()) {
    // The target's last modification time is equal to what's on record. This
    // means that either an unrelated event occurred, or the target changed
    // again (file modification times only have a resolution of 1s). Comparing
    // file modification times against the wall clock is not reliable to find
    // out whether the change is recent, since this code might just run too
    // late. Moreover, there's no guarantee that file modification time and wall
    // clock times come from the same source.
    //
    // Instead, the time at which the first notification carrying the current
    // |last_notified_| time stamp is recorded. Later notifications that find
    // the same file modification time only need to be forwarded until wall
    // clock has advanced one second from the initial notification. After that
    // interval, client code is guaranteed to having seen the current revision
    // of the file.
    if (base::Time::Now() - first_notification_ >
        base::TimeDelta::FromSeconds(1)) {
      // Stop further notifications for this |last_modification_| time stamp.
      first_notification_ = base::Time();
    }
    delegate_->OnFilePathChanged(target_);
  } else if (!file_exists && !last_modified_.is_null()) {
    last_modified_ = base::Time();
    delegate_->OnFilePathChanged(target_);
  }

  // The watch may have been cancelled by the callback.
  if (handle_ != INVALID_HANDLE_VALUE)
    watcher_.StartWatching(handle_, this);
}

FilePathWatcherImpl::~FilePathWatcherImpl() {
  if (handle_ != INVALID_HANDLE_VALUE)
    DestroyWatch();
}

// static
bool FilePathWatcherImpl::SetupWatchHandle(const FilePath& dir,
                                           HANDLE* handle) {
  *handle = FindFirstChangeNotification(
      dir.value().c_str(),
      false,  // Don't watch subtrees
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
      FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME |
      FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY);
  if (*handle != INVALID_HANDLE_VALUE) {
    // Make sure the handle we got points to an existing directory. It seems
    // that windows sometimes hands out watches to direectories that are
    // about to go away, but doesn't sent notifications if that happens.
    if (!file_util::DirectoryExists(dir)) {
      FindCloseChangeNotification(*handle);
      *handle = INVALID_HANDLE_VALUE;
    }
    return true;
  }

  // If FindFirstChangeNotification failed because the target directory
  // doesn't exist, access is denied (happens if the file is already gone but
  // there are still handles open), or the target is not a directory, try the
  // immediate parent directory instead.
  DWORD error_code = GetLastError();
  if (error_code != ERROR_FILE_NOT_FOUND &&
      error_code != ERROR_PATH_NOT_FOUND &&
      error_code != ERROR_ACCESS_DENIED &&
      error_code != ERROR_SHARING_VIOLATION &&
      error_code != ERROR_DIRECTORY) {
    PLOG(ERROR) << "FindFirstChangeNotification failed for "
                << dir.value();
    return false;
  }

  return true;
}

bool FilePathWatcherImpl::UpdateWatch() {
  if (handle_ != INVALID_HANDLE_VALUE)
    DestroyWatch();

  base::PlatformFileInfo file_info;
  if (file_util::GetFileInfo(target_, &file_info)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = base::Time::Now();
  }

  // Start at the target and walk up the directory chain until we succesfully
  // create a watch handle in |handle_|. |child_dirs| keeps a stack of child
  // directories stripped from target, in reverse order.
  std::vector<FilePath> child_dirs;
  FilePath watched_path(target_);
  while (true) {
    if (!SetupWatchHandle(watched_path, &handle_))
      return false;

    // Break if a valid handle is returned. Try the parent directory otherwise.
    if (handle_ != INVALID_HANDLE_VALUE)
      break;

    // Abort if we hit the root directory.
    child_dirs.push_back(watched_path.BaseName());
    FilePath parent(watched_path.DirName());
    if (parent == watched_path) {
      LOG(ERROR) << "Reached the root directory";
      return false;
    }
    watched_path = parent;
  }

  // At this point, handle_ is valid. However, the bottom-up search that the
  // above code performs races against directory creation. So try to walk back
  // down and see whether any children appeared in the mean time.
  while (!child_dirs.empty()) {
    watched_path = watched_path.Append(child_dirs.back());
    child_dirs.pop_back();
    HANDLE temp_handle = INVALID_HANDLE_VALUE;
    if (!SetupWatchHandle(watched_path, &temp_handle))
      return false;
    if (temp_handle == INVALID_HANDLE_VALUE)
      break;
    FindCloseChangeNotification(handle_);
    handle_ = temp_handle;
  }

  return true;
}

void FilePathWatcherImpl::DestroyWatch() {
  watcher_.StopWatching();
  FindCloseChangeNotification(handle_);
  handle_ = INVALID_HANDLE_VALUE;
}

}  // namespace

FilePathWatcher::FilePathWatcher() {
  impl_ = new FilePathWatcherImpl();
}
