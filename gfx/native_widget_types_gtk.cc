// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/native_widget_types.h"

#include "base/logging.h"
#include "gfx/gtk_native_view_id_manager.h"

namespace gfx {

NativeViewId IdFromNativeView(NativeView view) {
  return Singleton<GtkNativeViewManager>()->GetIdForWidget(view);
}

}  // namespace gfx
