// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"
#include "app/gfx/gl/gl_mock.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
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

class ProgramManagerTest : public testing::Test {
 public:
  ProgramManagerTest() { }
  ~ProgramManagerTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  ProgramManager manager_;
};

TEST_F(ProgramManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create program.
  manager_.CreateProgramInfo(kClient1Id, kService1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  EXPECT_FALSE(info1->CanLink());
  EXPECT_STREQ("", info1->log_info().c_str());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent program.
  EXPECT_TRUE(manager_.GetProgramInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent programs does not crash.
  manager_.RemoveProgramInfo(kClient2Id);
  // Check we can't get the program after we remove it.
  manager_.RemoveProgramInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetProgramInfo(kClient1Id) == NULL);
}

TEST_F(ProgramManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  manager_.CreateProgramInfo(kClient1Id, kService1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 =
      manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteProgram(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

class ProgramManagerWithShaderTest : public testing::Test {
 public:
  ProgramManagerWithShaderTest()
      : program_info_(NULL) {
  }

  ~ProgramManagerWithShaderTest() {
    manager_.Destroy(false);
  }

  static const GLint kNumVertexAttribs = 16;

  static const GLuint kClientProgramId = 123;
  static const GLuint kServiceProgramId = 456;

  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;
  static const GLint kAttrib1Size = 1;
  static const GLint kAttrib2Size = 1;
  static const GLint kAttrib3Size = 1;
  static const GLint kAttrib1Location = 0;
  static const GLint kAttrib2Location = 1;
  static const GLint kAttrib3Location = 2;
  static const GLenum kAttrib1Type = GL_FLOAT_VEC4;
  static const GLenum kAttrib2Type = GL_FLOAT_VEC2;
  static const GLenum kAttrib3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidAttribLocation = 30;
  static const GLint kBadAttribIndex = kNumVertexAttribs;

  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform3Name;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const GLint kUniform1Location = 3;
  static const GLint kUniform2Location = 10;
  static const GLint kUniform3Location = 20;
  static const GLenum kUniform1Type = GL_FLOAT_VEC4;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

  static const size_t kNumAttribs;
  static const size_t kNumUniforms;

 protected:
  struct AttribInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  struct UniformInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  virtual void SetUp() {
    gl_.reset(new StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    SetupDefaultShaderExpectations();

    manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
    program_info_ = manager_.GetProgramInfo(kClientProgramId);
    program_info_->Update();
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint service_id) {
    InSequence s;
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_INFO_LOG_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramInfoLog(service_id, _, _, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
        .WillOnce(SetArgumentPointee<2>(num_attribs))
        .RetiresOnSaturation();
    size_t max_attrib_len = 0;
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      size_t len = strlen(attribs[ii].name) + 1;
      max_attrib_len = std::max(max_attrib_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_attrib_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      const AttribInfo& info = attribs[ii];
      EXPECT_CALL(*gl_,
          GetActiveAttrib(service_id, ii,
                          max_attrib_len, _, _, _, _))
          .WillOnce(DoAll(
              SetArgumentPointee<3>(strlen(info.name)),
              SetArgumentPointee<4>(info.size),
              SetArgumentPointee<5>(info.type),
              SetArrayArgument<6>(info.name,
                                  info.name + strlen(info.name) + 1)))
          .RetiresOnSaturation();
      if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
        EXPECT_CALL(*gl_, GetAttribLocation(service_id,
                                            StrEq(info.name)))
            .WillOnce(Return(info.location))
            .RetiresOnSaturation();
      }
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_UNIFORMS, _))
        .WillOnce(SetArgumentPointee<2>(num_uniforms))
        .RetiresOnSaturation();
    size_t max_uniform_len = 0;
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      size_t len = strlen(uniforms[ii].name) + 1;
      max_uniform_len = std::max(max_uniform_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_uniform_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      EXPECT_CALL(*gl_,
          GetActiveUniform(service_id, ii,
                           max_uniform_len, _, _, _, _))
          .WillOnce(DoAll(
              SetArgumentPointee<3>(strlen(info.name)),
              SetArgumentPointee<4>(info.size),
              SetArgumentPointee<5>(info.type),
              SetArrayArgument<6>(info.name,
                                  info.name + strlen(info.name) + 1)))
          .RetiresOnSaturation();
      if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
        EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                             StrEq(info.name)))
            .WillOnce(Return(info.location))
            .RetiresOnSaturation();
        if (info.size > 1) {
          for (GLsizei jj = 1; jj < info.size; ++jj) {
            std::string element_name(
                std::string(info.name) + "[" + IntToString(jj) + "]");
            EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                                 StrEq(element_name)))
                .WillOnce(Return(info.location + jj * 2))
                .RetiresOnSaturation();
          }
        }
      }
    }
  }


  void SetupDefaultShaderExpectations() {
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kServiceProgramId);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];

  scoped_ptr<StrictMock<gfx::MockGLInterface> > gl_;

  ProgramManager manager_;

  ProgramManager::ProgramInfo* program_info_;
};

