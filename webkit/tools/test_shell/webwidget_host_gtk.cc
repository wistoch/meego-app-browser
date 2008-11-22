// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "skia/ext/platform_canvas_linux.h"
#include "skia/ext/platform_device_linux.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget.h"

namespace {

// In response to an invalidation, we call into WebKit to do layout. On
// Windows, WM_PAINT is a virtual message so any extra invalidates that come up
// while it's doing layout are implicitly swallowed as soon as we actually do
// drawing via BeginPaint.
//
// Though GTK does know how to collapse multiple paint requests, it won't erase
// paint requests from the future when we start drawing.  To avoid an infinite
// cycle of repaints, we track whether we're currently handling a redraw, and
// during that if we get told by WebKit that a region has become invalid, we
// still add that region to the local dirty rect but *don't* enqueue yet
// another "do a paint" message.
bool handling_expose = false;

// -----------------------------------------------------------------------------
// Callback functions to proxy to host...

gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                        WebWidgetHost* host) {
  host->Resize(gfx::Size(config->width, config->height));
  return FALSE;
}

gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                     WebWidgetHost* host) {
  // See comments above about what handling_expose is for.
  handling_expose = true;
  gfx::Rect rect(expose->area);
  host->UpdatePaintRect(rect);
  host->Paint();
  handling_expose = false;
  return FALSE;
}

gboolean DestroyEvent(GtkWidget* widget, GdkEvent* event,
                      WebWidgetHost* host) {
  host->WindowDestroyed();
  return FALSE;
}

gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                              WebWidgetHost* host) {
  WebKeyboardEvent wke(event);
  host->webwidget()->HandleInputEvent(&wke);

  // The WebKeyboardEvent model, when holding down a key, is:
  //   KEY_DOWN, CHAR, (repeated CHAR as key repeats,) KEY_UP
  // The GDK model for the same sequence is just:
  //   KEY_PRESS, (repeated KEY_PRESS as key repeats,) KEY_RELEASE
  // So we must simulate a CHAR event for every key press.
  if (event->type == GDK_KEY_PRESS) {
    wke.type = WebKeyboardEvent::CHAR;
    host->webwidget()->HandleInputEvent(&wke);
  }

  return FALSE;
}

// This signal is called when arrow keys or tab is pressed.  If we return true,
// we prevent focus from being moved to another widget.  If we want to allow
// focus to be moved outside of web contents, we need to implement
// WebViewDelegate::TakeFocus in the test webview delegate.
gboolean FocusMove(GtkWidget* widget, GdkEventFocus* focus,
                   WebWidgetHost* host) {
  return TRUE;
}

gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus,
                 WebWidgetHost* host) {
  host->webwidget()->SetFocus(true);
  return FALSE;
}

gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus,
                  WebWidgetHost* host) {
  host->webwidget()->SetFocus(false);
  return FALSE;
}

gboolean ButtonPressReleaseEvent(GtkWidget* widget, GdkEventButton* event,
                                 WebWidgetHost* host) {
  WebMouseEvent wme(event);
  host->webwidget()->HandleInputEvent(&wme);
  return FALSE;
}

gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                        WebWidgetHost* host) {
  WebMouseEvent wme(event);
  host->webwidget()->HandleInputEvent(&wme);
  return FALSE;
}

gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                          WebWidgetHost* host) {
  WebMouseWheelEvent wmwe(event);
  host->webwidget()->HandleInputEvent(&wmwe);
  return FALSE;
}

}  // anonymous namespace

// -----------------------------------------------------------------------------

