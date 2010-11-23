// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_WIN_H_
#define REMOTING_HOST_EVENT_EXECUTOR_WIN_H_

#include <vector>

#include "base/task.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "remoting/host/event_executor.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {

class KeyEvent;
class MouseDownEvent;
class MouseSetPositionEvent;
class MouseUpEvent;
class MouseWheelEvent;

// A class to generate events on Windows.
class EventExecutorWin : public protocol::InputStub {
 public:
  EventExecutorWin(MessageLoop* message_loop, Capturer* capturer);
  virtual ~EventExecutorWin();

  virtual void InjectKeyEvent(const KeyEvent* event, Task* done);
  virtual void InjectMouseEvent(const MouseEvent* event, Task* done);

 private:
  void HandleMouseSetPosition(const MouseSetPositionEvent& event);
  void HandleMouseWheel(const MouseWheelEvent& event);
  void HandleMouseButtonDown(const MouseDownEvent& event);
  void HandleMouseButtonUp(const MouseUpEvent& event);
  void HandleKey(const KeyEvent& event);

  MessageLoop* message_loop_;
  Capturer* capturer_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorWin);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::EventExecutorWin);

#endif  // REMOTING_HOST_EVENT_EXECUTOR_WIN_H_
