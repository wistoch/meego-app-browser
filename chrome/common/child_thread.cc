// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_thread.h"

#include "base/string_util.h"
#include "base/command_line.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_logging.h"
#include "webkit/glue/webkit_glue.h"

ChildThread::ChildThread(Thread::Options options)
    : Thread("Chrome_ChildThread"),
      owner_loop_(MessageLoop::current()),
      in_send_(0),
      options_(options) {
  DCHECK(owner_loop_);
  channel_name_ = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kProcessChannelID);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserAgent)) {
#if defined(OS_WIN)
    // TODO(port): calling this connects an, otherwise disconnected, subgraph
    // of symbols, causing huge numbers of linker errors.
    webkit_glue::SetUserAgent(WideToUTF8(
        CommandLine::ForCurrentProcess()->GetSwitchValue(switches::kUserAgent)));
#endif
  }
}

ChildThread::~ChildThread() {
}

bool ChildThread::Run() {
  return StartWithOptions(options_);
}

void ChildThread::OnChannelError() {
  owner_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

bool ChildThread::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  in_send_++;
  bool rv = channel_->Send(msg);
  in_send_--;
  return rv;
}

void ChildThread::AddRoute(int32 routing_id, IPC::Channel::Listener* listener) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.AddRoute(routing_id, listener);
}

void ChildThread::RemoveRoute(int32 routing_id) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.RemoveRoute(routing_id);
}

void ChildThread::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(msg);
  } else {
    router_.OnMessageReceived(msg);
  }
}

ChildThread* ChildThread::current() {
  return ChildProcess::current()->child_thread();
}

void ChildThread::Init() {
  channel_.reset(new IPC::SyncChannel(channel_name_,
      IPC::Channel::MODE_CLIENT, this, NULL, owner_loop_, true,
      ChildProcess::current()->GetShutDownEvent()));
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(this);
#endif
}

void ChildThread::CleanUp() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(NULL);
#endif
  // Need to destruct the SyncChannel to the browser before we go away because
  // it caches a pointer to this thread.
  channel_.reset();
}
