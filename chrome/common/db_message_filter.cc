// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "db_message_filter.h"

#include "base/atomic_sequence_num.h"
#include "base/id_map.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/task.h"
#include "chrome/common/child_process.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

DBMessageFilter* DBMessageFilter::instance_ = NULL;

DBMessageFilter::DBMessageFilter()
  : io_thread_message_loop_(ChildProcess::current()->io_message_loop()),
    channel_(NULL), channel_lock_(new Lock()),
    shutdown_event_(ChildProcess::current()->GetShutDownEvent()),
    messages_awaiting_replies_(new IDMap<DBMessageState>()),
    unique_id_generator_(new base::AtomicSequenceNumber()) {
    DCHECK(!instance_);
    instance_ = this;
}

DBMessageFilter::~DBMessageFilter() {
  instance_ = NULL;
}

DBMessageFilter* DBMessageFilter::GetInstance() {
  return instance_;
}

int DBMessageFilter::GetUniqueID() {
  return unique_id_generator_->GetNext();
}

static void SendMessageOnIOThread(IPC::Message* message,
                                  IPC::Channel* channel,
                                  Lock* channel_lock) {
  AutoLock channel_auto_lock(*channel_lock);
  if (!channel) {
    delete message;
  } else {
    channel->Send(message);
  }
}

void DBMessageFilter::Send(IPC::Message* message) {
  io_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(SendMessageOnIOThread, message,
      channel_, channel_lock_.get()));
}

void DBMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = channel;
}

void DBMessageFilter::OnChannelError() {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = NULL;
}

void DBMessageFilter::OnChannelClosing() {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = NULL;
}

bool DBMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DBMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseOpenFileResponse,
                        OnResponse<base::PlatformFile>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseDeleteFileResponse,
                        OnResponse<bool>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseGetFileAttributesResponse,
                        OnResponse<uint32>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseGetFileSizeResponse,
                        OnResponse<int64>)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}
