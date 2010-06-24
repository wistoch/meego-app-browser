// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include "app/drag_drop_types.h"
#include "app/os_exchange_data.h"
#include "app/os_exchange_data_provider_win.h"
#include "base/base_drag_source.h"
#include "base/logging.h"
#include "gfx/canvas_2.h"
#include "gfx/canvas_skia.h"

namespace views {

void RootView::OnPaint(HWND hwnd) {
  gfx::Rect original_dirty_region = GetScheduledPaintRectConstrainedToSize();
  if (!original_dirty_region.IsEmpty()) {
    // Invoke InvalidateRect so that the dirty region of the window includes the
    // region we need to paint. If we didn't do this and the region didn't
    // include the dirty region, ProcessPaint would incorrectly mark everything
    // as clean. This can happen if a WM_PAINT is generated by the system before
    // the InvokeLater schedule by RootView is processed.
    RECT win_version = original_dirty_region.ToRECT();
    InvalidateRect(hwnd, &win_version, FALSE);
  }
  scoped_ptr<gfx::CanvasPaint2> canvas(
      gfx::CanvasPaint2::CreateCanvasPaint(hwnd));
  if (!canvas->IsValid()) {
    SchedulePaint(canvas->GetInvalidRect(), false);
    if (NeedsPainting(false))
      ProcessPaint(canvas->AsCanvas2()->AsCanvas());
  }
}

void RootView::StartDragForViewFromMouseEvent(
    View* view,
    const OSExchangeData& data,
    int operation) {
  // NOTE: view may be null.
  drag_view_ = view;
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD effects;
  DoDragDrop(OSExchangeDataProviderWin::GetIDataObject(data), drag_source,
             DragDropTypes::DragOperationToDropEffect(operation), &effects);
  // If the view is removed during the drag operation, drag_view_ is set to
  // NULL.
  if (view && drag_view_ == view) {
    View* drag_view = drag_view_;
    drag_view_ = NULL;
    drag_view->OnDragDone();
  }
}

}
