// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store_manager.h"

#include "base/sys_info.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/chrome_constants.h"

namespace {

typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> BackingStoreCache;
static BackingStoreCache* cache = NULL;

// Returns the size of the backing store cache.
static int GetBackingStoreCacheSize() {
  // This uses a similar approach to GetMaxRendererProcessCount.  The goal
  // is to reduce memory pressure and swapping on low-resource machines.
  static const int kMaxDibCountByRamTier[] = {
    2,  // less than 256MB
    3,  // 256MB
    4,  // 512MB
    5   // 768MB and above
  };

  static int max_size = kMaxDibCountByRamTier[
          std::min(base::SysInfo::AmountOfPhysicalMemoryMB() / 256,
                   static_cast<int>(arraysize(kMaxDibCountByRamTier)) - 1)];
  return max_size;
}

// Creates the backing store for the host based on the dimensions passed in.
// Removes the existing backing store if there is one.
BackingStore* CreateBackingStore(RenderWidgetHost* host,
                                 const gfx::Size& backing_store_size) {
  BackingStoreManager::RemoveBackingStore(host);

  BackingStore* backing_store = host->AllocBackingStore(backing_store_size);
  int backing_store_cache_size = GetBackingStoreCacheSize();
  if (backing_store_cache_size > 0) {
    if (!cache)
      cache = new BackingStoreCache(backing_store_cache_size);
    cache->Put(host, backing_store);
  }
  return backing_store;
}

}  // namespace

// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.
int BackingStore::kMaxBitmapLengthAllowed = 23170;

// BackingStoreManager ---------------------------------------------------------

// static
BackingStore* BackingStoreManager::GetBackingStore(
    RenderWidgetHost* host,
    const gfx::Size& desired_size) {
  BackingStore* backing_store = Lookup(host);
  if (backing_store) {
    // If we already have a backing store, then make sure it is the correct
    // size.
    if (backing_store->size() == desired_size)
      return backing_store;
    backing_store = NULL;
  }

  return backing_store;
}

// static
BackingStore* BackingStoreManager::PrepareBackingStore(
    RenderWidgetHost* host,
    const gfx::Size& backing_store_size,
    base::ProcessHandle process_handle,
    TransportDIB* bitmap,
    const gfx::Rect& bitmap_rect,
    const gfx::Rect& paint_rect,
    bool* needs_full_paint) {
  BackingStore* backing_store = GetBackingStore(host, backing_store_size);
  if (!backing_store) {
    // We need to get Webkit to generate a new paint here, as we
    // don't have a previous snapshot.
    if (paint_rect.size() != backing_store_size ||
        paint_rect.x() != 0 || paint_rect.y() != 0) {
      DCHECK(needs_full_paint != NULL);
      *needs_full_paint = true;
    }
    backing_store = CreateBackingStore(host, backing_store_size);
  }

  DCHECK(backing_store != NULL);
  backing_store->PaintRect(process_handle, bitmap, bitmap_rect, paint_rect);
  return backing_store;
}

// static
BackingStore* BackingStoreManager::Lookup(RenderWidgetHost* host) {
  if (cache) {
    BackingStoreCache::iterator it = cache->Peek(host);
    if (it != cache->end())
      return it->second;
  }
  return NULL;
}

// static
void BackingStoreManager::RemoveBackingStore(RenderWidgetHost* host) {
  if (!cache)
    return;

  BackingStoreCache::iterator it = cache->Peek(host);
  if (it == cache->end())
    return;

  cache->Erase(it);

  if (cache->empty()) {
    delete cache;
    cache = NULL;
  }
}
