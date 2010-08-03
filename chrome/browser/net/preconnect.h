// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/net/url_info.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

class Preconnect : public net::CompletionCallback,
                   public base::RefCountedThreadSafe<Preconnect> {
 public:
  // Try to preconnect.  Typically motivated by OMNIBOX to reach search service.
  static void PreconnectOnUIThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation);

  // Try to preconnect.  Typically used by predictor when a subresource probably
  // needs a connection.
  static void PreconnectOnIOThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation);

  static void SetPreconnectDespiteProxy(bool status) {
    preconnect_despite_proxy_ = status;
  }

 private:
  friend class base::RefCountedThreadSafe<Preconnect>;

  explicit Preconnect(UrlInfo::ResolutionMotivation motivation)
      : motivation_(motivation) {
  }
  ~Preconnect();

  // Request actual connection.
  void Connect(const GURL& url);

  // IO Callback which whould be performed when the connection is established.
  virtual void RunWithParams(const Tuple1<int>& params);

  // Preconnections are currently conservative, and do nothing if there is a
  // chance that a proxy may be used.  This boolean allows proxy settings to
  // be ignored (presumably because a user knows that the proxy won't be doing
  // much work anway).
  static bool preconnect_despite_proxy_;

  // The handle holding the request. We need this so that we can mark the
  // request as speculative when an actual socket is bound to it.
  net::ClientSocketHandle handle_;

  // Generally either LEARNED_REFERAL_MOTIVATED or OMNIBOX_MOTIVATED to indicate
  // why we were trying to do a preconnection.
  const UrlInfo::ResolutionMotivation motivation_;


  DISALLOW_COPY_AND_ASSIGN(Preconnect);
};
}  // chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
