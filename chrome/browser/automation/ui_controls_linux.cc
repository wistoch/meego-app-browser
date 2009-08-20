// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/test/automation/automation_constants.h"

namespace {

guint32 EventTimeNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

class EventWaiter : public MessageLoopForUI::Observer {
 public:
  EventWaiter(Task* task, GdkEventType type) : task_(task), type_(type) {
    MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~EventWaiter() {
    MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) {
    // No-op.
  }

  virtual void DidProcessEvent(GdkEvent* event) {
    if (event->any.type == type_) {
      task_->Run();
      delete this;
    }
  }

 private:
  scoped_ptr<Task> task_;
  GdkEventType type_;
};

}  // namespace

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  wchar_t key, bool control, bool shift, bool alt) {
  // TODO(estade): send a release as well?
  GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);

  event->key.type = GDK_KEY_PRESS;
  event->key.window = GTK_WIDGET(window)->window;
  g_object_ref(event->key.window);
  event->key.send_event = false;
  event->key.time = EventTimeNow();

  // TODO(estade): handle other state flags besides control, shift, alt?
  // For example caps lock.
  event->key.state = (control ? GDK_CONTROL_MASK : 0) |
                     (shift ? GDK_SHIFT_MASK : 0) |
                     (alt ? GDK_MOD1_MASK : 0);
  event->key.keyval = key;
  // TODO(estade): fill in the string?

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

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window, wchar_t key,
                                bool control, bool shift,
                                bool alt, Task* task) {
  // This object will delete itself after running |task|.
  new EventWaiter(task, GDK_KEY_PRESS);
  return SendKeyPress(window, key, control, shift, alt);
}

bool SendMouseMove(long x, long y) {
  gdk_display_warp_pointer(gdk_display_get_default(), gdk_screen_get_default(),
                           x, y);
  return true;
}

bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  bool rv = SendMouseMove(x, y);
  // We can't rely on any particular event signalling the completion of the
  // mouse move. Posting the task to the message loop should gaurantee
  // the pointer has moved before task is run (although it may not run it as
  // soon as it could).
  MessageLoop::current()->PostTask(FROM_HERE, task);
  return rv;
}

bool SendMouseClick(const gfx::Point& point, MouseButton type) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);

  event->button.window = gdk_window_at_pointer(NULL, NULL);
  g_object_ref(event->button.window);
  event->button.send_event = false;
  event->button.time = EventTimeNow();

  event->motion.x_root = point.x();
  event->motion.y_root = point.x();
  gint origin_x, origin_y;
  gdk_window_get_origin(event->button.window, &origin_x, &origin_y);
  event->button.x = point.x() - origin_x;
  event->button.y = point.y() - origin_y;

  event->button.axes = NULL;
  // TODO(estade): as above, we may want to pack this with the actual state.
  event->button.state = 0;
  event->button.button = type == LEFT ? 1 : (type == MIDDLE ? 2 : 3);
  event->button.device = gdk_device_get_core_pointer();

  event->button.type = GDK_BUTTON_PRESS;
  gdk_event_put(event);

  // Also send a release event.
  GdkEvent* release_event = gdk_event_copy(event);
  release_event->button.type = GDK_BUTTON_RELEASE;
  release_event->button.time++;
  gdk_event_put(release_event);

  gdk_event_free(event);
  gdk_event_free(release_event);

  return false;
}

// TODO(estade): need to figure out a better type for this than View.
void MoveMouseToCenterAndPress(views::View* view,
                               MouseButton button,
                               int state,
                               Task* task) {
  NOTIMPLEMENTED();
}

}  // namespace ui_controls
