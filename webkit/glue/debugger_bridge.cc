// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"  // webkit config for V8
#include "base/message_loop.h"
#include "base/string_util.h"
#include "webkit/glue/debugger_bridge.h"

#if USE(V8)
#define USING_V8
#include "v8/include/v8-debug.h"
#endif

void V8DebugMessageHandler(const uint16_t* message, int length,
                           v8::Debug::ClientData* client_data) {
  if (!DebuggerBridge::instance_) {
    NOTREACHED();
    return;
  }
  std::wstring out(reinterpret_cast<const wchar_t*>(message), length);
  DebuggerBridge::instance_->OutputLater(out);
}

// static
DebuggerBridge* DebuggerBridge::instance_ = NULL;

DebuggerBridge::DebuggerBridge(Delegate* del)
    : delegate_(del),
      attached_(false) {
  delegate_loop_ = MessageLoop::current();
  DCHECK(instance_ == NULL);
  instance_ = this;
}

DebuggerBridge::~DebuggerBridge() {
  DCHECK(!attached_);
  instance_ = NULL;
  Detach();
}

void DebuggerBridge::Break(bool force) {
#ifdef USING_V8
  DCHECK(attached_);
  v8::Debug::DebugBreak();
#endif
}

void DebuggerBridge::Attach() {
#ifdef USING_V8
  if (!attached_) {
    attached_ = true;
    v8::Debug::SetMessageHandler(V8DebugMessageHandler);
  }
#endif
}

void DebuggerBridge::Detach() {
#ifdef USING_V8
  if (attached_) {
    attached_ = false;
    v8::Debug::SetMessageHandler(NULL);
  }
#endif
}

void DebuggerBridge::OutputLater(const std::wstring& out) {
  delegate_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DebuggerBridge::Output, out));
}

void DebuggerBridge::Output(const std::wstring& out) {
  delegate_->DebuggerOutput(out);
}

void DebuggerBridge::Command(const std::wstring& cmd) {
#ifdef USING_V8
  DCHECK(attached_);
  v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.data()),
                         cmd.length());
#endif
}
