// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public testing::Test {
 public:
  GLES2DecoderTest()
      : client_buffer_id_(100),
        client_framebuffer_id_(101),
        client_program_id_(102),
        client_renderbuffer_id_(103),
        client_shader_id_(104),
        client_texture_id_(105),
        client_element_buffer_id_(106) {
    memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
  }

 protected:
  static const GLint kNumVertexAttribs = 16;

  static const GLuint kServiceBufferId = 301;
  static const GLuint kServiceFramebufferId = 302;
  static const GLuint kServiceRenderbufferId = 303;
  static const GLuint kServiceTextureId = 304;
  static const GLuint kServiceProgramId = 305;
  static const GLuint kServiceShaderId = 306;
  static const GLuint kServiceElementBufferId = 307;

  static const int32 kSharedMemoryId = 401;
  static const size_t kSharedBufferSize = 2048;
  static const uint32 kSharedMemoryOffset = 132;
  static const int32 kInvalidSharedMemoryId = 402;
  static const uint32 kInvalidSharedMemoryOffset = kSharedBufferSize + 1;

  static const uint32 kNewClientId = 501;
  static const uint32 kNewServiceId = 502;
  static const uint32 kInvalidClientId = 601;

  // Template to call glGenXXX functions.
  template <typename T>
  void GenHelper(GLuint client_id) {
    int8 buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(parse_error::kParseNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  // This template exists solely so we can specialize it for
  // certain commands.
  template <typename T, int id>
  void SpecializedSetup() {
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  template <typename T, typename Command>
  T GetImmediateDataAs(Command* cmd) {
    reinterpret_cast<T>(ImmediateDataAddress(cmd));
  }

  virtual void SetUp() {
    gl_.reset(new ::gles2::MockGLInterface());
    ::gles2::GLInterface::SetGLInterface(gl_.get());

    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
        .WillOnce(SetArgumentPointee<1>(kNumVertexAttribs))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillRepeatedly(Return(GL_NO_ERROR));

    engine_.reset(new MockCommandBufferEngine());
    Buffer buffer = engine_->GetSharedMemoryBuffer(kSharedMemoryId);
    shared_memory_offset_ = kSharedMemoryOffset;
    shared_memory_address_ = reinterpret_cast<int8*>(buffer.ptr) +
        shared_memory_offset_;
    shared_memory_id_ = kSharedMemoryId;

    decoder_.reset(GLES2Decoder::Create());
    decoder_->Initialize();
    decoder_->set_engine(engine_.get());

    EXPECT_CALL(*gl_, GenBuffersARB(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceBufferId))
        .RetiresOnSaturation();
    GenHelper<GenBuffersImmediate>(client_buffer_id_);
    EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceFramebufferId))
        .RetiresOnSaturation();
    GenHelper<GenFramebuffersImmediate>(client_framebuffer_id_);
    EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceRenderbufferId))
        .RetiresOnSaturation();
    GenHelper<GenRenderbuffersImmediate>(client_renderbuffer_id_);
    EXPECT_CALL(*gl_, GenTextures(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceTextureId))
        .RetiresOnSaturation();
    GenHelper<GenTexturesImmediate>(client_texture_id_);
    EXPECT_CALL(*gl_, GenBuffersARB(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceElementBufferId))
        .RetiresOnSaturation();
    GenHelper<GenBuffersImmediate>(client_element_buffer_id_);

    {
      EXPECT_CALL(*gl_, CreateProgram())
          .Times(1)
          .WillOnce(Return(kServiceProgramId))
          .RetiresOnSaturation();
      CreateProgram cmd;
      cmd.Init(client_program_id_);
      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }

    {
      EXPECT_CALL(*gl_, CreateShader(_))
          .Times(1)
          .WillOnce(Return(kServiceShaderId))
          .RetiresOnSaturation();
      CreateShader cmd;
      cmd.Init(GL_VERTEX_SHADER, client_shader_id_);
      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }

    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  virtual void TearDown() {
    decoder_->Destroy();
    decoder_.reset();
    engine_.reset();
    ::gles2::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  template <typename T>
  parse_error::ParseError ExecuteCmd(const T& cmd) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd)) - 1,
                               &cmd);
  }

  template <typename T>
  parse_error::ParseError ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd) + data_size) - 1,
                               &cmd);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_);
  }

  uint32 GetServiceId(uint32 client_id) {
    return decoder_->GetServiceIdForTesting(client_id);
  }

  GLenum GetGLError() {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    return *GetSharedMemoryAs<GLenum*>();
  }

  scoped_ptr<::gles2::MockGLInterface> gl_;
  scoped_ptr<GLES2Decoder> decoder_;

  GLuint client_buffer_id_;
  GLuint client_framebuffer_id_;
  GLuint client_program_id_;
  GLuint client_renderbuffer_id_;
  GLuint client_shader_id_;
  GLuint client_texture_id_;
  GLuint client_element_buffer_id_;

  uint32 shared_memory_id_;
  uint32 shared_memory_offset_;
  void* shared_memory_address_;

  int8 immediate_buffer_[256];

 private:
  class MockCommandBufferEngine : public CommandBufferEngine {
   public:
    MockCommandBufferEngine() {
      data_.reset(new int8[kSharedBufferSize]);
      valid_buffer_.ptr = data_.get();
      valid_buffer_.size = kSharedBufferSize;
    }

    virtual ~MockCommandBufferEngine() {
    }

    Buffer GetSharedMemoryBuffer(int32 shm_id) {
      return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
    }

    void set_token(int32 token) {
      DCHECK(false);
    }

    // Overridden from CommandBufferEngine.
    virtual bool SetGetOffset(int32 offset) {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual int32 GetGetOffset() {
      DCHECK(false);
      return 0;
    }

   private:
    scoped_array<int8> data_;
    Buffer valid_buffer_;
    Buffer invalid_buffer_;
  };

  scoped_ptr<MockCommandBufferEngine> engine_;
};

