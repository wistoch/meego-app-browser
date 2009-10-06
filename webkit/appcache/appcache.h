// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_H_
#define WEBKIT_APPCACHE_APPCACHE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/manifest_parser.h"

namespace appcache {

class AppCacheGroup;
class AppCacheHost;
class AppCacheService;

// Set of cached resources for an application. A cache exists as long as a
// host is associated with it, the cache is in an appcache group or the
// cache is being created during an appcache upate.
class AppCache : public base::RefCounted<AppCache> {
 public:
  typedef std::map<GURL, AppCacheEntry> EntryMap;

  AppCache(AppCacheService *service, int64 cache_id);
  ~AppCache();

  int64 cache_id() const { return cache_id_; }

  AppCacheGroup* owning_group() const { return owning_group_; }
  void set_owning_group(AppCacheGroup* group) { owning_group_ = group; }

  bool is_complete() const { return is_complete_; }
  void set_complete(bool value) { is_complete_ = value; }

  AppCacheService* service() const { return service_; }

  // Adds a new entry. Entry must not already be in cache.
  void AddEntry(const GURL& url, const AppCacheEntry& entry);

  // Adds a new entry or modifies an existing entry by merging the types
  // of the new entry with the existing entry.
  void AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry);

  // Do not store the returned object as it could be deleted anytime.
  AppCacheEntry* GetEntry(const GURL& url);

  const EntryMap& entries() const { return entries_; }

  const std::set<AppCacheHost*>& associated_hosts() const {
    return associated_hosts_;
  }

  bool IsNewerThan(AppCache* cache) const {
    return update_time_ > cache->update_time_;
  }

  void set_update_time(base::TimeTicks ticks) {
    update_time_ = ticks;
  }

  // Initializes the cache with information in the manifest.
  // Do not use the manifest after this call.
  void InitializeWithManifest(Manifest* manifest);

 private:
  friend class AppCacheHost;
  friend class AppCacheUpdateJobTest;

  // Use AppCacheHost::AssociateCache() to manipulate host association.
  void AssociateHost(AppCacheHost* host) {
    associated_hosts_.insert(host);
  }
  void UnassociateHost(AppCacheHost* host);

  const int64 cache_id_;
  AppCacheGroup* owning_group_;
  std::set<AppCacheHost*> associated_hosts_;

  EntryMap entries_;    // contains entries of all types

  std::vector<FallbackNamespace> fallback_namespaces_;
  std::vector<GURL> online_whitelist_namespaces_;
  bool online_whitelist_all_;

  bool is_complete_;

  // when this cache was last updated
  base::TimeTicks update_time_;

  // to notify service when cache is deleted
  AppCacheService* service_;

  FRIEND_TEST(AppCacheTest, InitializeWithManifest);
  DISALLOW_COPY_AND_ASSIGN(AppCache);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_H_