gfx::WindowHandle WebWidgetHost::CreateWindow(gfx::WindowHandle box,
                                              void* host) {
  GtkWidget* widget = gtk_drawing_area_new();
  gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);

  gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON_PRESS_MASK |
                                GDK_BUTTON_RELEASE_MASK |
                                GDK_KEY_PRESS_MASK |
                                GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
  g_signal_connect(widget, "configure-event", G_CALLBACK(ConfigureEvent), host);
  g_signal_connect(widget, "expose-event", G_CALLBACK(ExposeEvent), host);
  g_signal_connect(widget, "destroy-event", G_CALLBACK(DestroyEvent), host);
  g_signal_connect(widget, "key-press-event", G_CALLBACK(KeyPressReleaseEvent),
                   host);
  g_signal_connect(widget, "key-release-event",
                   G_CALLBACK(KeyPressReleaseEvent), host);
  g_signal_connect(widget, "focus", G_CALLBACK(FocusMove), host);
  g_signal_connect(widget, "focus-in-event", G_CALLBACK(FocusIn), host);
  g_signal_connect(widget, "focus-out-event", G_CALLBACK(FocusOut), host);
  g_signal_connect(widget, "button-press-event",
                   G_CALLBACK(ButtonPressReleaseEvent), host);
  g_signal_connect(widget, "button-release-event",
                   G_CALLBACK(ButtonPressReleaseEvent), host);
  g_signal_connect(widget, "motion-notify-event", G_CALLBACK(MouseMoveEvent),
                   host);
  g_signal_connect(widget, "scroll-event", G_CALLBACK(MouseScrollEvent),
                   host);

  return widget;
}

WebWidgetHost* WebWidgetHost::Create(gfx::WindowHandle box,
                                     WebWidgetDelegate* delegate) {
  WebWidgetHost* host = new WebWidgetHost();
  host->view_ = CreateWindow(box, host);
  host->webwidget_ = WebWidget::Create(delegate);

  return host;
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
  paint_rect_ = paint_rect_.Union(rect);
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";

  UpdatePaintRect(damaged_rect);

  if (!handling_expose) {
    gtk_widget_queue_draw_area(GTK_WIDGET(view_), damaged_rect.x(),
        damaged_rect.y(), damaged_rect.width(), damaged_rect.height());
  }
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  // This is used for optimizing painting when the renderer is scrolled. We're
  // currently not doing any optimizations so just invalidate the region.
  DidInvalidateRect(clip_rect);
}

WebWidgetHost* FromWindow(gfx::WindowHandle view) {
  const gpointer p = g_object_get_data(G_OBJECT(view), "webwidgethost");
  return (WebWidgetHost* ) p;
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      webwidget_(NULL),
      scroll_dx_(0),
      scroll_dy_(0),
      track_mouse_leave_(false) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  webwidget_->Close();
  webwidget_->Release();
}

void WebWidgetHost::Resize(const gfx::Size &newsize) {
  // The pixel buffer backing us is now the wrong size
  canvas_.reset();

  webwidget_->Resize(gfx::Size(newsize.width(), newsize.height()));
}

void WebWidgetHost::Paint() {
  int width = view_->allocation.width;
  int height = view_->allocation.height;
  gfx::Rect client_rect(width, height);

  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
    paint_rect_ = client_rect;
    canvas_.reset(new gfx::PlatformCanvas(width, height, true));
    if (!canvas_.get()) {
      // memory allocation failed, we can't paint.
      LOG(ERROR) << "Failed to allocate memory for " << width << "x" << height;
      return;
    }
  }

  // This may result in more invalidation
  webwidget_->Layout();

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  // BitBlit to the X server
  gfx::PlatformDeviceLinux &platdev = canvas_->getTopPlatformDevice();
  gfx::BitmapPlatformDeviceLinux* const bitdev =
    static_cast<gfx::BitmapPlatformDeviceLinux* >(&platdev);
  
  cairo_t* cairo_drawable = gdk_cairo_create(view_->window);
  cairo_set_source_surface(cairo_drawable, bitdev->surface(), 0, 0);
  cairo_paint(cairo_drawable);
  cairo_destroy(cairo_drawable);
}

void WebWidgetHost::ResetScrollRect() {
  // This method is only needed for optimized scroll painting, which we don't
  // care about in the test shell, yet.
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
  set_painting(true);
  webwidget_->Paint(canvas_.get(), rect);
  set_painting(false);
}

// -----------------------------------------------------------------------------
// This is called when the GTK window is destroyed. In the Windows code this
// deletes this object. Since it's only test_shell it probably doesn't matter
// that much.
// -----------------------------------------------------------------------------
void WebWidgetHost::WindowDestroyed() {
  delete this;
}