template <>
void GLES2DecoderTest::SpecializedSetup<LinkProgram, 0>() {
  InSequence dummy;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
};


class GLES2DecoderWithShaderTest : public GLES2DecoderTest {
 public:
  GLES2DecoderWithShaderTest()
      : GLES2DecoderTest() {
  }

  static const GLint kNumAttribs = 3;
  static const GLint kMaxAttribLength = 10;
  static const GLsizei kNumVertices = 100;
  static const GLsizei kNumIndices = 10;
  static const int kValidIndexRangeStart = 1;
  static const int kValidIndexRangeCount = 7;
  static const int kInvalidIndexRangeStart = 0;
  static const int kInvalidIndexRangeCount = 7;
  static const int kOutOfRangeIndexRangeEnd = 10;
  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;

 protected:
  virtual void SetUp() {
    GLES2DecoderTest::SetUp();

    {
      struct AttribInfo {
        const char* name;
        GLint size;
        GLenum type;
        GLint location;
      };
      static AttribInfo attribs[] = {
        { kAttrib1Name, 1, GL_FLOAT_VEC4, 0, },
        { kAttrib2Name, 1, GL_FLOAT_VEC2, 1, },
        { kAttrib3Name, 1, GL_FLOAT_VEC3, 2, },
      };
      LinkProgram cmd;
      cmd.Init(client_program_id_);

      EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_, GetError())
          .WillOnce(Return(GL_NO_ERROR))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
          GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
          .WillOnce(SetArgumentPointee<2>(kNumAttribs))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
          GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
          .WillOnce(SetArgumentPointee<2>(kMaxAttribLength))
          .RetiresOnSaturation();
      {
        InSequence s;
        for (GLint ii = 0; ii < kNumAttribs; ++ii) {
          const AttribInfo& info = attribs[ii];
          EXPECT_CALL(*gl_,
              GetActiveAttrib(kServiceProgramId, ii,
                              kMaxAttribLength + 1, _, _, _, _))
              .WillOnce(DoAll(
                  SetArgumentPointee<3>(strlen(info.name)),
                  SetArgumentPointee<4>(info.size),
                  SetArgumentPointee<5>(info.type),
                  SetArrayArgument<6>(info.name,
                                      info.name + strlen(info.name) + 1)))
              .RetiresOnSaturation();
          EXPECT_CALL(*gl_, GetAttribLocation(kServiceProgramId,
                                              StrEq(info.name)))
              .WillOnce(Return(info.location))
              .RetiresOnSaturation();
        }
      }

      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }

