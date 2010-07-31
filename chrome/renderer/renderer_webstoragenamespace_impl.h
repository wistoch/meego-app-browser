// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/common/dom_storage_common.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageNamespace.h"

class RendererWebStorageNamespaceImpl : public WebKit::WebStorageNamespace {
 public:
  explicit RendererWebStorageNamespaceImpl(DOMStorageType storage_type);
  RendererWebStorageNamespaceImpl(DOMStorageType storage_type,
                                  int64 namespace_id);

  // See WebStorageNamespace.h for documentation on these functions.
  virtual ~RendererWebStorageNamespaceImpl();
  virtual WebKit::WebStorageArea* createStorageArea(
      const WebKit::WebString& origin);
  virtual WebKit::WebStorageNamespace* copy();
  virtual void close();

 private:
  // Used during lazy initialization of namespace_id_.
  const DOMStorageType storage_type_;

  // Our namespace ID.
  int64 namespace_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_
