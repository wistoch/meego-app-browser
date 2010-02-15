// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_negotiate.h"
#include "net/http/http_auth_handler_ntlm.h"

namespace net {

int HttpAuthHandlerFactory::CreateAuthHandlerFromString(
    const std::string& challenge,
    HttpAuth::Target target,
    const GURL& origin,
    scoped_refptr<HttpAuthHandler>* handler) {
  HttpAuth::ChallengeTokenizer props(challenge.begin(), challenge.end());
  return CreateAuthHandler(&props, target, origin, handler);
}

// static
HttpAuthHandlerFactory* HttpAuthHandlerFactory::CreateDefault() {
  HttpAuthHandlerRegistryFactory* registry_factory =
      new HttpAuthHandlerRegistryFactory();
  registry_factory->RegisterSchemeFactory(
      "basic", new HttpAuthHandlerBasic::Factory());
  registry_factory->RegisterSchemeFactory(
      "digest", new HttpAuthHandlerDigest::Factory());
  registry_factory->RegisterSchemeFactory(
      "negotiate", new HttpAuthHandlerNegotiate::Factory());
  registry_factory->RegisterSchemeFactory(
      "ntlm", new HttpAuthHandlerNTLM::Factory());
  return registry_factory;
}

HttpAuthHandlerRegistryFactory::HttpAuthHandlerRegistryFactory() {
}

HttpAuthHandlerRegistryFactory::~HttpAuthHandlerRegistryFactory() {
  STLDeleteContainerPairSecondPointers(factory_map_.begin(),
                                       factory_map_.end());
}

void HttpAuthHandlerRegistryFactory::RegisterSchemeFactory(
    const std::string& scheme,
    HttpAuthHandlerFactory* factory) {
  std::string lower_scheme = StringToLowerASCII(scheme);
  FactoryMap::iterator it = factory_map_.find(lower_scheme);
  if (it != factory_map_.end()) {
    delete it->second;
  }
  if (factory)
    factory_map_[lower_scheme] = factory;
  else
    factory_map_.erase(it);
}

int HttpAuthHandlerRegistryFactory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    scoped_refptr<HttpAuthHandler>* handler) {
  if (!challenge->valid()) {
    *handler = NULL;
    return ERR_INVALID_RESPONSE;
  }
  std::string lower_scheme = StringToLowerASCII(challenge->scheme());
  FactoryMap::iterator it = factory_map_.find(lower_scheme);
  if (it == factory_map_.end()) {
    *handler = NULL;
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
  DCHECK(it->second);
  return it->second->CreateAuthHandler(challenge, target, origin, handler);
}

}  // namespace net