    {
      EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
          .Times(1)
          .RetiresOnSaturation();
      UseProgram cmd;
      cmd.Init(client_program_id_);
      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }
  }

  virtual void TearDown() {
    GLES2DecoderTest::TearDown();
  }

  inline GLvoid* BufferOffset(unsigned i) {
    return static_cast<int8 *>(NULL)+(i);
  }

  void DoEnableVertexAttribArray(GLint index) {
    EXPECT_CALL(*gl_, EnableVertexAttribArray(index))
        .Times(1)
        .RetiresOnSaturation();
    EnableVertexAttribArray cmd;
    cmd.Init(index);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoBindBuffer(GLenum target, GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, BindBuffer(target, service_id))
        .Times(1)
        .RetiresOnSaturation();
    BindBuffer cmd;
    cmd.Init(target, client_id);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoBufferData(GLenum target, GLsizei size) {
    EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
        .Times(1)
        .RetiresOnSaturation();
    BufferData cmd;
    cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoBufferSubData(
      GLenum target, GLint offset, GLsizei size, const void* data) {
    EXPECT_CALL(*gl_, BufferSubData(target, offset, size,
                                    shared_memory_address_))
        .Times(1)
        .RetiresOnSaturation();
    memcpy(shared_memory_address_, data, size);
    BufferSubData cmd;
    cmd.Init(target, offset, size, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoDeleteBuffer(GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(service_id)))
        .Times(1)
        .RetiresOnSaturation();
    DeleteBuffers cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    memcpy(shared_memory_address_, &client_id, sizeof(client_id));
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoDeleteProgram(GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, DeleteProgram(service_id))
        .Times(1)
        .RetiresOnSaturation();
    DeleteProgram cmd;
    cmd.Init(client_id);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void DoVertexAttribPointer(
      GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset) {
    EXPECT_CALL(*gl_,
                VertexAttribPointer(index, size, type, GL_FALSE, stride,
                                    BufferOffset(offset)))
        .Times(1)
        .RetiresOnSaturation();
    VertexAttribPointer cmd;
    cmd.Init(index, size, GL_FLOAT, GL_FALSE, stride, offset);
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }

  void SetupVertexBuffer() {
    DoEnableVertexAttribArray(1);
    DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
    GLfloat f = 0;
    DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 2 * sizeof(f));
  }

  void SetupIndexBuffer() {
    DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                 client_element_buffer_id_,
                 kServiceElementBufferId);
    static const GLshort indices[] = {100, 1, 2, 3, 4, 5, 6, 7, 100, 9};
    COMPILE_ASSERT(arraysize(indices) == kNumIndices, Indices_is_not_10);
    DoBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices));
    DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);
  }

  void DeleteVertexBuffer() {
    DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
  }

  void DeleteIndexBuffer() {
    DoDeleteBuffer(client_element_buffer_id_, kServiceElementBufferId);
  }
};

const char* GLES2DecoderWithShaderTest::kAttrib1Name = "attrib1";
const char* GLES2DecoderWithShaderTest::kAttrib2Name = "attrib2";
const char* GLES2DecoderWithShaderTest::kAttrib3Name = "attrib3";

TEST_F(GLES2DecoderWithShaderTest, DrawArraysNoAttributesSucceeds) {
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysMissingAttributesFails) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysValidAttributesSucceeds) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
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
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedProgramFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysWithInvalidModeFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_QUADS, 0, 1);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, 0, 1);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysInvalidCountFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawArrays(_, _, _)).Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 1, kNumVertices);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, 0, kNumVertices + 1);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with attrib offset > 0
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 4);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with size > 2 (ie, vec3 instead of vec2)
  DoVertexAttribPointer(1, 3, GL_FLOAT, 0, 0);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with stride > 8 (vec2 + vec2 byte)
  GLfloat f;
  DoVertexAttribPointer(1, 2, GL_FLOAT, sizeof(f) * 2 + 1, 0);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsNoAttributesSucceeds) {
  SetupIndexBuffer();
  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsMissingAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsValidAttributesSucceeds) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
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
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeleteProgramFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsWithInvalidModeFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_QUADS, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsInvalidCountFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kNumIndices, GL_UNSIGNED_SHORT, 1);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, kNumIndices + 1, GL_UNSIGNED_SHORT, 0);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

#if 0  // TODO(gman): Turn on this test once buffer validation is in
TEST_F(GLES2DecoderWithShaderTest, DrawElementsOutOfRangeIndicesFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT,
           kInvalidIndexRangeStart);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_autogen.h"

}  // namespace gles2
}  // namespace gpu


