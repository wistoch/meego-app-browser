// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/constants.h"
#include "gpu/pgl/command_buffer_pepper.h"

#ifdef __native_client__
#include <assert.h>
#define NOTREACHED() assert(0)
#else
#include "base/logging.h"
#endif  // __native_client__

using base::SharedMemory;
using gpu::Buffer;
using gpu::CommandBuffer;

CommandBufferPepper::CommandBufferPepper(NPP npp,
                                         NPDevice* device,
                                         NPDeviceContext3D* context)
    : npp_(npp),
      device_(device),
      context_(context) {
}

CommandBufferPepper::~CommandBufferPepper() {
}

// Not implemented in CommandBufferPepper.
bool CommandBufferPepper::Initialize(int32 size) {
  NOTREACHED();
  return false;
}

Buffer CommandBufferPepper::GetRingBuffer() {
  Buffer buffer;
#if defined(ENABLE_NEW_NPDEVICE_API)
  NPDeviceBuffer np_buffer;
  device_->mapBuffer(npp_,
                     context_,
                     NP3DCommandBufferId,
                     &np_buffer);
  buffer.ptr = np_buffer.ptr;
  buffer.size = np_buffer.size;
#else
  buffer.ptr = context_->commandBuffer;
  buffer.size = context_->commandBufferSize * sizeof(int32);
#endif
  return buffer;
}

CommandBuffer::State CommandBufferPepper::GetState() {
#if defined(ENABLE_NEW_NPDEVICE_API)
  int32 output_attribs[] = {
    NP3DAttrib_CommandBufferSize, 0,
    NP3DAttrib_GetOffset, 0,
    NP3DAttrib_PutOffset, 0,
    NP3DAttrib_Token, 0,
    NPAttrib_Error, 0,
    NPAttrib_End
  };
  device_->synchronizeContext(npp_,
                              context_,
                              NPDeviceSynchronizationMode_Immediate,
                              NULL,
                              output_attribs,
                              NULL,
                              NULL);

  CommandBuffer::State state;
  state.size = output_attribs[1];
  state.get_offset = output_attribs[3];
  state.put_offset = output_attribs[5];
  state.token = output_attribs[7];
  state.error = static_cast<gpu::error::Error>(
      output_attribs[9]);

  return state;
#else
  context_->waitForProgress = false;

  if (NPERR_NO_ERROR != device_->flushContext(npp_, context_, NULL, NULL))
    context_->error = NPDeviceContext3DError_GenericError;

  context_->waitForProgress = true;

  return ConvertState();
#endif  // ENABLE_NEW_NPDEVICE_API
}

CommandBuffer::State CommandBufferPepper::Flush(int32 put_offset) {
#if defined(ENABLE_NEW_NPDEVICE_API)
  int32 input_attribs[] = {
    NP3DAttrib_PutOffset, put_offset,
    NPAttrib_End
  };
  int32 output_attribs[] = {
    NP3DAttrib_CommandBufferSize, 0,
    NP3DAttrib_GetOffset, 0,
    NP3DAttrib_PutOffset, 0,
    NP3DAttrib_Token, 0,
    NPAttrib_Error, 0,
    NPAttrib_End
  };
  device_->synchronizeContext(npp_,
                              context_,
                              NPDeviceSynchronizationMode_Flush,
                              input_attribs,
                              output_attribs,
                              NULL,
                              NULL);

  CommandBuffer::State state;
  state.size = output_attribs[1];
  state.get_offset = output_attribs[3];
  state.put_offset = output_attribs[5];
  state.token = output_attribs[7];
  state.error = static_cast<gpu::error::Error>(
      output_attribs[9]);

  return state;
#else
  context_->waitForProgress = true;
  context_->putOffset = put_offset;

  if (NPERR_NO_ERROR != device_->flushContext(npp_, context_, NULL, NULL))
    context_->error = NPDeviceContext3DError_GenericError;

  return ConvertState();
#endif  // ENABLE_NEW_NPDEVICE_API
}

void CommandBufferPepper::SetGetOffset(int32 get_offset) {
  // Not implemented by proxy.
  NOTREACHED();
}

int32 CommandBufferPepper::CreateTransferBuffer(size_t size) {
  int32 id;
  if (NPERR_NO_ERROR != device_->createBuffer(npp_, context_, size, &id))
    return -1;

  return id;
}

void CommandBufferPepper::DestroyTransferBuffer(int32 id) {
  device_->destroyBuffer(npp_, context_, id);
}

Buffer CommandBufferPepper::GetTransferBuffer(int32 id) {
  NPDeviceBuffer np_buffer;
  if (NPERR_NO_ERROR != device_->mapBuffer(npp_, context_, id, &np_buffer))
    return Buffer();

  Buffer buffer;
  buffer.ptr = np_buffer.ptr;
  buffer.size = np_buffer.size;
  return buffer;
}

void CommandBufferPepper::SetToken(int32 token) {
  // Not implemented by proxy.
  NOTREACHED();
}

void CommandBufferPepper::SetParseError(
    gpu::error::Error error) {
  // Not implemented by proxy.
  NOTREACHED();
}

gpu::error::Error CommandBufferPepper::GetCachedError() {
  int32 attrib_list[] = {
    NPAttrib_Error, 0,
    NPAttrib_End
  };
  device_->synchronizeContext(npp_,
                              context_,
                              NPDeviceSynchronizationMode_Cached,
                              NULL,
                              attrib_list,
                              NULL,
                              NULL);
  return static_cast<gpu::error::Error>(attrib_list[1]);
}

CommandBuffer::State CommandBufferPepper::ConvertState() {
  CommandBuffer::State state;
  state.size = context_->commandBufferSize;
  state.get_offset = context_->getOffset;
  state.put_offset = context_->putOffset;
  state.token = context_->token;
  state.error = static_cast<gpu::error::Error>(
      context_->error);
  return state;
}
