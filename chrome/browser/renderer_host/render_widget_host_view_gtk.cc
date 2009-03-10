// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo/cairo.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/x11_util.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "skia/ext/platform_device_linux.h"
#include "webkit/glue/webinputevent.h"

namespace {

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_drawing_area_new();
    gtk_widget_set_double_buffered(widget, FALSE);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "configure-event",
                     G_CALLBACK(ConfigureEvent), host_view);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(FocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(FocusOut), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    g_signal_connect(widget, "scroll-event",
                     G_CALLBACK(MouseScrollEvent), host_view);

    return widget;
  }

 private:
  static gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                                 RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->WasResized();
    return FALSE;
  }

  static gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                              RenderWidgetHostViewGtk* host_view) {
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       RenderWidgetHostViewGtk* host_view) {
    NativeWebKeyboardEvent wke(event);
    host_view->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);
    return FALSE;
  }

  static gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus,
                          RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->Focus();
    return FALSE;
  }

  static gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus,
                           RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->Blur();
    return FALSE;
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    WebMouseEvent wme(event);
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(wme);

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    WebMouseEvent wme(event);
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(wme);
    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    WebMouseWheelEvent wmwe(event);
    host_view->GetRenderWidgetHost()->ForwardWheelEvent(wmwe);
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

}  // namespace

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host) {
  host_->set_view(this);
  view_ = RenderWidgetHostViewGtkWidget::CreateNewWidget(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::WasHidden() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

gfx::NativeView RenderWidgetHostViewGtk::GetPluginNativeView() {
  // TODO(port): We need to pass some widget pointer out here because the
  // renderer echos it back to us when it asks for GetScreenInfo. However, we
  // should probably be passing the top-level window or some such instead.
  return view_;
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  if (plugin_window_moves.empty())
    return;

  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Focus() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Blur() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGtk::Show() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Hide() {
  NOTIMPLEMENTED();
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  return gfx::Rect(view_->allocation.x, view_->allocation.y,
                   view_->allocation.width, view_->allocation.height);
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // TODO(port): some of this logic may need moving to UpdateCursorIfOverSelf at
  // some point.
  GdkCursorType current_cursor_type = current_cursor_.GetCursorType();
  GdkCursorType new_cursor_type = cursor.GetCursorType();
  current_cursor_ = cursor;
  GdkCursor* gdk_cursor;
  if (new_cursor_type == GDK_CURSOR_IS_PIXMAP) {
    // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
    // that calling gdk_window_set_cursor repeatedly is expensive.  We should
    // avoid it here where possible.
    gdk_cursor = current_cursor_.GetCustomCursor();
  } else {
    // Optimize the common case, where the cursor hasn't changed.
    // However, we can switch between different pixmaps, so only on the
    // non-pixmap branch.
    if (new_cursor_type == current_cursor_type)
      return;
    gdk_cursor = gdk_cursor_new(new_cursor_type);
  }
  gdk_window_set_cursor(view_->window, gdk_cursor);
  // The window now owns the cursor.
  gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::UpdateCursorIfOverSelf() {
  // Windows uses this to show the resizer arrow if the mouse is over the
  // bottom-right corner.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  // Windows tracks loading whether it's loading to switch the cursor
  // out for the arrow+hourglass one.  We don't have such a cursor, so we just
  // ignore this.
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::RenderViewGone() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Destroy() {
  // We need to disconnect ourselves from our parent widget at this time; this
  // does the right thing, automatically removing ourselves from our parent
  // container.
  gtk_widget_destroy(view_);
  view_ = NULL;
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text.empty()) {
    gtk_widget_set_has_tooltip(view_, FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_, WideToUTF8(tooltip_text).c_str());
  }
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  Display* display = x11_util::GetXDisplay();
  void* visual = x11_util::GetVisualFromGtkWidget(view_);
  XID root_window = x11_util::GetX11RootWindow();
  bool use_render = x11_util::QueryRenderSupport(display);
  bool use_shared_memory = x11_util::QuerySharedMemorySupport(display);
  int depth = gtk_widget_get_visual(view_)->depth;

  return new BackingStore(size, display, depth, visual, root_window,
                          use_render, use_shared_memory);
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  BackingStore* backing_store = host_->GetBackingStore();

  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    GdkWindow* window = view_->window;
    if (window) {
      backing_store->ShowRect(
          damage_rect, x11_util::GetX11WindowFromGtkWidget(view_));
    }
  } else {
    NOTIMPLEMENTED();
  }
}
