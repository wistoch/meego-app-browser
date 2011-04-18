// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_

#include "../common/types.h"

namespace gpu {

typedef int32 CommandBufferOffset;
const CommandBufferOffset kInvalidCommandBufferOffset = -1;

// This enum must stay in sync with NPDeviceContext3DError.
namespace error {
  enum Error {
    kNoError,
    kInvalidSize,
    kOutOfBounds,
    kUnknownCommand,
    kInvalidArguments,
    kLostContext,
    kGenericError,

    // This is not an error. It is returned by WaitLatch when it is blocked.
    // When blocked, the context will not reschedule itself until another
    // context executes a SetLatch command.
    kWaiting,

    // This is not an error. It is returned by commands to mark a position
    // in the command buffer that should not be issued to the the GL backend
    // until no more than a fixed number of such positions have already been
    // issued.
    kThrottle
  };
}

// Invalid shared memory Id, returned by RegisterSharedMemory in case of
// failure.
const int32 kInvalidSharedMemoryId = -1;

// Common Command Buffer shared memory transfer buffer ID.
const int32 kCommandBufferSharedMemoryId = 4;

// Common Latch shared memory transfer buffer ID.
const int32 kLatchSharedMemoryId = 5;

// Invalid latch ID.
const int32 kInvalidLatchId = 0xffffffffu;

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
