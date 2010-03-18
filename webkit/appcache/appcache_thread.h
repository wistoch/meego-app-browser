// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_THREAD_H_
#define WEBKIT_APPCACHE_APPCACHE_THREAD_H_

#include "base/task.h"

namespace tracked_objects {
class Location;
}

class MessageLoop;

namespace appcache {

// The appcache system uses two threads, an IO thread and a DB thread.
// It does not create these threads, the embedder is responsible for
// providing them to the appcache library by providing a concrete
// implementation of the PostTask and CurrentlyOn methods declared here,
// and by calling the Init method prior to using the appcache library.
// The disk_cache also requires the embedder to provide a thread message
// loop.
class AppCacheThread {
 public:
  static void Init(int db, int io, MessageLoop* disk_cache_thread) {
    db_ = db;
    io_ = io;
    disk_cache_thread_ = disk_cache_thread;
  }
  static int db() { return db_; }
  static int io() { return io_; }
  static MessageLoop* disk_cache_thread() { return disk_cache_thread_; }

  static bool PostTask(int id,
                       const tracked_objects::Location& from_here,
                       Task* task);
  static bool CurrentlyOn(int id);

  template <class T>
  static bool DeleteSoon(int id,
                         const tracked_objects::Location& from_here,
                         T* object) {
    return PostTask(id, from_here, new DeleteTask<T>(object));
  }

 private:
  AppCacheThread();
  ~AppCacheThread();

  static int db_;
  static int io_;
  static MessageLoop* disk_cache_thread_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_THREAD_H_
