// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>

#include "app/gfx/chrome_canvas.h"
#include "base/logging.h"
#include "skia/include/SkColor.h"

namespace views {

void RootView::UpdateCursor(const MouseEvent& e) {
  NOTIMPLEMENTED();
}

void RootView::OnPaint(GdkEventExpose* event) {
  ChromeCanvasPaint canvas(event);

  if (!canvas.isEmpty()) {
    SchedulePaint(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      ProcessPaint(&canvas);
    }
  }
}

}
