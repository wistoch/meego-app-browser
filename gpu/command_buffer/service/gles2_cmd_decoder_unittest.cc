// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gles2::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest() { }
};

class GLES2DecoderWithShaderTest : public GLES2DecoderWithShaderTestBase {
 public:
  GLES2DecoderWithShaderTest()
      : GLES2DecoderWithShaderTestBase() {
  }
};

TEST_F(GLES2DecoderWithShaderTest, DrawArraysNoAttributesSucceeds) {
  SetupTexture();
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysBadTextureUsesBlack) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // This is an NPOT texture. As the default filtering requires mips
  // this should trigger replacing with black textures before rendering.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  {
    InSequence sequence;
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceBlackTexture2dId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
  }
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysMissingAttributesFails) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysValidAttributesSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedBufferFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteVertexBuffer();

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       DrawArraysDeletedProgramSucceedsWithoutGLCall) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysWithInvalidModeFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_QUADS, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysInvalidCountFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawArrays(_, _, _)).Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 1, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, 0, kNumVertices + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with attrib offset > 0
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with size > 2 (ie, vec3 instead of vec2)
  DoVertexAttribPointer(1, 3, GL_FLOAT, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with stride > 8 (vec2 + vec2 byte)
  GLfloat f;
  DoVertexAttribPointer(1, 2, GL_FLOAT, sizeof(f) * 2 + 1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsNoAttributesSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart * 2)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsMissingAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsValidAttributesSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart * 2)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeletedBufferFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteIndexBuffer();

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeletedProgramSucceedsNoGLCall) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsWithInvalidModeFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_QUADS, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsInvalidCountFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kNumIndices, GL_UNSIGNED_SHORT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, kNumIndices + 1, GL_UNSIGNED_SHORT, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsOutOfRangeIndicesFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT,
           kInvalidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsOddOffsetForUint16Fails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervSucceeds) {
  const float dummy = 0;
  const GLuint kOffsetToTestFor = sizeof(dummy) * 4;
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test that initial value is 0.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(0u, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Set the value and see that we get it.
  SetupVertexBuffer();
  DoVertexAttribPointer(kIndexToTest, 2, GL_FLOAT, 0, kOffsetToTestFor);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(kOffsetToTestFor, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervBadArgsFails) {
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test pname invalid fails.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER + 1,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Test index out of range fails.
  result->size = 0;
  cmd.Init(kNumVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test memory id bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Test memory offset bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivArrayElementSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformiv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadProgramFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
  result->size = kInitialResult;
  cmd.Init(client_texture_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadLocationFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadSharedMemoryFails) {
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvArrayElementSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformfv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadProgramFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
  result->size = kInitialResult;
  cmd.Init(client_texture_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadLocationFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadSharedMemoryFails) {
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersSucceeds) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(kServiceProgramId, 1, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),
                      SetArgumentPointee<3>(kServiceShaderId)));
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(client_shader_id_, result->GetData()[0]);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersResultNotInitFail) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 1;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadProgramFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  cmd.Init(kInvalidClientId, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadSharedMemoryFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset,
           Result::ComputeSize(1));
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatSucceeds) {
  GetShaderPrecisionFormat cmd;
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  // NOTE: GL will not be called. There is no equivalent Desktop OpenGL
  // function.
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(-62, result->min_range);
  EXPECT_EQ(62, result->max_range);
  EXPECT_EQ(-16, result->precision);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatResultNotInitFails) {
  GetShaderPrecisionFormat cmd;
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  // NOTE: GL will not be called. There is no equivalent Desktop OpenGL
  // function.
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatBadArgsFails) {
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_TEXTURE_2D, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  result->success = 0;
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       GetShaderPrecisionFormatBadSharedMemoryFails) {
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformSucceeds) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kUniform2Size, result->size);
  EXPECT_EQ(kUniform2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kUniform2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformResultNotInitFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadProgramFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  result->success = 0;
  cmd.Init(client_texture_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kBadUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadSharedMemoryFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribSucceeds) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kAttrib2Size, result->size);
  EXPECT_EQ(kAttrib2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kAttrib2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribResultNotInitFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadProgramFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  result->success = 0;
  cmd.Init(client_texture_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kBadAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadSharedMemoryFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CompileShaderValidArgs) {
  EXPECT_CALL(*gl_, ShaderSource(kServiceShaderId, 1, _, _));
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  CompileShader cmd;
  cmd.Init(client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CompileShaderInvalidArgs) {
  CompileShader cmd;
  cmd.Init(kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, ShaderSourceAndGetShaderSourceValidArgs) {
  const uint32 kBucketId = 123;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  memcpy(shared_memory_address_, kSource, kSourceSize);
  ShaderSource cmd;
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  memset(shared_memory_address_, 0, kSourceSize);
  GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(kSourceSize + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kSource,
                      bucket->size()));
}

TEST_F(GLES2DecoderTest, ShaderSourceInvalidArgs) {
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  memcpy(shared_memory_address_, kSource, kSourceSize);
  ShaderSource cmd;
  cmd.Init(kInvalidClientId,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_texture_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_shader_id_,
           kInvalidSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kInvalidSharedMemoryOffset, kSourceSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSharedBufferSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ShaderSourceImmediateAndGetShaderSourceValidArgs) {
  const uint32 kBucketId = 123;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  ShaderSourceImmediate& cmd = *GetImmediateAs<ShaderSourceImmediate>();
  cmd.Init(client_shader_id_, kSourceSize);
  memcpy(GetImmediateDataAs<void*>(&cmd), kSource, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  memset(shared_memory_address_, 0, kSourceSize);
  // TODO(gman): GetShaderSource has to change format so result is always set.
  GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(kSourceSize + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kSource,
                      bucket->size()));
}

TEST_F(GLES2DecoderTest, ShaderSourceImmediateInvalidArgs) {
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  ShaderSourceImmediate& cmd = *GetImmediateAs<ShaderSourceImmediate>();
  cmd.Init(kInvalidClientId, kSourceSize);
  memcpy(GetImmediateDataAs<void*>(&cmd), kSource, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_texture_id_, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, GenerateMipmapWrongFormatsFails) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_))
       .Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1iValidArgs) {
  EXPECT_CALL(*gl_, Uniform1i(kUniform1Location, 2));
  SpecializedSetup<Uniform1i, 0>();
  Uniform1i cmd;
  cmd.Init(kUniform1Location, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1iv(
          kUniform1Location, 2,
          reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivImmediateValidArgs) {
  Uniform1ivImmediate& cmd = *GetImmediateAs<Uniform1ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1iv(kUniform1Location, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform1ivImmediate, 0>();
  GLint temp[1 * 2] = { 0, };
  cmd.Init(kUniform1Location, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderWithShaderTest, BindBufferToDifferentTargetFails) {
  // Bind the buffer to GL_ARRAY_BUFFER
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  // Attempt to rebind to GL_ELEMENT_ARRAY_BUFFER
  // NOTE: Real GLES2 does not have this restriction but WebGL and we do.
  EXPECT_CALL(*gl_, BindBuffer(_, _))
      .Times(0);
  BindBuffer cmd;
  cmd.Init(GL_ELEMENT_ARRAY_BUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, ActiveTextureValidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  SpecializedSetup<ActiveTexture, 0>();
  ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, ActiveTextureInalidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(_)).Times(0);
  SpecializedSetup<ActiveTexture, 0>();
  ActiveTexture cmd;
  cmd.Init(GL_TEXTURE0 - 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(kNumTextureUnits);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest, CheckFramebufferStatusWithNoBoundTarget) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .Times(0);
  CheckFramebufferStatus::Result* result =
      static_cast<CheckFramebufferStatus::Result*>(shared_memory_address_);
  *result = 0;
  CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(*result, GL_FRAMEBUFFER_COMPLETE);
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(_, _, _, _))
      .Times(0);
  FramebufferRenderbuffer cmd;
  cmd.Init(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
    client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, FramebufferTexture2DWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _))
      .Times(0);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _))
      .Times(0);
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, GetRenderbufferParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _))
      .Times(0);
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, RenderbufferStorageWithNoBoundTarget) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _))
      .Times(0);
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

// TODO(gman): BindAttribLocation

// TODO(gman): BindAttribLocationImmediate

// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate

// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexSubImage2D

// TODO(gman): CompressedTexSubImage2DImmediate

// TODO(gman): DeleteProgram

// TODO(gman): DeleteShader

// TODO(gman): GetAttribLocation

// TODO(gman): GetAttribLocationImmediate

// TODO(gman): GetUniformLocation

// TODO(gman): GetUniformLocationImmediate

// TODO(gman): PixelStorei

// TODO(gman): ReadPixels

// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate

// TODO(gman): TexSubImage2D

// TODO(gman): TexSubImage2DImmediate

// TODO(gman): UseProgram

// TODO(gman): SwapBuffers

// TODO(gman): VertexAttribPointer

}  // namespace gles2
}  // namespace gpu


