// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "chrome/common/command_buffer_messages.h"
#include "chrome/plugin/command_buffer_stub.h"
#include "chrome/plugin/plugin_channel.h"

using gpu::Buffer;

CommandBufferStub::CommandBufferStub(PluginChannel* channel,
                                     gfx::NativeView view)
    : channel_(channel),
      view_(view) {
  route_id_ = channel->GenerateRouteID();
  channel->AddRoute(route_id_, this, false);
}

CommandBufferStub::~CommandBufferStub() {
  channel_->RemoveRoute(route_id_);
}

void CommandBufferStub::OnChannelError() {
  NOTREACHED() << "CommandBufferService::OnChannelError called";
}

bool CommandBufferStub::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void CommandBufferStub::OnInitialize(int32 size,
                                     base::SharedMemoryHandle* ring_buffer) {
  DCHECK(!command_buffer_.get());

  *ring_buffer = base::SharedMemory::NULLHandle();

  // Assume service is responsible for duplicating the handle from the calling
  // process.
  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(channel_->peer_pid(), &peer_handle))
    return;

  command_buffer_.reset(new gpu::CommandBufferService);

  // Initialize the CommandBufferService and GPUProcessor.
  if (command_buffer_->Initialize(size)) {
    Buffer buffer = command_buffer_->GetRingBuffer();
    if (buffer.shared_memory) {
      processor_ = new gpu::GPUProcessor(command_buffer_.get());
      if (processor_->Initialize(view_)) {
        command_buffer_->SetPutOffsetChangeCallback(
            NewCallback(processor_.get(),
                        &gpu::GPUProcessor::ProcessCommands));
        buffer.shared_memory->ShareToProcess(peer_handle, ring_buffer);
      } else {
        processor_ = NULL;
        command_buffer_.reset();
      }
    }
  }

  base::CloseProcessHandle(peer_handle);
}

void CommandBufferStub::OnSyncOffsets(int32 put_offset, int32* get_offset) {
  *get_offset = command_buffer_->SyncOffsets(put_offset);
}

void CommandBufferStub::OnGetGetOffset(int32* get_offset) {
  *get_offset = command_buffer_->GetGetOffset();
}

void CommandBufferStub::OnGetPutOffset(int32* put_offset) {
  *put_offset = command_buffer_->GetPutOffset();
}

void CommandBufferStub::OnCreateTransferBuffer(int32 size, int32* id) {
  *id = command_buffer_->CreateTransferBuffer(size);
}

void CommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferStub::OnGetTransferBuffer(
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    size_t* size) {
  *transfer_buffer = 0;
  *size = 0;

  // Assume service is responsible for duplicating the handle to the calling
  // process.
  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(channel_->peer_pid(), &peer_handle))
    return;

  Buffer buffer = command_buffer_->GetTransferBuffer(id);
  if (buffer.shared_memory) {
    buffer.shared_memory->ShareToProcess(peer_handle, transfer_buffer);
    *size = buffer.shared_memory->max_size();
  }

  base::CloseProcessHandle(peer_handle);
}

void CommandBufferStub::OnGetToken(int32* token) {
  *token = command_buffer_->GetToken();
}

void CommandBufferStub::OnResetParseError(int32* parse_error) {
  *parse_error = command_buffer_->ResetParseError();
}

void CommandBufferStub::OnGetErrorStatus(bool* error_status) {
  *error_status = command_buffer_->GetErrorStatus();
}

void CommandBufferStub::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(CommandBufferStub, msg)
    IPC_MESSAGE_HANDLER(CommandBufferMsg_Initialize, OnInitialize);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_SyncOffsets, OnSyncOffsets);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetGetOffset, OnGetGetOffset);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetPutOffset, OnGetPutOffset);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_CreateTransferBuffer,
                        OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetTransferBuffer,
                        OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetToken, OnGetToken);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_ResetParseError, OnResetParseError);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetErrorStatus, OnGetErrorStatus);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}
