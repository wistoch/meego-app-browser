// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

// TODO(cbentzel): Negotiate authentication protocol is not supported on Posix
// systems currently. These stubs make the main HTTP Authentication code bypass
// Negotiate without requiring conditional compilation.

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate() {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return false;
}

bool HttpAuthHandlerNegotiate::IsFinalRound() {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return false;
}

bool HttpAuthHandlerNegotiate::AllowDefaultCredentials() {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return false;
}

bool HttpAuthHandlerNegotiate::Init(std::string::const_iterator challenge_begin,
                                    std::string::const_iterator challenge_end) {
  return false;
}

int HttpAuthHandlerNegotiate::GenerateAuthToken(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return ERR_NOT_IMPLEMENTED;
}

int HttpAuthHandlerNegotiate::GenerateDefaultAuthToken(
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
