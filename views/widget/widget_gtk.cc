// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_gtk.h"

#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/window/window_gtk.h"

namespace views {

// Returns the position of a widget on screen.
static void GetWidgetPositionOnScreen(GtkWidget* widget, int* x, int *y) {
  GtkWidget* parent = widget;
  while (parent) {
    if (GTK_IS_WINDOW(widget)) {
      int window_x, window_y;
      gtk_window_get_position(GTK_WINDOW(widget), &window_x, &window_y);
      *x += window_x;
      *y += window_y;
      return;
    }
    // Not a window.
    *x += widget->allocation.x;
    *y += widget->allocation.y;
    parent = gtk_widget_get_parent(parent);
  }
}

WidgetGtk::WidgetGtk(Type type)
    : type_(type),
      widget_(NULL),
      child_widget_parent_(NULL),
      is_mouse_down_(false),
      last_mouse_event_was_move_(false) {
}

WidgetGtk::~WidgetGtk() {
  if (widget_) {
    // TODO: make sure this is right.
    gtk_widget_destroy(widget_);
    child_widget_parent_ = widget_ = NULL;
  }
  // MessageLoopForUI::current()->RemoveObserver(this);
}

void WidgetGtk::Init(const gfx::Rect& bounds,
                     bool has_own_focus_manager) {
  // Force creation of the RootView if it hasn't been created yet.
  GetRootView();

  // Make container here.
  CreateGtkWidget();

  // Make sure we receive our motion events.

  // We register everything on the parent of all widgets. At a minimum we need
  // painting to happen on the parent (otherwise painting doesn't work at all),
  // and similarly we need mouse release events on the parent as windows don't
  // get mouse releases.
  gtk_widget_add_events(child_widget_parent_,
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK);

  root_view_->OnWidgetCreated();

  // TODO(port): if(has_own_focus_manager) block

  SetRootViewForWidget(widget_, root_view_.get());

  // MessageLoopForUI::current()->AddObserver(this);

  g_signal_connect_after(G_OBJECT(child_widget_parent_), "size_allocate",
                         G_CALLBACK(CallSizeAllocate), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "expose_event",
                   G_CALLBACK(CallPaint), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "enter_notify_event",
                   G_CALLBACK(CallEnterNotify), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "leave_notify_event",
                   G_CALLBACK(CallLeaveNotify), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "motion_notify_event",
                   G_CALLBACK(CallMotionNotify), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "button_press_event",
                   G_CALLBACK(CallButtonPress), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "button_release_event",
                   G_CALLBACK(CallButtonRelease), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "focus_in_event",
                   G_CALLBACK(CallFocusIn), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "focus_out_event",
                   G_CALLBACK(CallFocusOut), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "key_press_event",
                   G_CALLBACK(CallKeyPress), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "key_release_event",
                   G_CALLBACK(CallKeyRelease), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "scroll_event",
                   G_CALLBACK(CallScroll), NULL);
  g_signal_connect(G_OBJECT(child_widget_parent_), "visibility_notify_event",
                   G_CALLBACK(CallVisibilityNotify), NULL);

  // TODO(erg): Ignore these signals for now because they're such a drag.
  //
  // g_signal_connect(G_OBJECT(widget_), "drag_motion",
  //                  G_CALLBACK(drag_motion_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_leave",
  //                  G_CALLBACK(drag_leave_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_drop",
  //                  G_CALLBACK(drag_drop_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_data_received",
  //                  G_CALLBACK(drag_data_received_event_cb), NULL);
}

void WidgetGtk::AddChild(GtkWidget* child) {
  gtk_container_add(GTK_CONTAINER(child_widget_parent_), child);
}

void WidgetGtk::RemoveChild(GtkWidget* child) {
  gtk_container_remove(GTK_CONTAINER(child_widget_parent_), child);
}

void WidgetGtk::PositionChild(GtkWidget* child, int x, int y, int w, int h) {
  GtkAllocation alloc = { x, y, w, h };
  gtk_widget_size_allocate(child, &alloc);
  gtk_fixed_move(GTK_FIXED(child_widget_parent_), child, x, y);
}

void WidgetGtk::SetContentsView(View* view) {
  DCHECK(view && widget_)
      << "Can't be called until after the HWND is created!";
  // The ContentsView must be set up _after_ the window is created so that its
  // Widget pointer is valid.
  root_view_->SetLayoutManager(new FillLayout);
  if (root_view_->GetChildViewCount() != 0)
    root_view_->RemoveAllChildViews(true);
  root_view_->AddChildView(view);

  DCHECK(widget_);  // Widget must have been created by now.

  OnSizeAllocate(widget_, &(widget_->allocation));
}

void WidgetGtk::GetBounds(gfx::Rect* out, bool including_frame) const {
  DCHECK(widget_);

  int x = 0, y = 0, w, h;
  if (GTK_IS_WINDOW(widget_)) {
    gtk_window_get_position(GTK_WINDOW(widget_), &x, &y);
    gtk_window_get_size(GTK_WINDOW(widget_), &w, &h);
  } else {
    // TODO: make sure this is right. Docs indicate gtk_window_get_position
    // returns a value useful to the window manager, which may not be the same
    // as the actual location on the screen.
    GetWidgetPositionOnScreen(widget_, &x, &y);
    w = widget_->allocation.width;
    h = widget_->allocation.height;
  }

  if (including_frame) {
    // TODO: Docs indicate it isn't possible to get at this value. We may need
    // to turn off all decorations so that the frame is always of a 0x0 size.
    NOTIMPLEMENTED();
  }

  return out->SetRect(x, y, w, h);
}

gfx::NativeView WidgetGtk::GetNativeView() const {
  return widget_;
}

void WidgetGtk::PaintNow(const gfx::Rect& update_rect) {
  gtk_widget_queue_draw_area(widget_, update_rect.x(), update_rect.y(),
                             update_rect.width(), update_rect.height());
}

RootView* WidgetGtk::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

Widget* WidgetGtk::GetRootWidget() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool WidgetGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(widget_);
}

