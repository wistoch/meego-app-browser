// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "gfx/canvas_skia_paint.h"
#include "views/widget/widget_gtk.h"

namespace views {

void RootView::OnPaint(GdkEventExpose* event) {
  WidgetGtk* widget = static_cast<WidgetGtk*>(GetWidget());
  if (!widget) {
    NOTREACHED();
    return;
  }
  gfx::Rect scheduled_dirty_rect = GetScheduledPaintRectConstrainedToSize();
  gfx::Rect expose_rect = gfx::Rect(event->area);
  gfx::CanvasSkiaPaint canvas(event);
  bool invoked_process_paint = false;
  if (!canvas.is_empty()) {
    canvas.set_composite_alpha(widget->is_transparent());
    SchedulePaint(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      ProcessPaint(&canvas);
      invoked_process_paint = true;
    }
  }

  if (invoked_process_paint && !scheduled_dirty_rect.IsEmpty() &&
      !expose_rect.Contains(scheduled_dirty_rect) && widget &&
      !widget->in_paint_now()) {
    // We're painting as the result of Gtk wanting us to paint (not from views)
    // and there was a region scheduled by views to be painted that is not
    // contained in the region gtk wants us to paint. As a result of the
    // ProcessPaint call above views no longer thinks it needs to be painted.
    // We have to invoke SchedulePaint here to be sure we end up painting the
    // region views wants to paint, otherwise we'll drop the views paint region
    // on the floor.
    //
    // NOTE: We don't expand the region to paint to include
    // scheduled_dirty_rect as that results in us drawing on top of any GTK
    // widgets that don't have a window. We have to schedule the paint through
    // GTK so that such widgets are painted.
    SchedulePaint(scheduled_dirty_rect, false);
  }
}

void RootView::StartDragForViewFromMouseEvent(
    View* view,
    const OSExchangeData& data,
    int operation) {
  // NOTE: view may be null.
  drag_view_ = view;
  static_cast<WidgetGtk*>(GetWidget())->DoDrag(data, operation);
  // If the view is removed during the drag operation, drag_view_ is set to
  // NULL.
  if (view && drag_view_ == view) {
    View* drag_view = drag_view_;
    drag_view_ = NULL;
    drag_view->OnDragDone();
  }
}

}
