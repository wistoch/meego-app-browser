// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/time.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_writer_delegate.h"

namespace fileapi {

FileSystemOperation::FileSystemOperation(
    FileSystemCallbackDispatcher* dispatcher,
    scoped_refptr<base::MessageLoopProxy> proxy)
    : proxy_(proxy),
      dispatcher_(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      cancel_operation_(NULL) {
  DCHECK(dispatcher);
#ifndef NDEBUG
  pending_operation_ = kOperationNone;
#endif
}

FileSystemOperation::~FileSystemOperation() {
  if (file_writer_delegate_.get())
    base::FileUtilProxy::Close(proxy_, file_writer_delegate_->file(), NULL);
}

void FileSystemOperation::CreateFile(const FilePath& path,
                                     bool exclusive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateFile;
#endif

  base::FileUtilProxy::Create(
    proxy_, path, base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
    callback_factory_.NewCallback(
        exclusive ? &FileSystemOperation::DidCreateFileExclusive
                  : &FileSystemOperation::DidCreateFileNonExclusive));
}

void FileSystemOperation::CreateDirectory(const FilePath& path,
                                          bool exclusive,
                                          bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateDirectory;
#endif

  base::FileUtilProxy::CreateDirectory(
      proxy_, path, exclusive, recursive, callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Copy(const FilePath& src_path,
                               const FilePath& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCopy;
#endif

  base::FileUtilProxy::Copy(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Move(const FilePath& src_path,
                               const FilePath& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationMove;
#endif

  base::FileUtilProxy::Move(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DirectoryExists(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationDirectoryExists;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidDirectoryExists));
}

void FileSystemOperation::FileExists(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationFileExists;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidFileExists));
}

void FileSystemOperation::GetMetadata(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationGetMetadata;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidGetMetadata));
}

void FileSystemOperation::ReadDirectory(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationReadDirectory;
#endif

  base::FileUtilProxy::ReadDirectory(proxy_, path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidReadDirectory));
}

void FileSystemOperation::Remove(const FilePath& path, bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationRemove;
#endif

  base::FileUtilProxy::Delete(proxy_, path, recursive,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Write(
    scoped_refptr<URLRequestContext> url_request_context,
    const FilePath& path,
    const GURL& blob_url,
    int64 offset) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationWrite;
#endif
  DCHECK(blob_url.is_valid());
  file_writer_delegate_.reset(new FileWriterDelegate(this, offset));
  blob_request_.reset(new URLRequest(blob_url, file_writer_delegate_.get()));
  blob_request_->set_context(url_request_context);
  base::FileUtilProxy::Create(
      proxy_,
      path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      callback_factory_.NewCallback(
          &FileSystemOperation::OnFileOpenedForWrite));
}

void FileSystemOperation::OnFileOpenedForWrite(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  if (base::PLATFORM_FILE_OK != rv) {
    dispatcher_->DidFail(rv);
    return;
  }
  file_writer_delegate_->Start(file.ReleaseValue(), blob_request_.get());
}

void FileSystemOperation::Truncate(const FilePath& path, int64 length) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTruncate;
#endif
  base::FileUtilProxy::Truncate(proxy_, path, length,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::TouchFile(const FilePath& path,
                                    const base::Time& last_access_time,
                                    const base::Time& last_modified_time) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTouchFile;
#endif

  base::FileUtilProxy::Touch(
      proxy_, path, last_access_time, last_modified_time,
      callback_factory_.NewCallback(&FileSystemOperation::DidTouchFile));
}

// We can only get here on a write or truncate that's not yet completed.
// We don't support cancelling any other operation at this time.
void FileSystemOperation::Cancel(FileSystemOperation* cancel_operation) {
  if (file_writer_delegate_.get()) {
#ifndef NDEBUG
    DCHECK(kOperationWrite == pending_operation_);
#endif
    // Writes are done without proxying through FileUtilProxy after the initial
    // opening of the PlatformFile.  All state changes are done on this thread,
    // so we're guaranteed to be able to shut down atomically.  We do need to
    // check that the file has been opened [which means the blob_request_ has
    // been created], so we know how much we need to do.
    if (blob_request_.get())
      // This halts any calls to file_writer_delegate_ from blob_request_.
      blob_request_->Cancel();

    // This deletes us, and by proxy deletes file_writer_delegate_ if any.
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_operation->dispatcher_->DidSucceed();
  } else {
#ifndef NDEBUG
    DCHECK(kOperationTruncate == pending_operation_);
#endif
    // We're cancelling a truncate operation, but we can't actually stop it
    // since it's been proxied to another thread.  We need to save the
    // cancel_operation so that when the truncate returns, it can see that it's
    // been cancelled, report it, and report that the cancel has succeeded.
    cancel_operation_ = cancel_operation;
  }
}

void FileSystemOperation::DidCreateFileExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  DidFinishFileOperation(rv);
}

void FileSystemOperation::DidCreateFileNonExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  // Suppress the already exists error and report success.
  if (rv == base::PLATFORM_FILE_OK ||
      rv == base::PLATFORM_FILE_ERROR_EXISTS)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidFinishFileOperation(
    base::PlatformFileError rv) {
  if (cancel_operation_) {
#ifndef NDEBUG
    DCHECK(kOperationTruncate == pending_operation_);
#endif
    FileSystemOperation *cancel_op = cancel_operation_;
    // This call deletes us, so we have to extract cancel_op first.
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_op->dispatcher_->DidSucceed();
  } else if (rv == base::PLATFORM_FILE_OK) {
    dispatcher_->DidSucceed();
  } else {
    dispatcher_->DidFail(rv);
  }
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidSucceed();
    else
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_FAILED);
  } else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_FAILED);
    else
      dispatcher_->DidSucceed();
  } else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadMetadata(file_info);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::file_util_proxy::Entry>& entries) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadDirectory(entries, false /* has_more */);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidWrite(bytes, complete);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidTouchFile(base::PlatformFileError rv) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
}

}  // namespace fileapi