bool WidgetGtk::IsActive() const {
  // If this only applies to windows, it shouldn't be in widget.
  DCHECK(GTK_IS_WINDOW(widget_));
  return gtk_window_is_active(GTK_WINDOW(widget_));
}

TooltipManager* WidgetGtk::GetTooltipManager() {
  NOTIMPLEMENTED();
  return NULL;
}

bool WidgetGtk::GetAccelerator(int cmd_id, Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

Window* WidgetGtk::GetWindow() {
  return GetWindowImpl(widget_);
}

const Window* WidgetGtk::GetWindow() const {
  return GetWindowImpl(widget_);
}

void WidgetGtk::CreateGtkWidget() {
  if (type_ == TYPE_CHILD) {
    child_widget_parent_ = widget_ = gtk_fixed_new();
    SetViewForNative(widget_, this);
  } else {
    widget_ = gtk_window_new(
        type_ == TYPE_WINDOW ? GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(widget_), false);
    // We'll take care of positioning our window.
    gtk_window_set_position(GTK_WINDOW(widget_), GTK_WIN_POS_NONE);
    SetWindowForNative(widget_, static_cast<WindowGtk*>(this));
    SetViewForNative(widget_, this);

    child_widget_parent_ = gtk_fixed_new();
    gtk_fixed_set_has_window(GTK_FIXED(child_widget_parent_), true);
    gtk_container_add(GTK_CONTAINER(widget_), child_widget_parent_);
    gtk_widget_show(child_widget_parent_);

    SetViewForNative(child_widget_parent_, this);
  }
  gtk_widget_show(widget_);
}

void WidgetGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  root_view_->SetBounds(0, 0, allocation->width, allocation->height);
  root_view_->Layout();
  root_view_->SchedulePaint();
}

gboolean WidgetGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  gfx::Point screen_loc(event->x_root, event->y_root);
  if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x() &&
      last_mouse_move_y_ == screen_loc.y()) {
    // Don't generate a mouse event for the same location as the last.
    return false;
  }
  last_mouse_move_x_ = screen_loc.x();
  last_mouse_move_y_ = screen_loc.y();
  last_mouse_event_was_move_ = true;
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                        event->x,
                        event->y,
                        Event::GetFlagsFromGdkState(event->state));
  root_view_->OnMouseMoved(mouse_move);
  return true;
}

gboolean WidgetGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  ProcessMousePressed(event);
  return true;
}

gboolean WidgetGtk::OnButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  ProcessMouseReleased(event);
  return true;
}

void WidgetGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  root_view_->OnPaint(event);
}

gboolean WidgetGtk::OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  // TODO(port): We may not actually need this message; it looks like
  // OnNotificationNotify() takes care of this case...
  return false;
}

