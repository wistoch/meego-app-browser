// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webstoragearea_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "webkit/api/public/WebString.h"

using WebKit::WebString;

RendererWebStorageAreaImpl::RendererWebStorageAreaImpl(
    int64 namespace_id,
    const WebString& origin)
    : namespace_id_(namespace_id),
      origin_(origin),
      storage_area_id_(kUninitializedStorageAreaId),
      lock_held_(false),
      bytes_left_in_quota_(0) {
}

RendererWebStorageAreaImpl::~RendererWebStorageAreaImpl() {
}

void RendererWebStorageAreaImpl::lock(bool& invalidate_cache,
                                      size_t& bytes_left_in_quota) {
  EnsureInitializedAndLocked();
}

void RendererWebStorageAreaImpl::unlock() {
  if (storage_area_id_ != kUninitializedStorageAreaId) {
    RenderThread::current()->Send(
        new ViewHostMsg_DOMStorageUnlock(storage_area_id_));
  }
  lock_held_ = false;
}

unsigned RendererWebStorageAreaImpl::length() {
  EnsureInitializedAndLocked();
  // Right now this is always sync.  We could cache it, but I can't think of
  // too many use cases where you'd repeatedly look up length() and not be
  // doing so many key() lookups that the length() calls are the problem.
  unsigned length;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageLength(storage_area_id_, &length));
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  EnsureInitializedAndLocked();
  // Right now this is always sync.  We may want to optimize this by fetching
  // chunks of keys rather than single keys (and flushing the cache on every
  // mutation of the storage area) since this will most often be used to fetch
  // all the keys at once.
  NullableString16 key;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageKey(storage_area_id_, index, &key));
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& webkit_key) {
  EnsureInitializedAndLocked();
  string16 key = webkit_key;

  // Return from our cache if possible.
  CacheMap::const_iterator iterator = cached_items_.find(key);
  if (iterator != cached_items_.end())
    return iterator->second;
  if (cached_invalid_items_.find(key) != cached_invalid_items_.end())
    return WebString();  // Return a "null" string.

  // The item is not in the cache, so we must do a sync IPC.  Afterwards,
  // add it to the cache.
  NullableString16 raw_value;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageGetItem(storage_area_id_, key, &raw_value));
  WebString value = raw_value;  // Only do the conversion once.
  SetCache(key, value);
  return value;
}

void RendererWebStorageAreaImpl::setItem(const WebString& key,
                                         const WebString& value,
                                         bool& quota_exception) {
  EnsureInitializedAndLocked();
  quota_exception = !UpdateQuota(key, value);
  if (quota_exception)
    return;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageSetItem(storage_area_id_, key, value));
  SetCache(key, value);
}

void RendererWebStorageAreaImpl::removeItem(const WebString& key) {
  EnsureInitializedAndLocked();
  bool update_succeeded = UpdateQuota(key, WebString());
  DCHECK(update_succeeded);
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageRemoveItem(storage_area_id_, key));
  SetCache(key, WebString());
}

void RendererWebStorageAreaImpl::clear() {
  EnsureInitializedAndLocked();
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageClear(storage_area_id_,
                                      &bytes_left_in_quota_));
  // A possible optimization is a flag that says our cache is 100% complete.
  // This could be set here, and then future gets would never require IPC.
  cached_items_.clear();
  cached_invalid_items_.clear();
}

void RendererWebStorageAreaImpl::EnsureInitializedAndLocked() {
  if (storage_area_id_ == kUninitializedStorageAreaId) {
    RenderThread::current()->Send(
        new ViewHostMsg_DOMStorageStorageAreaId(namespace_id_, origin_,
                                                &storage_area_id_));
  }

  if (lock_held_)
    return;

  bool invalidate_cache;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageLock(storage_area_id_, &invalidate_cache,
                                     &bytes_left_in_quota_));
  lock_held_ = true;
  if (invalidate_cache) {
    cached_items_.clear();
    cached_invalid_items_.clear();
  }
}

bool RendererWebStorageAreaImpl::UpdateQuota(const WebString& key,
                                             const WebString& value) {
  // TODO(jorlow): Remove once the bytes_left_in_quota values we're getting
  //               are accurate.
  return true;

  size_t existing_bytes = getItem(key).length();
  size_t new_bytes = value.length();

  if (existing_bytes < new_bytes) {
    size_t delta = new_bytes - existing_bytes;
    if (delta > bytes_left_in_quota_)
      return false;
    bytes_left_in_quota_ -= delta;
  } else {
    size_t delta = existing_bytes - new_bytes;
    DCHECK(delta + bytes_left_in_quota_ >= bytes_left_in_quota_);
    bytes_left_in_quota_ += delta;
  }
  return true;
}

void RendererWebStorageAreaImpl::SetCache(const string16& key,
                                          const WebString& value) {
  cached_items_.erase(key);
  cached_invalid_items_.erase(key);

  if (!value.isNull())
    cached_items_[key] = value;
  else
    cached_invalid_items_.insert(key);
}
