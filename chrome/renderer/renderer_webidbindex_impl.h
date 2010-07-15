// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBIndex.h"

class RendererWebIDBIndexImpl : public WebKit::WebIDBIndex {
 public:
  explicit RendererWebIDBIndexImpl(int32 idb_index_id);
  virtual ~RendererWebIDBIndexImpl();

  // WebKit::WebIDBIndex
  virtual WebKit::WebString name() const;
  virtual WebKit::WebString keyPath() const;
  virtual bool unique() const;

 private:
  int32 idb_index_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_
