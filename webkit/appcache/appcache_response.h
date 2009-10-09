// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_RESPONSE_H_
#define WEBKIT_APPCACHE_APPCACHE_RESPONSE_H_

#include "base/logging.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/http/http_response_info.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage.h"

namespace net {
class IOBuffer;
}
namespace disk_cache {
class Backend;
};


namespace appcache {

// Response info for a particular response id. Instances are tracked in
// the working set.
class AppCacheResponseInfo
    : public base::RefCounted<AppCacheResponseInfo> {

  // AppCacheResponseInfo takes ownership of the http_info.
  AppCacheResponseInfo(AppCacheService* service, int64 response_id,
                       net::HttpResponseInfo* http_info)
      : response_id_(response_id), http_response_info_(http_info),
        service_(service) {
    DCHECK(http_info);
    DCHECK(response_id != kNoResponseId);
    service_->storage()->working_set()->AddResponseInfo(this);
  }

  ~AppCacheResponseInfo() {
    service_->storage()->working_set()->RemoveResponseInfo(this);
  }

  int64 response_id() const { return response_id_; }

  const net::HttpResponseInfo* http_response_info() const {
    return http_response_info_.get();
  }

 private:
  const int64 response_id_;
  const scoped_ptr<net::HttpResponseInfo> http_response_info_;
  const AppCacheService* service_;
};

// Common base class for response reader and writer.
class AppCacheResponseIO {
 public:
  virtual ~AppCacheResponseIO() {}
  int64 response_id() const { return response_id_; }
 protected:
  explicit AppCacheResponseIO(
      int64 response_id, disk_cache::Backend* disk_cache)
      : response_id_(response_id), disk_cache_(disk_cache) {}
  const int64 response_id_;
  disk_cache::Backend* disk_cache_;
};

// A refcounted wrapper for HttpResponseInfo so we can apply the
// refcounting semantics used with IOBuffer with these structures too.
struct HttpResponseInfoIOBuffer
    : public base::RefCountedThreadSafe<HttpResponseInfoIOBuffer> {
  scoped_ptr<net::HttpResponseInfo> http_info;
};

// Reads existing response data from storage. If the object is deleted
// and there is a read in progress, the implementation will return
// immediately but will take care of any side effect of cancelling the
// operation.  In other words, instances are safe to delete at will.
class AppCacheResponseReader : public AppCacheResponseIO {
 public:
  // Reads http info from storage. Returns the number of bytes read
  // or a net:: error code. Guaranteed to not perform partial reads of
  // the info data. ERR_IO_PENDING is returned if the
  // operation could not be completed synchronously, in which case the reader
  // acquires a reference to the provided 'info_buf' until completion at which
  // time the callback is invoked with either a negative error code or the
  // number of bytes written. The 'info_buf' argument should contain a NULL
  // http_info when ReadInfo is called. The 'callback' is a required parameter.
  // Should only be called where there is no Read operation in progress.
  int ReadInfo(HttpResponseInfoIOBuffer* info_buf,
               net::CompletionCallback* callback) {
    DCHECK(info_buf && !info_buf->http_info.get());
    return -2;
  }

  // Reads data from storage. Returns the number of bytes read
  // or a net:: error code. EOF is indicated with a return value of zero.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the reader acquires a reference to the
  // provided 'buf' until completion at which time the callback is invoked
  // with either a negative error code or the number of bytes read. The
  // 'callback' is a required parameter.
  // Should only be called where there is no Read operation in progress.
  int ReadData(net::IOBuffer* buf, int buf_len,
               net::CompletionCallback* callback) { return -2; }

  // Returns true if there is a read operation, for data or info, pending.
  bool IsReadPending() { return false; }

  // Used to support range requests. If not called, the reader will
  // read the entire response body. If called, this must be called prior
  // to the first call to the ReadData method.
  void SetReadRange(int64 offset, int64 length) {
    range_offset_ = offset;
    range_length_ = length;
  }

 private:
  friend class AppCacheStorageImpl;
  friend class MockAppCacheStorage;

  // Should only be constructed by the storage class.
  explicit AppCacheResponseReader(
      int64 response_id, disk_cache::Backend* disk_cache)
      : AppCacheResponseIO(response_id, disk_cache),
        range_offset_(0), range_length_(kint64max) {}

  int64 range_offset_;
  int64 range_length_;
};

// Writes new response data to storage. If the object is deleted
// and there is a write in progress, the implementation will return
// immediately but will take care of any side effect of cancelling the
// operation. In other words, instances are safe to delete at will.
class AppCacheResponseWriter : public AppCacheResponseIO {
 public:
  // Writes the http info to storage. Returns the number of bytes written
  // or a net:: error code. ERR_IO_PENDING is returned if the
  // operation could not be completed synchronously, in which case the writer
  // acquires a reference to the provided 'info_buf' until completion at which
  // time the callback is invoked with either a negative error code or the
  // number of bytes written. The 'callback' is a required parameter. The
  // contents of 'info_buf' are not modified.
  // Should only be called where there is no Write operation in progress.
  int WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                net::CompletionCallback* callback) {
    DCHECK(info_buf && info_buf->http_info.get());
    return -2;
  }

  // Writes data to storage. Returns the number of bytes written
  // or a net:: error code. Guaranteed to not perform partial writes.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the writer acquires a reference to the
  // provided 'buf' until completion at which time the callback is invoked
  // with either a negative error code or the number of bytes written. The
  // 'callback' is a required parameter. The contents of 'buf' are not
  // modified.
  // Should only be called where there is no Write operation in progress.
  int WriteData(net::IOBuffer* buf, int buf_len,
                net::CompletionCallback* callback) { return -2; }

  // Returns true if there is a write pending.
  bool IsWritePending() { return false; }

 private:
  friend class AppCacheStorageImpl;
  friend class MockAppCacheStorage;

  // Should only be constructed by the storage class.
  explicit AppCacheResponseWriter(
      int64 response_id, disk_cache::Backend* disk_cache)
      : AppCacheResponseIO(response_id, disk_cache) {}
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_RESPONSE_H_

