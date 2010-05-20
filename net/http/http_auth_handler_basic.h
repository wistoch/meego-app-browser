// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_

#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace net {

// Code for handling http basic authentication.
class HttpAuthHandlerBasic : public HttpAuthHandler {
 public:
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory();
    virtual ~Factory();

    virtual int CreateAuthHandler(HttpAuth::ChallengeTokenizer* challenge,
                                  HttpAuth::Target target,
                                  const GURL& origin,
                                  CreateReason reason,
                                  int digest_nonce_count,
                                  scoped_refptr<HttpAuthHandler>* handler);
  };

  virtual int GenerateAuthToken(const std::wstring& username,
                                const std::wstring& password,
                                const HttpRequestInfo*,
                                const ProxyInfo*,
                                std::string* auth_token);

  virtual int GenerateDefaultAuthToken(const HttpRequestInfo* request,
                                       const ProxyInfo* proxy,
                                       std::string* auth_token);

 protected:
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge);

 private:
  ~HttpAuthHandlerBasic() {}
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
