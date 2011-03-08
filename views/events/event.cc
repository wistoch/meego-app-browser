// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include "views/view.h"
#include "views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Event, protected:

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  Init();
}

Event::Event(NativeEvent native_event, ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent(native_event);
}

Event::Event(NativeEvent2 native_event_2, ui::EventType type, int flags,
             FromNativeEvent2 from_native)
    : native_event_2_(native_event_2),
      type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent2(native_event_2, from_native);
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

// TODO(msw): Kill this legacy constructor when we update uses.
LocatedEvent::LocatedEvent(ui::EventType type, const gfx::Point& location,
                           int flags)
    : Event(type, flags),
      location_(location) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* source,
                           View* target)
    : Event(model),
      location_(model.location_) {
  if (target)
    View::ConvertPointToView(source, target, &location_);
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, RootView* root)
    : Event(model),
      location_(model.location_) {
  View::ConvertPointFromWidget(root, &location_);
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(ui::EventType type, ui::KeyboardCode key_code,
                   int event_flags)
    : Event(type, event_flags),
      key_code_(key_code) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

// TODO(msw): Kill this legacy constructor when we update uses.
MouseEvent::MouseEvent(ui::EventType type,
                       View* source,
                       View* target,
                       const gfx::Point &l,
                       int flags)
    : LocatedEvent(MouseEvent(type, l.x(), l.y(), flags), source, target) {
}

MouseEvent::MouseEvent(const MouseEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

#if defined(TOUCH_UI)
TouchEvent::TouchEvent(ui::EventType type, int x, int y, int flags,
                       int touch_id)
      : LocatedEvent(type, gfx::Point(x, y), flags),
        touch_id_(touch_id) {
}


TouchEvent::TouchEvent(ui::EventType type,
                       View* source,
                       View* target,
                       const gfx::Point& l,
                       int flags,
                       int touch_id)
    : LocatedEvent(TouchEvent(type, l.x(), l.y(), flags, touch_id), source,
                              target),
      touch_id_(touch_id) {
}

TouchEvent::TouchEvent(const TouchEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target),
      touch_id_(model.touch_id_) {
}
#endif

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;

}  // namespace views
