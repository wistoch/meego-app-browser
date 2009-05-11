// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_

#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/string_piece.h"

#include <string>

class RenderView;

template<int kResourceId>
struct StringResourceTemplate {
  StringResourceTemplate()
      : resource(ResourceBundle::GetSharedInstance().GetRawDataResource(
            kResourceId).as_string()) {
  }
  std::string resource;
};

template<int kResourceId>
const char* GetStringResource() {
  return
      Singleton< StringResourceTemplate<kResourceId> >::get()->resource.c_str();
}

// Returns the active RenderView, based on which V8 context is active.  It is
// an error to call this when not in a V8 context.
RenderView* GetActiveRenderView();

#endif  // CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
