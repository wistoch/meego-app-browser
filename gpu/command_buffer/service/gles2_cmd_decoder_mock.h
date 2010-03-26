// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the mock GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_

#include "gfx/size.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {
namespace gles2 {

class ContextGroup;
class MockGLES2Decoder : public GLES2Decoder {
 public:
  explicit MockGLES2Decoder(ContextGroup* group)
      : GLES2Decoder(group) {
    ON_CALL(*this, GetCommandName(testing::_))
      .WillByDefault(testing::Return(""));
    ON_CALL(*this, MakeCurrent())
      .WillByDefault(testing::Return(true));
  }

#if defined(OS_MACOSX)
  MOCK_METHOD2(SetWindowSizeForIOSurface, uint64(int32 width, int32 height));
  MOCK_METHOD2(SetWindowSizeForTransportDIB,
               TransportDIB::Handle(int32 width, int32 height));
  MOCK_METHOD2(SetTransportDIBAllocAndFree,
               void(Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
                    Callback1<TransportDIB::Id>::Type* deallocator));
#endif
  MOCK_METHOD3(Initialize, bool(GLES2Decoder* parent,
                                const gfx::Size& size,
                                uint32 parent_texture_id));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD1(ResizeOffscreenFrameBuffer, void(const gfx::Size& size));
  MOCK_METHOD0(MakeCurrent, bool());
  MOCK_METHOD1(GetServiceIdForTesting, uint32(uint32 client_id));
  MOCK_METHOD0(GetGLES2Util, GLES2Util*());
  MOCK_METHOD1(SetSwapBuffersCallback, void(Callback0::Type*));
  MOCK_METHOD3(DoCommand, error::Error(unsigned int command,
                                       unsigned int arg_count,
                                       const void* cmd_data));
  MOCK_CONST_METHOD1(GetCommandName, const char*(unsigned int command_id));

  DISALLOW_COPY_AND_ASSIGN(MockGLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
