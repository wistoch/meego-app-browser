// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "base/gfx/rect.h"
#include "base/keyboard_code_conversion_gtk.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/gtk_util.h"
#include "chrome/test/automation/automation_constants.h"

#if defined(TOOLKIT_VIEWS)
#include "views/view.h"
#include "views/widget/widget.h"
#endif

namespace {

guint32 EventTimeNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

class EventWaiter : public MessageLoopForUI::Observer {
 public:
  EventWaiter(Task* task, GdkEventType type, int count)
      : task_(task),
        type_(type),
        count_(count) {
    MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~EventWaiter() {
    MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) {
    if ((event->type == type_) && (--count_ == 0)) {
      // At the time we're invoked the event has not actually been processed.
      // Use PostTask to make sure the event has been processed before
      // notifying.
      // NOTE: if processing a message results in running a nested message
      // loop, then DidProcessEvent isn't immediately sent. As such, we do
      // the processing in WillProcessEvent rather than DidProcessEvent.
      MessageLoop::current()->PostTask(FROM_HERE, task_);
      delete this;
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
    // No-op.
  }

 private:
  // We pass ownership of task_ to MessageLoop when the current event is
  // received.
  Task* task_;
  GdkEventType type_;
  // The number of events of this type to wait for.
  int count_;
};

class ClickTask : public Task {
 public:
  ClickTask(ui_controls::MouseButton button, int state, Task* followup)
      : button_(button), state_(state), followup_(followup)  {
  }

  virtual ~ClickTask() {}

  virtual void Run() {
    if (followup_)
      ui_controls::SendMouseEventsNotifyWhenDone(button_, state_, followup_);
    else
      ui_controls::SendMouseEvents(button_, state_);
  }

 private:
  ui_controls::MouseButton button_;
  int state_;
  Task* followup_;
};

bool SendKeyEvent(GdkWindow* window, bool press, guint gdk_key, guint state) {
  GdkEvent* event = gdk_event_new(press ? GDK_KEY_PRESS : GDK_KEY_RELEASE);

  event->key.type = press ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  event->key.window = window;
  g_object_ref(event->key.window);
  event->key.send_event = false;
  event->key.time = EventTimeNow();

  event->key.state = state;
  event->key.keyval = gdk_key;

  GdkKeymapKey* keys;
  gint n_keys;
  if (!gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(),
                                         event->key.keyval, &keys, &n_keys)) {
    return false;
  }
  event->key.hardware_keycode = keys[0].keycode;
  event->key.group = keys[0].group;
  g_free(keys);

  gdk_event_put(event);
  // gdk_event_put appends a copy of the event.
  gdk_event_free(event);
  return true;
}

void FakeAMouseMotionEvent(gint x, gint y) {
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);

  event->motion.send_event = false;
  event->motion.time = EventTimeNow();

  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, we need to target all events at it regardless of
    // what widget the mouse is over.
    event->motion.window = grab_widget->window;
  } else {
    event->motion.window = gdk_window_at_pointer(&x, &y);
  }
  g_object_ref(event->motion.window);
  event->motion.x = x;
  event->motion.y = y;
  gint origin_x, origin_y;
  gdk_window_get_origin(event->motion.window, &origin_x, &origin_y);
  event->motion.x_root = x + origin_x;
  event->motion.y_root = y + origin_y;

  event->motion.device = gdk_device_get_core_pointer();
  event->type = GDK_MOTION_NOTIFY;

  gdk_event_put(event);
  gdk_event_free(event);
}

}  // namespace

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  base::KeyboardCode key, bool control, bool shift, bool alt) {
  GdkWindow* event_window = NULL;
  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, send all events to the grabbed widget.
    event_window = grab_widget->window;
  } else if (window) {
    event_window = GTK_WIDGET(window)->window;
  } else {
    // No target was specified. Send the events to the active toplevel.
    GList* windows = gtk_window_list_toplevels();
    for (GList* element = windows; element; element = g_list_next(element)) {
      GtkWindow* this_window = GTK_WINDOW(element->data);
      if (gtk_window_is_active(this_window)) {
        event_window = GTK_WIDGET(this_window)->window;
        break;
      }
    }
    g_list_free(windows);
  }
  if (!event_window) {
    NOTREACHED() << "Window not specified and none is active";
    return false;
  }

  bool rv = true;

  if (control)
    rv = rv && SendKeyEvent(event_window, true, GDK_Control_L, 0);

  if (shift) {
    rv = rv && SendKeyEvent(event_window, true, GDK_Shift_L,
                            control ? GDK_CONTROL_MASK : 0);
  }

  if (alt) {
    guint state = (control ? GDK_CONTROL_MASK : 0) |
                  (shift ? GDK_SHIFT_MASK : 0);
    rv = rv && SendKeyEvent(event_window, true, GDK_Alt_L, state);
  }

  // TODO(estade): handle other state flags besides control, shift, alt?
  // For example caps lock.
  guint state = (control ? GDK_CONTROL_MASK : 0) |
                (shift ? GDK_SHIFT_MASK : 0) |
                (alt ? GDK_MOD1_MASK : 0);

  guint gdk_key = base::GdkKeyCodeForWindowsKeyCode(key, shift);
  rv = rv && SendKeyEvent(event_window, true, gdk_key, state);
  rv = rv && SendKeyEvent(event_window, false, gdk_key, state);

  if (alt) {
    guint state = (control ? GDK_CONTROL_MASK : 0) |
                  (shift ? GDK_SHIFT_MASK : 0) | GDK_MOD1_MASK;
    rv = rv && SendKeyEvent(event_window, false, GDK_Alt_L, state);
  }

  if (shift) {
    rv = rv && SendKeyEvent(event_window, false, GDK_Shift_L,
                            (control ? GDK_CONTROL_MASK : 0) | GDK_SHIFT_MASK);
  }

  if (control)
    rv = rv && SendKeyEvent(event_window, false, GDK_Control_L,
                            GDK_CONTROL_MASK);

  return rv;
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                base::KeyboardCode key,
                                bool control, bool shift, bool alt,
                                Task* task) {
  int release_count = 1;
  if (control)
    release_count++;
  if (shift)
    release_count++;
  if (alt)
    release_count++;
  // This object will delete itself after running |task|.
  new EventWaiter(task, GDK_KEY_RELEASE, release_count);
  return SendKeyPress(window, key, control, shift, alt);
}

