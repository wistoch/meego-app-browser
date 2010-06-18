// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/appcache/appcache_frontend_proxy.h"

#include "chrome/common/render_messages.h"

void AppCacheFrontendProxy::OnCacheSelected(int host_id, int64 cache_id ,
                                            appcache::Status status) {
  sender_->Send(new AppCacheMsg_CacheSelected(host_id, cache_id, status));
}

void AppCacheFrontendProxy::OnStatusChanged(const std::vector<int>& host_ids,
                                            appcache::Status status) {
  sender_->Send(new AppCacheMsg_StatusChanged(host_ids, status));
}

void AppCacheFrontendProxy::OnEventRaised(const std::vector<int>& host_ids,
                                          appcache::EventID event_id) {
  DCHECK(event_id != appcache::PROGRESS_EVENT);  // See OnProgressEventRaised.
  sender_->Send(new AppCacheMsg_EventRaised(host_ids, event_id));
}

void AppCacheFrontendProxy::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url, int num_total, int num_complete) {
  sender_->Send(new AppCacheMsg_ProgressEventRaised(
      host_ids, url, num_total, num_complete));
}

void AppCacheFrontendProxy::OnLogMessage(int host_id,
                                         appcache::LogLevel log_level,
                                         const std::string& message) {
  sender_->Send(new AppCacheMsg_LogMessage(host_id, log_level, message));
}

void AppCacheFrontendProxy::OnContentBlocked(int host_id) {
  sender_->Send(new AppCacheMsg_ContentBlocked(host_id));
}
