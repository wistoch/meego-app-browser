// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

// static
const void* const BlacklistRequestInfo::kURLRequestDataKey = 0;

BlacklistRequestInfo::BlacklistRequestInfo(const GURL& url,
                                           ResourceType::Type resource_type,
                                           BlacklistManager* blacklist_manager)
    : url_(url),
      resource_type_(resource_type),
      blacklist_manager_(blacklist_manager) {
}

BlacklistRequestInfo::~BlacklistRequestInfo() {
}

// static
BlacklistRequestInfo* BlacklistRequestInfo::FromURLRequest(
    const URLRequest* request) {
  URLRequest::UserData* user_data =
      request->GetUserData(&kURLRequestDataKey);
  return (user_data ? static_cast<BlacklistRequestInfo*>(user_data) : NULL);
}
