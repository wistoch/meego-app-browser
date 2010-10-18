// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_HOST_INFO_H
#define NET_BASE_SSL_HOST_INFO_H

#include <string>
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"

namespace net {

// SSLHostInfo is an interface for fetching information about an SSL server.
// This information may be stored on disk so does not include keys or session
// information etc. Primarily it's intended for caching the server's
// certificates.
class SSLHostInfo {
 public:
  virtual ~SSLHostInfo();

  // Start will commence the lookup. This must be called before any other
  // methods. By opportunistically calling this early, it may be possible to
  // overlap this object's lookup and reduce latency.
  virtual void Start() = 0;

  // WaitForDataReady returns OK if the fetch of the requested data has
  // completed. Otherwise it returns ERR_IO_PENDING and will call |callback| on
  // the current thread when ready.
  //
  // Only a single callback can be outstanding at a given time and, in the
  // event that WaitForDataReady returns OK, it's the caller's responsibility
  // to delete |callback|.
  //
  // |callback| may be NULL, in which case ERR_IO_PENDING may still be returned
  // but, obviously, a callback will never be made.
  virtual int WaitForDataReady(CompletionCallback* callback) = 0;

  // data returns any host information once WaitForDataReady has indicated that
  // the fetch has completed. In the event of an error, |data| returns an empty
  // string.
  virtual const std::string& data() const = 0;

  // Set allows for the host information to be updated for future users. This
  // is a fire and forget operation: the caller may drop its reference from
  // this object and the store operation will still complete. This can only be
  // called once WaitForDataReady has returned OK or called its callback.
  virtual void Set(const std::string& new_data) = 0;
};

class SSLHostInfoFactory {
 public:
  virtual ~SSLHostInfoFactory();

  // GetForHost returns a fresh, allocated SSLHostInfo for the given hostname
  // or NULL on failure.
  virtual SSLHostInfo* GetForHost(const std::string& hostname) = 0;
};

}  // namespace net

#endif  // NET_BASE_SSL_HOST_INFO_H
