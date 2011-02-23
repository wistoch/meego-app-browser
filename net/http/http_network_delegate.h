// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_DELEGATE_H_
#define NET_HTTP_HTTP_NETWORK_DELEGATE_H_
#pragma once

namespace net {

class HttpRequestHeaders;
class URLRequest;

class HttpNetworkDelegate {
 public:
  virtual ~HttpNetworkDelegate() {}

  // Called before a request is sent.
  virtual void OnBeforeURLRequest(URLRequest* request) = 0;

  // Called right before the HTTP headers are sent.  Allows the delegate to
  // read/write |headers| before they get sent out.
  virtual void OnSendHttpRequest(HttpRequestHeaders* headers) = 0;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  virtual void OnResponseStarted(URLRequest* request) = 0;

  // This corresponds to URLRequestDelegate::OnReadCompleted.
  virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_DELEGATE_H_
