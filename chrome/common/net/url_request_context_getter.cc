// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/url_request/url_request_context.h"

net::CookieStore* URLRequestContextGetter::GetCookieStore() {
  return GetURLRequestContext()->cookie_store();
}

URLRequestContextGetter::URLRequestContextGetter() : is_main_(false) {}

URLRequestContextGetter::~URLRequestContextGetter() {}

void URLRequestContextGetter::OnDestruct() {
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy =
      GetIOMessageLoopProxy();
  DCHECK(io_message_loop_proxy);
  if (io_message_loop_proxy) {
    if (io_message_loop_proxy->BelongsToCurrentThread()) {
      delete this;
    } else {
      io_message_loop_proxy->DeleteSoon(FROM_HERE, this);
    }
  }
  // If no IO message loop proxy was available, we will just leak memory.
  // This is also true if the IO thread is gone.
}
