// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_file_writer.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using fileapi::FileSystemOperation;
using fileapi::FileSystemCallbackDispatcher;
using fileapi::WebFileWriterBase;
using WebKit::WebFileWriterClient;
using WebKit::WebString;
using WebKit::WebURL;

URLRequestContext* SimpleFileWriter::request_context_ = NULL;

// Helper class to proxy to write and truncate calls to the IO thread,
// and to proxy the results back to the main thead. There is a one-to-one
// relationship between SimpleFileWriters and IOThreadBackends.
class SimpleFileWriter::IOThreadProxy
    : public base::RefCountedThreadSafe<SimpleFileWriter::IOThreadProxy> {
 public:
  explicit IOThreadProxy(const base::WeakPtr<SimpleFileWriter>& simple_writer)
      : simple_writer_(simple_writer) {
    // The IO thread needs to be running for this class to work.
    SimpleResourceLoaderBridge::EnsureIOThread();
    io_thread_ = SimpleResourceLoaderBridge::GetIoThread();
    main_thread_ = base::MessageLoopProxy::CreateForCurrentThread();
  }

  virtual ~IOThreadProxy() {
  }

  void Truncate(const FilePath& path, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::Truncate, path, offset));
      return;
    }
    DCHECK(!operation_.get());
    operation_.reset(GetNewOperation());
    operation_->Truncate(path, offset);
  }

  void Write(const FilePath& path, const GURL& blob_url, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::Write, path, blob_url, offset));
      return;
    }
    DCHECK(request_context_);
    DCHECK(!operation_.get());
    operation_.reset(GetNewOperation());
    operation_->Write(request_context_, path, blob_url, offset);
  }

  void Cancel() {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::Cancel));
      return;
    }
    if (!operation_.get()) {
      DidFail(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
      return;
    }
    cancel_operation_.reset(GetNewOperation());
    operation_->Cancel(cancel_operation_.get());
  }

 private:
  // Inner class to receive callbacks from FileSystemOperation.
  class CallbackDispatcher : public FileSystemCallbackDispatcher {
   public:
    explicit CallbackDispatcher(IOThreadProxy* proxy) : proxy_(proxy) {
    }

    virtual void DidSucceed() {
      proxy_->DidSucceed();
    }

    virtual void DidFail(base::PlatformFileError error_code) {
      proxy_->DidFail(error_code);
    }

    virtual void DidWrite(int64 bytes, bool complete) {
      proxy_->DidWrite(bytes, complete);
    }

    virtual void DidReadMetadata(const base::PlatformFileInfo&) {
      NOTREACHED();
    }

    virtual void DidReadDirectory(
        const std::vector<base::file_util_proxy::Entry>& entries,
        bool has_more) {
      NOTREACHED();
    }

    virtual void DidOpenFileSystem(const std::string& name,
                                   const FilePath& root_path) {
      NOTREACHED();
    }

    scoped_refptr<IOThreadProxy> proxy_;
  };

  FileSystemOperation* GetNewOperation() {
    // The FileSystemOperation takes ownership of the CallbackDispatcher.
    return new FileSystemOperation(new CallbackDispatcher(this), io_thread_);
  }

  void DidSucceed() {
    if (!main_thread_->BelongsToCurrentThread()) {
      operation_.reset();
      main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::DidSucceed));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidSucceed();
  }

  void DidFail(base::PlatformFileError error_code) {
    if (!main_thread_->BelongsToCurrentThread()) {
      operation_.reset();
      main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::DidFail, error_code));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidFail(error_code);
  }

  void DidWrite(int64 bytes, bool complete) {
    if (!main_thread_->BelongsToCurrentThread()) {
      if (complete)
        operation_.reset();
      main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &IOThreadProxy::DidWrite, bytes, complete));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidWrite(bytes, complete);
  }

  scoped_refptr<base::MessageLoopProxy> io_thread_;
  scoped_refptr<base::MessageLoopProxy> main_thread_;

  // Only used on the main thread.
  base::WeakPtr<SimpleFileWriter> simple_writer_;

  // Only used on the io thread.
  scoped_ptr<FileSystemOperation> operation_;
  scoped_ptr<FileSystemOperation> cancel_operation_;
};


SimpleFileWriter::SimpleFileWriter(
     const WebString& path, WebFileWriterClient* client)
  : WebFileWriterBase(path, client),
    io_thread_proxy_(new IOThreadProxy(AsWeakPtr())) {
}

SimpleFileWriter::~SimpleFileWriter() {
}

void SimpleFileWriter::DoTruncate(const FilePath& path, int64 offset) {
  io_thread_proxy_->Truncate(path, offset);
}

void SimpleFileWriter::DoWrite(
    const FilePath& path, const GURL& blob_url, int64 offset) {
  io_thread_proxy_->Write(path, blob_url, offset);
}

void SimpleFileWriter::DoCancel() {
  io_thread_proxy_->Cancel();
}
