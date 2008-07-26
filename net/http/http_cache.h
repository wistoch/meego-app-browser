// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file declares a HttpTransactionFactory implementation that can be
// layered on top of another HttpTransactionFactory to add HTTP caching.  The
// caching logic follows RFC 2616 (any exceptions are called out in the code).
//
// The HttpCache takes a disk_cache::Backend as a parameter, and uses that for
// the cache storage.
//
// See HttpTransactionFactory and HttpTransaction for more details.

#ifndef NET_HTTP_HTTP_CACHE_H__
#define NET_HTTP_HTTP_CACHE_H__

#include <hash_map>
#include <hash_set>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/task.h"
#include "net/http/http_transaction_factory.h"

namespace disk_cache {
class Backend;
class Entry;
}

namespace net {

class HttpProxyInfo;
class HttpRequestInfo;
class HttpResponseInfo;

class HttpCache : public HttpTransactionFactory {
 public:
  ~HttpCache();

  // The cache mode of operation.
  enum Mode {
    // Normal mode just behaves like a standard web cache.
    NORMAL = 0,
    // Record mode caches everything for purposes of offline playback.
    RECORD,
    // Playback mode replays from a cache without considering any
    // standard invalidations.
    PLAYBACK
  };

  // Initialize the cache from the directory where its data is stored.  The
  // disk cache is initialized lazily (by CreateTransaction) in this case. If
  // |cache_size| is zero, a default value will be calculated automatically.
  // If the proxy information is null, then the system settings will be used.
  HttpCache(const HttpProxyInfo* proxy_info,
            const std::wstring& cache_dir,
            int cache_size);

  // Initialize using an in-memory cache. The cache is initialized lazily
  // (by CreateTransaction) in this case. If |cache_size| is zero, a default
  // value will be calculated automatically. If the proxy information is null,
  // then the system settings will be used.
  HttpCache(const HttpProxyInfo* proxy_info, int cache_size);

  // Initialize the cache from its component parts, which is useful for
  // testing.  The lifetime of the network_layer and disk_cache are managed by
  // the HttpCache and will be destroyed using |delete| when the HttpCache is
  // destroyed.
  HttpCache(HttpTransactionFactory* network_layer,
            disk_cache::Backend* disk_cache);

  HttpTransactionFactory* network_layer() { return network_layer_.get(); }
  disk_cache::Backend* disk_cache() { return disk_cache_.get(); }

  // HttpTransactionFactory implementation:
  virtual HttpTransaction* CreateTransaction();
  virtual HttpCache* GetCache();
  virtual AuthCache* GetAuthCache();
  virtual void Suspend(bool suspend);

  // Helper function for reading response info from the disk cache.
  static bool ReadResponseInfo(disk_cache::Entry* disk_entry,
                               HttpResponseInfo* response_info);

  // Helper function for writing response info into the disk cache.
  static bool WriteResponseInfo(disk_cache::Entry* disk_entry,
                                const HttpResponseInfo* response_info,
                                bool skip_transient_headers);

  // Generate a key that can be used inside the cache.
  std::string GenerateCacheKey(const HttpRequestInfo* request);

  // Get/Set the cache's mode.
  void set_mode(Mode value) { mode_ = value; }
  Mode mode() { return mode_; }

 private:

  // Types --------------------------------------------------------------------

  class Transaction;
  friend Transaction;

  typedef std::list<Transaction*> TransactionList;

  struct ActiveEntry {
    disk_cache::Entry* disk_entry;
    Transaction*       writer;
    TransactionList    readers;
    TransactionList    pending_queue;
    bool               will_process_pending_queue;
    bool               doomed;

    explicit ActiveEntry(disk_cache::Entry*);
    ~ActiveEntry();
  };

  typedef stdext::hash_map<std::string, ActiveEntry*> ActiveEntriesMap;
  typedef stdext::hash_set<ActiveEntry*> ActiveEntriesSet;


  // Methods ------------------------------------------------------------------

  void DoomEntry(const std::string& key);
  void FinalizeDoomedEntry(ActiveEntry* entry);
  ActiveEntry* FindActiveEntry(const std::string& key);
  ActiveEntry* ActivateEntry(const std::string& key, disk_cache::Entry*);
  void DeactivateEntry(ActiveEntry* entry);
  ActiveEntry* OpenEntry(const std::string& key);
  ActiveEntry* CreateEntry(const std::string& cache_key);
  void DestroyEntry(ActiveEntry* entry);
  int AddTransactionToEntry(ActiveEntry* entry, Transaction* trans);
  void DoneWritingToEntry(ActiveEntry* entry, bool success);
  void DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans);
  void ConvertWriterToReader(ActiveEntry* entry);
  void RemovePendingTransaction(Transaction* trans);
  void ProcessPendingQueue(ActiveEntry* entry);


  // Events (called via PostTask) ---------------------------------------------

  void OnProcessPendingQueue(ActiveEntry* entry);


  // Variables ----------------------------------------------------------------

  // used when lazily constructing the disk_cache_
  std::wstring disk_cache_dir_;

  Mode mode_;

  scoped_ptr<HttpTransactionFactory> network_layer_;
  scoped_ptr<disk_cache::Backend> disk_cache_;

  // The set of active entries indexed by cache key
  ActiveEntriesMap active_entries_;

  // The set of doomed entries
  ActiveEntriesSet doomed_entries_;

  ScopedRunnableMethodFactory<HttpCache> task_factory_;

  bool in_memory_cache_;
  int cache_size_;

  typedef stdext::hash_map<std::string, int> PlaybackCacheMap;
  scoped_ptr<PlaybackCacheMap> playback_cache_map_;

  DISALLOW_EVIL_CONSTRUCTORS(HttpCache);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_H__
