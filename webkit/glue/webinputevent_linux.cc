// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/webinputevent.h"

#include "KeyboardCodes.h"
#include "KeyCodeConversion.h"

#include "webkit/glue/event_conversion.h"

// This header is out of alphabetical order, but event_conversion.h pulls
// in more webkit headers that redefine LOG so I need to undef afterwards.
#undef LOG
#include "base/logging.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkversion.h>

namespace {

double GdkEventTimeToWebEventTime(guint32 time) {
  // Convert from time in ms to time in sec.
  return time / 1000.0;
}

int GdkStateToWebEventModifiers(guint state) {
  int modifiers = 0;
  if (state & GDK_SHIFT_MASK)
    modifiers |= WebInputEvent::SHIFT_KEY;
  if (state & GDK_CONTROL_MASK)
    modifiers |= WebInputEvent::CTRL_KEY;
  if (state & GDK_MOD1_MASK)
    modifiers |= WebInputEvent::ALT_KEY;
#if GTK_CHECK_VERSION(2,10,0)
  if (state & GDK_META_MASK)
    modifiers |= WebInputEvent::META_KEY;
#endif
  return modifiers;
}

}  // namespace

WebMouseEvent::WebMouseEvent(const GdkEventButton* event) {
  timestamp_sec = GdkEventTimeToWebEventTime(event->time);
  modifiers = GdkStateToWebEventModifiers(event->state);
  x = static_cast<int>(event->x);
  y = static_cast<int>(event->y);
  global_x = static_cast<int>(event->x_root);
  global_y = static_cast<int>(event->y_root);
  layout_test_click_count = 0;

  switch (event->type) {
    case GDK_BUTTON_PRESS:
      type = MOUSE_DOWN;
      break;
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      type = MOUSE_DOUBLE_CLICK;
      break;
    case GDK_BUTTON_RELEASE:
      type = MOUSE_UP;
      break;

    default:
      ASSERT_NOT_REACHED();
  };

  button = BUTTON_NONE;
  if (event->button == 1) {
    button = BUTTON_LEFT;
  } else if (event->button == 2) {
    button = BUTTON_MIDDLE;
  } else if (event->button == 3) {
    button = BUTTON_RIGHT;
  }
}

WebMouseEvent::WebMouseEvent(const GdkEventMotion* event) {
  timestamp_sec = GdkEventTimeToWebEventTime(event->time);
  modifiers = GdkStateToWebEventModifiers(event->state);
  x = static_cast<int>(event->x);
  y = static_cast<int>(event->y);
  global_x = static_cast<int>(event->x_root);
  global_y = static_cast<int>(event->y_root);

  switch (event->type) {
    case GDK_MOTION_NOTIFY:
      type = MOUSE_MOVE;
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  button = BUTTON_NONE;
  if (event->state & GDK_BUTTON1_MASK) {
    button = BUTTON_LEFT;
  } else if (event->state & GDK_BUTTON2_MASK) {
    button = BUTTON_MIDDLE;
  } else if (event->state & GDK_BUTTON3_MASK) {
    button = BUTTON_RIGHT;
  }
}

WebMouseWheelEvent::WebMouseWheelEvent(const GdkEventScroll* event) {
  timestamp_sec = GdkEventTimeToWebEventTime(event->time);
  modifiers = GdkStateToWebEventModifiers(event->state);
  x = static_cast<int>(event->x);
  y = static_cast<int>(event->y);
  global_x = static_cast<int>(event->x_root);
  global_y = static_cast<int>(event->y_root);

  type = MOUSE_WHEEL;

  // How much should we scroll per mouse wheel event?
  // - Windows uses 3 lines by default and obeys a system setting.
  // - Mozilla has a pref that lets you either use the "system" number of lines
  //   to scroll, or lets the user override it.
  //   For the "system" number of lines, it appears they've hardcoded 3.
  //   See case NS_MOUSE_SCROLL in content/events/src/nsEventStateManager.cpp
  //   and InitMouseScrollEvent in widget/src/gtk2/nsCommonWidget.cpp .
  // - Gtk makes the scroll amount a function of the size of the scroll bar,
  //   which is not available to us here.
  // Instead, we pick a number that empirically matches Firefox's behavior.
  static const int kWheelDelta = 4;

  delta_x = 0;
  delta_y = 0;

  switch (event->direction) {
    case GDK_SCROLL_UP:
      delta_y = kWheelDelta;
      break;
    case GDK_SCROLL_DOWN:
      delta_y = -kWheelDelta;
      break;
    case GDK_SCROLL_LEFT:
      delta_x = -kWheelDelta;
      break;
    case GDK_SCROLL_RIGHT:
      delta_x = kWheelDelta;
      break;
    default:
      break;
  }
}

WebKeyboardEvent::WebKeyboardEvent(const GdkEventKey* event) {
  modifiers = GdkStateToWebEventModifiers(event->state);

  // GDK only exposes key press and release events.  By contrast,
  // WebKeyboardEvent matches Windows and wants key down/up events along with a
  // separate CHAR event.
  // We require the caller to simulate the CHAR event manually.  See
  // test_shell's webwidget_host for an example.
  switch (event->type) {
    case GDK_KEY_RELEASE:
      type = KEY_UP;
      break;
    case GDK_KEY_PRESS:
      type = KEY_DOWN;
      break;
    default:
      NOTREACHED();
      break;
  }

  // The key code tells us which physical key was pressed (for example, the
  // A key went down or up).  It does not determine whether A should be lower
  // or upper case.  This is what text does, which should be the keyval.
  key_code = WebCore::windowsKeyCodeForKeyEvent(event->keyval);

  switch (event->keyval) {
    // We need to treat the enter key as a key press of character \r.  This
    // is apparently just how webkit handles it and what it expects.
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
      text = '\r';
      break;
    default:
      // This should set text to 0 when it's not a real character.
      text = gdk_keyval_to_unicode(event->keyval);
      break;
  }

  // TODO(tc): Do we need to set IS_AUTO_REPEAT or IS_KEYPAD?
}