gboolean WidgetGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  return true;
}

gboolean WidgetGtk::OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

gboolean WidgetGtk::OnKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

// static
WindowGtk* WidgetGtk::GetWindowForNative(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "chrome-window");
  return static_cast<WindowGtk*>(user_data);
}

// static
void WidgetGtk::SetWindowForNative(GtkWidget* widget, WindowGtk* window) {
  g_object_set_data(G_OBJECT(widget), "chrome-window", window);
}

RootView* WidgetGtk::CreateRootView() {
  return new RootView(this);
}

bool WidgetGtk::ProcessMousePressed(GdkEventButton* event) {
  last_mouse_event_was_move_ = false;
  // TODO: move this code into a common place. Also need support for
  // double/triple click.
  int flags = Event::GetFlagsFromGdkState(event->state);
  switch (event->button) {
    case 1:
      flags |= Event::EF_LEFT_BUTTON_DOWN;
      break;
    case 2:
      flags |= Event::EF_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      flags |= Event::EF_MIDDLE_BUTTON_DOWN;
      break;
    default:
      // We only deal with 1-3.
      break;
  }
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           event->x, event->y, flags);
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    // TODO(port): Enable this once I figure out what capture is.
    // if (!has_capture_) {
    //   SetCapture();
    //   has_capture_ = true;
    //   current_action_ = FA_FORWARDING;
    // }
    return true;
  }

  return false;
}

void WidgetGtk::ProcessMouseReleased(GdkEventButton* event) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED,
                      event->x, event->y,
                      Event::GetFlagsFromGdkState(event->state));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  //
  // TODO(port): Enable this once I figure out what capture is.
  // if (has_capture_ && ReleaseCaptureOnMouseReleased()) {
  //   has_capture_ = false;
  //   current_action_ = FA_NONE;
  //   ReleaseCapture();
  // }
  is_mouse_down_ = false;
  root_view_->OnMouseReleased(mouse_up, false);
}

// static
WidgetGtk* WidgetGtk::GetViewForNative(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "chrome-views");
  return static_cast<WidgetGtk*>(user_data);
}

// static
void WidgetGtk::SetViewForNative(GtkWidget* widget, WidgetGtk* view) {
  g_object_set_data(G_OBJECT(widget), "chrome-views", view);
}

// static
RootView* WidgetGtk::GetRootViewForWidget(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "root-view");
  return static_cast<RootView*>(user_data);
}

// static
void WidgetGtk::SetRootViewForWidget(GtkWidget* widget, RootView* root_view) {
  g_object_set_data(G_OBJECT(widget), "root-view", root_view);
}

// static
void WidgetGtk::CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return;

  widget_gtk->OnSizeAllocate(widget, allocation);
}

gboolean WidgetGtk::CallPaint(GtkWidget* widget, GdkEventExpose* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (widget_gtk)
    widget_gtk->OnPaint(widget, event);
  return false;  // False indicates other widgets should get the event as well.
}

gboolean WidgetGtk::CallEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnEnterNotify(widget, event);
}

gboolean WidgetGtk::CallLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnLeaveNotify(widget, event);
}

gboolean WidgetGtk::CallMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnMotionNotify(widget, event);
}

gboolean WidgetGtk::CallButtonPress(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonPress(widget, event);
}

gboolean WidgetGtk::CallButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonRelease(widget, event);
}

gboolean WidgetGtk::CallFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusIn(widget, event);
}

gboolean WidgetGtk::CallFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusOut(widget, event);
}

gboolean WidgetGtk::CallKeyPress(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyPress(widget, event);
}

gboolean WidgetGtk::CallKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyRelease(widget, event);
}

gboolean WidgetGtk::CallScroll(GtkWidget* widget, GdkEventScroll* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnScroll(widget, event);
}

gboolean WidgetGtk::CallVisibilityNotify(GtkWidget* widget,
                                         GdkEventVisibility* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnVisibilityNotify(widget, event);
}

// Returns the first ancestor of |widget| that is a window.
// static
Window* WidgetGtk::GetWindowImpl(GtkWidget* widget) {
  GtkWidget* parent = widget;
  while (parent) {
    WindowGtk* window = GetWindowForNative(widget);
    if (window)
      return window;
    parent = gtk_widget_get_parent(parent);
  }
  return NULL;
}

}  // namespace views
