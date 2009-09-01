// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_
#define WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_

#include "base/singleton.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

namespace appcache {

class AppCacheService;

// An interceptor to hijack requests and potentially service them out of
// the appcache.
class AppCacheInterceptor : public URLRequest::Interceptor {
 public:
  // Registers a singleton instance with the net library.
  // Should be called early in the IO thread prior to initiating requests.
  static void EnsureRegistered() {
    CHECK(instance());
  }

  // Must be called to make a request eligible for retrieval from an appcache.
  static void SetExtraRequestInfo(URLRequest* request,
                                  AppCacheService* service,
                                  int process_id,
                                  int host_id,
                                  ResourceType::Type resource_type);

  // May be called after response headers are complete to retrieve extra
  // info about the response.
  static void GetExtraResponseInfo(URLRequest* request,
                                   int64* cache_id,
                                   GURL* manifest_url);

 protected:
  // URLRequest::Interceptor overrides
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);
  virtual URLRequestJob* MaybeInterceptResponse(URLRequest* request);
  virtual URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                                const GURL& location);

 private:
  friend struct DefaultSingletonTraits<AppCacheInterceptor>;
  static AppCacheInterceptor* instance()  {
    return Singleton<AppCacheInterceptor>::get();
  }
  struct ExtraInfo;
  AppCacheInterceptor();
  virtual ~AppCacheInterceptor();
  DISALLOW_COPY_AND_ASSIGN(AppCacheInterceptor);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_