ProgramManagerWithShaderTest::AttribInfo
    ProgramManagerWithShaderTest::kAttribs[] = {
  { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
  { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
  { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint ProgramManagerWithShaderTest::kNumVertexAttribs;
const GLuint ProgramManagerWithShaderTest::kClientProgramId;
const GLuint ProgramManagerWithShaderTest::kServiceProgramId;
const GLint ProgramManagerWithShaderTest::kAttrib1Size;
const GLint ProgramManagerWithShaderTest::kAttrib2Size;
const GLint ProgramManagerWithShaderTest::kAttrib3Size;
const GLint ProgramManagerWithShaderTest::kAttrib1Location;
const GLint ProgramManagerWithShaderTest::kAttrib2Location;
const GLint ProgramManagerWithShaderTest::kAttrib3Location;
const GLenum ProgramManagerWithShaderTest::kAttrib1Type;
const GLenum ProgramManagerWithShaderTest::kAttrib2Type;
const GLenum ProgramManagerWithShaderTest::kAttrib3Type;
const GLint ProgramManagerWithShaderTest::kInvalidAttribLocation;
const GLint ProgramManagerWithShaderTest::kBadAttribIndex;
const GLint ProgramManagerWithShaderTest::kUniform1Size;
const GLint ProgramManagerWithShaderTest::kUniform2Size;
const GLint ProgramManagerWithShaderTest::kUniform3Size;
const GLint ProgramManagerWithShaderTest::kUniform1Location;
const GLint ProgramManagerWithShaderTest::kUniform2Location;
const GLint ProgramManagerWithShaderTest::kUniform3Location;
const GLenum ProgramManagerWithShaderTest::kUniform1Type;
const GLenum ProgramManagerWithShaderTest::kUniform2Type;
const GLenum ProgramManagerWithShaderTest::kUniform3Type;
const GLint ProgramManagerWithShaderTest::kInvalidUniformLocation;
const GLint ProgramManagerWithShaderTest::kBadUniformIndex;
#endif

const size_t ProgramManagerWithShaderTest::kNumAttribs =
    arraysize(ProgramManagerWithShaderTest::kAttribs);

ProgramManagerWithShaderTest::UniformInfo
    ProgramManagerWithShaderTest::kUniforms[] = {
  { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
  { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
  { kUniform3Name, kUniform3Size, kUniform3Type, kUniform3Location, },
};

const size_t ProgramManagerWithShaderTest::kNumUniforms =
    arraysize(ProgramManagerWithShaderTest::kUniforms);

const char* ProgramManagerWithShaderTest::kAttrib1Name = "attrib1";
const char* ProgramManagerWithShaderTest::kAttrib2Name = "attrib2";
const char* ProgramManagerWithShaderTest::kAttrib3Name = "attrib3";
const char* ProgramManagerWithShaderTest::kUniform1Name = "uniform1";
// Correctly has array spec.
const char* ProgramManagerWithShaderTest::kUniform2Name = "uniform2[0]";
// Incorrectly missing array spec.
const char* ProgramManagerWithShaderTest::kUniform3Name = "uniform3";

TEST_F(ProgramManagerWithShaderTest, GetAttribInfos) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::AttribInfoVector& infos =
      program_info->GetAttribInfos();
  for (size_t ii = 0; ii < kNumAttribs; ++ii) {
    const ProgramManager::ProgramInfo::VertexAttribInfo& info = infos[ii];
    const AttribInfo& expected = kAttribs[ii];
    EXPECT_EQ(expected.size, info.size);
    EXPECT_EQ(expected.type, info.type);
    EXPECT_EQ(expected.location, info.location);
    EXPECT_STREQ(expected.name, info.name.c_str());
  }
}

TEST_F(ProgramManagerWithShaderTest, GetAttribInfo) {
  const GLint kValidIndex = 1;
  const GLint kInvalidIndex = 1000;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::VertexAttribInfo* info =
      program_info->GetAttribInfo(kValidIndex);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kAttrib2Size, info->size);
  EXPECT_EQ(kAttrib2Type, info->type);
  EXPECT_EQ(kAttrib2Location, info->location);
  EXPECT_STREQ(kAttrib2Name, info->name.c_str());
  EXPECT_TRUE(program_info->GetAttribInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, GetAttribLocation) {
  const char* kInvalidName = "foo";
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kAttrib2Location, program_info->GetAttribLocation(kAttrib2Name));
  EXPECT_EQ(-1, program_info->GetAttribLocation(kInvalidName));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfo) {
  const GLint kInvalidIndex = 1000;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::UniformInfo* info =
      program_info->GetUniformInfo(0);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform1Size, info->size);
  EXPECT_EQ(kUniform1Type, info->type);
  EXPECT_EQ(kUniform1Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform1Name, info->name.c_str());
  info = program_info->GetUniformInfo(1);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2Size, info->size);
  EXPECT_EQ(kUniform2Type, info->type);
  EXPECT_EQ(kUniform2Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform2Name, info->name.c_str());
  info = program_info->GetUniformInfo(2);
  // We emulate certain OpenGL drivers by supplying the name without
  // the array spec. Our implementation should correctly add the required spec.
  const std::string expected_name(std::string(kUniform3Name) + "[0]");
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform3Size, info->size);
  EXPECT_EQ(kUniform3Type, info->type);
  EXPECT_EQ(kUniform3Location, info->element_locations[0]);
  EXPECT_STREQ(expected_name.c_str(), info->name.c_str());
  EXPECT_TRUE(program_info->GetUniformInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, AttachDetachShader) {
  ShaderManager shader_manager;
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  shader_manager.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ShaderManager::ShaderInfo* vshader = shader_manager.GetShaderInfo(
      kVShaderClientId);
  vshader->SetStatus(true, "");
  shader_manager.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ShaderManager::ShaderInfo* fshader = shader_manager.GetShaderInfo(
      kFShaderClientId);
  fshader->SetStatus(true, "");
  program_info->AttachShader(vshader);
  EXPECT_FALSE(program_info->CanLink());
  program_info->AttachShader(fshader);
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(vshader);
  EXPECT_FALSE(program_info->CanLink());
  program_info->AttachShader(vshader);
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(fshader);
  EXPECT_FALSE(program_info->CanLink());
  program_info->AttachShader(vshader);
  EXPECT_FALSE(program_info->CanLink());
  program_info->AttachShader(fshader);
  EXPECT_TRUE(program_info->CanLink());
  vshader->SetStatus(false, "");
  EXPECT_FALSE(program_info->CanLink());
  vshader->SetStatus(true, "");
  EXPECT_TRUE(program_info->CanLink());
  fshader->SetStatus(false, "");
  EXPECT_FALSE(program_info->CanLink());
  fshader->SetStatus(true, "");
  EXPECT_TRUE(program_info->CanLink());
  shader_manager.Destroy(false);
}

TEST_F(ProgramManagerWithShaderTest, GetUniformLocation) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kUniform1Location, program_info->GetUniformLocation(kUniform1Name));
  EXPECT_EQ(kUniform2Location, program_info->GetUniformLocation(kUniform2Name));
  EXPECT_EQ(kUniform3Location, program_info->GetUniformLocation(kUniform3Name));
  // Check we can get uniform2 as "uniform2" even though the name is
  // "uniform2[0]"
  EXPECT_EQ(kUniform2Location, program_info->GetUniformLocation("uniform2"));
  // Check we can get uniform3 as "uniform3[0]" even though we simulated GL
  // returning "uniform3"
  EXPECT_EQ(kUniform3Location, program_info->GetUniformLocation("uniform3[0]"));
  // Check that we can get the locations of the array elements > 1
  EXPECT_EQ(kUniform2Location + 2,
            program_info->GetUniformLocation("uniform2[1]"));
  EXPECT_EQ(kUniform2Location + 4,
            program_info->GetUniformLocation("uniform2[2]"));
  EXPECT_EQ(-1,
            program_info->GetUniformLocation("uniform2[3]"));
  EXPECT_EQ(kUniform3Location + 2,
            program_info->GetUniformLocation("uniform3[1]"));
  EXPECT_EQ(-1,
            program_info->GetUniformLocation("uniform3[2]"));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformTypeByLocation) {
  const GLint kInvalidLocation = 1234;
  GLenum type = 0u;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->GetUniformTypeByLocation(kUniform2Location, &type));
  EXPECT_EQ(kUniform2Type, type);
  type = 0u;
  EXPECT_FALSE(program_info->GetUniformTypeByLocation(
      kInvalidLocation, &type));
  EXPECT_EQ(0u, type);
}

// Some GL drivers incorrectly return gl_DepthRange and possibly other uniforms
// that start with "gl_". Our implementation catches these and does not allow
// them back to client.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsGLUnderscoreUniform) {
  static const char* kUniform2Name = "gl_longNameWeCanCheckFor";
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
    { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
    { kUniform3Name, kUniform3Size, kUniform3Type, kUniform3Location, },
  };
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  program_info->Update();
  GLint value = 0;
  program_info->GetProgramiv(GL_ACTIVE_ATTRIBUTES, &value);
  EXPECT_EQ(3, value);
  // Check that we skipped the "gl_" uniform.
  program_info->GetProgramiv(GL_ACTIVE_UNIFORMS, &value);
  EXPECT_EQ(2, value);
  // Check that our max length adds room for the array spec and is not as long
  // as the "gl_" uniform we skipped.
  // +4u is to account for "gl_" and NULL terminator.
  program_info->GetProgramiv(GL_ACTIVE_UNIFORM_MAX_LENGTH, &value);
  EXPECT_EQ(strlen(kUniform3Name) + 4u, static_cast<size_t>(value));
}

}  // namespace gles2
}  // namespace gpu