bool SendMouseMove(long x, long y) {
  gdk_display_warp_pointer(gdk_display_get_default(), gdk_screen_get_default(),
                           x, y);
  // Sometimes gdk_display_warp_pointer fails to send back any indication of
  // the move, even though it succesfully moves the server cursor. We fake it in
  // order to get drags to work.
  FakeAMouseMotionEvent(x, y);

  return true;
}

bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  bool rv = SendMouseMove(x, y);
  // We can't rely on any particular event signalling the completion of the
  // mouse move. Posting the task to the message loop hopefully guarantees
  // the pointer has moved before task is run (although it may not run it as
  // soon as it could).
  MessageLoop::current()->PostTask(FROM_HERE, task);
  return rv;
}

bool SendMouseEvents(MouseButton type, int state) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);

  event->button.send_event = false;
  event->button.time = EventTimeNow();

  gint x, y;
  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, we need to target all events at it regardless of
    // what widget the mouse is over.
    event->button.window = grab_widget->window;
    gdk_window_get_pointer(event->button.window, &x, &y, NULL);
  } else {
    event->button.window = gdk_window_at_pointer(&x, &y);
  }

  g_object_ref(event->button.window);
  event->button.x = x;
  event->button.y = y;
  gint origin_x, origin_y;
  gdk_window_get_origin(event->button.window, &origin_x, &origin_y);
  event->button.x_root = x + origin_x;
  event->button.y_root = y + origin_y;

  event->button.axes = NULL;
  GdkModifierType modifier;
  gdk_window_get_pointer(event->button.window, NULL, NULL, &modifier);
  event->button.state = modifier;
  event->button.button = type == LEFT ? 1 : (type == MIDDLE ? 2 : 3);
  event->button.device = gdk_device_get_core_pointer();

  event->button.type = GDK_BUTTON_PRESS;
  if (state & DOWN)
    gdk_event_put(event);

  // Also send a release event.
  GdkEvent* release_event = gdk_event_copy(event);
  release_event->button.type = GDK_BUTTON_RELEASE;
  release_event->button.time++;
  if (state & UP)
    gdk_event_put(release_event);

  gdk_event_free(event);
  gdk_event_free(release_event);

  return false;
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task) {
  bool rv = SendMouseEvents(type, state);
  GdkEventType wait_type;
  if (state & UP) {
    wait_type = GDK_BUTTON_RELEASE;
  } else {
    if (type == LEFT)
      wait_type = GDK_BUTTON_PRESS;
    else if (type == MIDDLE)
      wait_type = GDK_2BUTTON_PRESS;
    else
      wait_type = GDK_3BUTTON_PRESS;
  }
  new EventWaiter(task, wait_type, 1);
  return rv;
}

bool SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN);
}

#if defined(TOOLKIT_VIEWS)
void MoveMouseToCenterAndPress(views::View* view, MouseButton button,
                               int state, Task* task) {
  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  SendMouseMoveNotifyWhenDone(view_center.x(), view_center.y(),
                              new ClickTask(button, state, task));
}
#else
void MoveMouseToCenterAndPress(GtkWidget* widget,
                               MouseButton button,
                               int state,
                               Task* task) {
  gfx::Rect bounds = gtk_util::GetWidgetScreenBounds(widget);
  SendMouseMoveNotifyWhenDone(bounds.x() + bounds.width() / 2,
                              bounds.y() + bounds.height() / 2,
                              new ClickTask(button, state, task));
}
#endif

}  // namespace ui_controls
