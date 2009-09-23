// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_interfaces.h"

#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/api/public/WebApplicationCacheHost.h"

using WebKit::WebApplicationCacheHost;

namespace appcache {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kHttpGETMethod[] = "GET";
const char kHttpHEADMethod[] = "HEAD";

bool IsSchemeSupported(const GURL& url) {
  bool supported = url.SchemeIs(kHttpScheme) || url.SchemeIs(kHttpsScheme);
#ifndef NDEBUG
  supported |= url.SchemeIsFile();
#endif
  return supported;
}

bool IsMethodSupported(const std::string& method) {
  return (method == kHttpGETMethod) || (method == kHttpHEADMethod);
}

bool IsSchemeAndMethodSupported(const URLRequest* request) {
  return IsSchemeSupported(request->url()) &&
         IsMethodSupported(request->method());
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api
COMPILE_ASSERT((int)WebApplicationCacheHost::Uncached ==
               (int)UNCACHED, Uncached);
COMPILE_ASSERT((int)WebApplicationCacheHost::Idle ==
               (int)IDLE, Idle);
COMPILE_ASSERT((int)WebApplicationCacheHost::Checking ==
               (int)CHECKING, Checking);
COMPILE_ASSERT((int)WebApplicationCacheHost::Downloading ==
               (int)DOWNLOADING, Downloading);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReady ==
               (int)UPDATE_READY, UpdateReady);
COMPILE_ASSERT((int)WebApplicationCacheHost::Obsolete ==
               (int)OBSOLETE, Obsolete);
COMPILE_ASSERT((int)WebApplicationCacheHost::CheckingEvent ==
               (int)CHECKING_EVENT, CheckingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ErrorEvent ==
               (int)ERROR_EVENT, ErrorEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::NoUpdateEvent ==
               (int)NO_UPDATE_EVENT, NoUpdateEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::DownloadingEvent ==
               (int)DOWNLOADING_EVENT, DownloadingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ProgressEvent ==
               (int)PROGRESS_EVENT, ProgressEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReadyEvent ==
               (int)UPDATE_READY_EVENT, UpdateReadyEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::CachedEvent ==
               (int)CACHED_EVENT, CachedEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ObsoleteEvent ==
               (int)OBSOLETE_EVENT, ObsoleteEvent);

}  // namespace
