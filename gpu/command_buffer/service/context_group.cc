// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup()
    : initialized_(false),
      max_vertex_attribs_(0u),
      max_texture_units_(0u),
      max_texture_image_units_(0u),
      max_vertex_texture_image_units_(0u),
      max_fragment_uniform_vectors_(0u),
      max_varying_vectors_(0u),
      max_vertex_uniform_vectors_(0u) {
}

ContextGroup::~ContextGroup() {
}

static void GetIntegerv(GLenum pname, uint32* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

bool ContextGroup::Initialize() {
  if (initialized_) {
    return true;
  }

  buffer_manager_.reset(new BufferManager());
  framebuffer_manager_.reset(new FramebufferManager());
  renderbuffer_manager_.reset(new RenderbufferManager());
  shader_manager_.reset(new ShaderManager());
  program_manager_.reset(new ProgramManager());

  // Lookup GL things we need to know.
  GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs_);
  const GLuint kGLES2RequiredMiniumumVertexAttribs = 8u;
  DCHECK_GE(max_vertex_attribs_, kGLES2RequiredMiniumumVertexAttribs);

  GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units_);
  const GLuint kGLES2RequiredMiniumumTextureUnits = 8u;
  DCHECK_GE(max_texture_units_, kGLES2RequiredMiniumumTextureUnits);

  GLint max_texture_size;
  GLint max_cube_map_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_cube_map_texture_size);
  texture_manager_.reset(new TextureManager(max_texture_size,
                                            max_cube_map_texture_size));

  GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_image_units_);
  GetIntegerv(
      GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units_);

#if defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &max_fragment_uniform_vectors_);
  GetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors_);
  GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors_);

#else  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  GetIntegerv(
      GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_fragment_uniform_vectors_);
  max_fragment_uniform_vectors_ /= 4;
  GetIntegerv(GL_MAX_VARYING_FLOATS, &max_varying_vectors_);
  max_varying_vectors_ /= 4;
  GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_vertex_uniform_vectors_);
  max_vertex_uniform_vectors_ /= 4;

#endif  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  initialized_ = true;
  return true;
}

IdAllocator* ContextGroup::GetIdAllocator(unsigned namespace_id) {
  IdAllocatorMap::iterator it = id_namespaces_.find(namespace_id);
  if (it != id_namespaces_.end()) {
    return it->second.get();
  }
  IdAllocator* id_allocator = new IdAllocator();
  id_namespaces_[namespace_id] = linked_ptr<IdAllocator>(id_allocator);
  return id_allocator;
}

}  // namespace gles2
}  // namespace gpu


