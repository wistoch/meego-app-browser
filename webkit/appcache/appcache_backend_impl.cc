// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_backend_impl.h"

#include "base/stl_util-inl.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/web_application_cache_host_impl.h"

namespace appcache {

AppCacheBackendImpl::~AppCacheBackendImpl() {
  STLDeleteValues(&hosts_);
  if (service_)
    service_->UnregisterBackend(this);
}

void AppCacheBackendImpl::Initialize(AppCacheService* service,
                                     AppCacheFrontend* frontend,
                                     int process_id) {
  DCHECK(!service_ && !frontend_ && frontend && service);
  service_ = service;
  frontend_ = frontend;
  process_id_ = process_id;
  service_->RegisterBackend(this);
}

bool AppCacheBackendImpl::RegisterHost(int id) {
  if (GetHost(id))
    return false;

  hosts_.insert(
      HostMap::value_type(id, new AppCacheHost(id, frontend_, service_)));
  return true;
}

bool AppCacheBackendImpl::UnregisterHost(int id) {
  HostMap::iterator found = hosts_.find(id);
  if (found == hosts_.end())
    return false;

  delete found->second;
  hosts_.erase(found);
  return true;
}

bool AppCacheBackendImpl::SelectCache(
    int host_id,
    const GURL& document_url,
    const int64 cache_document_was_loaded_from,
    const GURL& manifest_url) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->SelectCache(document_url, cache_document_was_loaded_from,
                    manifest_url);
  return true;
}

bool AppCacheBackendImpl::MarkAsForeignEntry(
    int host_id,
    const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->MarkAsForeignEntry(document_url, cache_document_was_loaded_from);
  return true;
}

bool AppCacheBackendImpl::GetStatusWithCallback(
    int host_id, GetStatusCallback* callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->GetStatusWithCallback(callback, callback_param);
  return true;
}

bool AppCacheBackendImpl::StartUpdateWithCallback(
    int host_id, StartUpdateCallback* callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->StartUpdateWithCallback(callback, callback_param);
  return true;
}

bool AppCacheBackendImpl::SwapCacheWithCallback(
    int host_id, SwapCacheCallback* callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->SwapCacheWithCallback(callback, callback_param);
  return true;
}

}  // namespace appcache
