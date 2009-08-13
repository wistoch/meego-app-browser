// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "base/logging.h"
#include "chrome/test/automation/automation_constants.h"

namespace {

int GdkKeycodeForWindowsKeycode(wchar_t windows_keyval) {
  switch (windows_keyval) {
    case VK_SPACE:
      return GDK_space;
    default:
      NOTREACHED() << "Unsupported keyval: " << windows_keyval;
      return 0;
  }
}

}  // namespace

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  wchar_t key, bool control, bool shift, bool alt) {
  GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);

  event->key.type = GDK_KEY_PRESS;
  event->key.window = GTK_WIDGET(window)->window;
  g_object_ref(event->key.window);
  event->key.send_event = false;
  // TODO(estade): Put the real time?
  event->key.time = GDK_CURRENT_TIME;
  // TODO(estade): handle other state flags besides control, shift, alt?
  // For example caps lock.
  event->key.state = (control ? GDK_CONTROL_MASK : 0) |
                     (shift ? GDK_SHIFT_MASK : 0) |
                     (alt ? GDK_MOD1_MASK : 0);
  event->key.keyval = GdkKeycodeForWindowsKeycode(key);
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

bool SendKeyPressNotifyWhenDone(wchar_t key, bool control, bool shift,
                                bool alt, Task* task) {
  NOTIMPLEMENTED();
  return false;
}

// TODO(estade): this appears to be unused on Windows. Can we remove it?
bool SendKeyDown(wchar_t key) {
  NOTIMPLEMENTED();
  return false;
}

// TODO(estade): this appears to be unused on Windows. Can we remove it?
bool SendKeyUp(wchar_t key) {
  NOTIMPLEMENTED();
  return false;
}

bool SendMouseMove(long x, long y) {
  NOTIMPLEMENTED();
  return false;
}

void SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  NOTIMPLEMENTED();
}

bool SendMouseClick(gfx::NativeWindow window, const gfx::Point& point,
                    MouseButton type) {
  NOTIMPLEMENTED();
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
