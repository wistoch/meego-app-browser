// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

static int ShaderTypeToIndex(GLenum shader_type) {
  switch (shader_type) {
    case GL_VERTEX_SHADER:
      return 0;
    case GL_FRAGMENT_SHADER:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

bool ProgramManager::IsInvalidPrefix(const char* name, size_t length) {
  static const char kInvalidPrefix[] = { 'g', 'l', '_' };
  return (length >= sizeof(kInvalidPrefix) &&
      memcmp(name, kInvalidPrefix, sizeof(kInvalidPrefix)) == 0);
}

void ProgramManager::ProgramInfo::Reset() {
  valid_ = false;
  max_uniform_name_length_ = 0;
  max_attrib_name_length_ = 0;
  attrib_infos_.clear();
  uniform_infos_.clear();
  sampler_indices_.clear();
  attrib_location_to_index_map_.clear();
  uniform_location_to_index_map_.clear();
  UpdateLogInfo();
}

void ProgramManager::ProgramInfo::UpdateLogInfo() {
  GLint len = 0;
  glGetProgramiv(service_id_, GL_INFO_LOG_LENGTH, &len);
  scoped_array<char> temp(new char[len]);
  glGetProgramInfoLog(service_id_, len, &len, temp.get());
  set_log_info(std::string(temp.get(), len));
}

void ProgramManager::ProgramInfo::Update() {
  Reset();
  GLint num_attribs = 0;
  GLint max_len = 0;
  GLint max_location = -1;
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveAttrib(
        service_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    if (!IsInvalidPrefix(name_buffer.get(), length)) {
      // TODO(gman): Should we check for error?
      GLint location = glGetAttribLocation(service_id_, name_buffer.get());
      if (location > max_location) {
        max_location = location;
      }
      attrib_infos_.push_back(
          VertexAttribInfo(size, type, name_buffer.get(), location));
      max_attrib_name_length_ = std::max(max_attrib_name_length_, length);
    }
  }

  // Create attrib location to index map.
  attrib_location_to_index_map_.resize(max_location + 1);
  for (GLint ii = 0; ii <= max_location; ++ii) {
    attrib_location_to_index_map_[ii] = -1;
  }
  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    attrib_location_to_index_map_[info.location] = ii;
  }

  GLint num_uniforms = 0;
  max_len = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  name_buffer.reset(new char[max_len]);
  max_location = -1;
  int index = 0;  // this index tracks valid uniforms.
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveUniform(
        service_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    if (!IsInvalidPrefix(name_buffer.get(), length)) {
      GLint location =  glGetUniformLocation(service_id_, name_buffer.get());
      const UniformInfo* info =
          AddUniformInfo(size, type, location, name_buffer.get());
      for (size_t jj = 0; jj < info->element_locations.size(); ++jj) {
        if (info->element_locations[jj] > max_location) {
          max_location = info->element_locations[jj];
        }
      }
      if (info->IsSampler()) {
        sampler_indices_.push_back(index);
      }
      max_uniform_name_length_ =
          std::max(max_uniform_name_length_,
                   static_cast<GLsizei>(info->name.size()));
      ++index;
    }
  }
  // Create uniform location to index map.
  uniform_location_to_index_map_.resize(max_location + 1);
  for (GLint ii = 0; ii <= max_location; ++ii) {
    uniform_location_to_index_map_[ii] = -1;
  }
  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
      uniform_location_to_index_map_[info.element_locations[jj]] = ii;
    }
  }
  valid_ = true;
}

GLint ProgramManager::ProgramInfo::GetUniformLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.element_locations[0];
    } else if (info.is_array &&
               name.size() >= 3 && name[name.size() - 1] == ']') {
      // Look for an array specification.
      size_t open_pos = name.find_last_of('[');
      if (open_pos != std::string::npos &&
          open_pos < name.size() - 2 &&
          info.name.size() > open_pos &&
          name.compare(0, open_pos, info.name, 0, open_pos) == 0) {
        GLint index = 0;
        size_t last = name.size() - 1;
        bool bad = false;
        for (size_t pos = open_pos + 1; pos < last; ++pos) {
          int8 digit = name[pos] - '0';
          if (digit < 0 || digit > 9) {
            bad = true;
            break;
          }
          index = index * 10 + digit;
        }
        if (!bad && index >= 0 && index < info.size) {
          return info.element_locations[index];
        }
      }
    }
  }
  return -1;
}

GLint ProgramManager::ProgramInfo::GetAttribLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    if (info.name == name) {
      return info.location;
    }
  }
  return -1;
}

bool ProgramManager::ProgramInfo::GetUniformTypeByLocation(
    GLint location, GLenum* type) const {
  if (location >= 0 &&
      static_cast<size_t>(location) < uniform_location_to_index_map_.size()) {
    GLint index = uniform_location_to_index_map_[location];
    if (index >= 0) {
      *type = uniform_infos_[index].type;
      return true;
    }
  }
  return false;
}

const ProgramManager::ProgramInfo::UniformInfo*
    ProgramManager::ProgramInfo::AddUniformInfo(
        GLsizei size, GLenum type, GLint location, const std::string& name) {
  const char* kArraySpec = "[0]";
  uniform_infos_.push_back(UniformInfo(size, type, name));
  UniformInfo& info = uniform_infos_.back();
  info.element_locations.resize(size);
  info.element_locations[0] = location;
  size_t num_texture_units = info.IsSampler() ? size : 0u;
  info.texture_units.clear();
  info.texture_units.resize(num_texture_units, 0);

  if (size > 1) {
    for (GLsizei ii = 1; ii < info.size; ++ii) {
      std::string element_name(name + "[" + IntToString(ii) + "]");
      info.element_locations[ii] =
          glGetUniformLocation(service_id_, element_name.c_str());
    }
    // Sadly there is no way to tell if this is an array except if the name
    // has an array string or the size > 1. That means an array of size 1 can
    // be ambiguous.
    //
    // For now we just make sure that if the size is > 1 then the name must have
    // an array spec.

    // Go through the array element locations looking for a match.
    // We can skip the first element because it's the same as the
    // the location without the array operators.
    size_t array_pos = name.rfind(kArraySpec);
    if (name.size() > 3 && array_pos != name.size() - 3) {
      info.name = name + kArraySpec;
    }
  }

  info.is_array =
     (size > 1 ||
      (info.name.size() > 3 &&
       info.name.rfind(kArraySpec) == info.name.size() - 3));

  return &info;
}

bool ProgramManager::ProgramInfo::SetSamplers(
    GLint location, GLsizei count, const GLint* value) {
  if (location >= 0 &&
      static_cast<size_t>(location) < uniform_location_to_index_map_.size()) {
    GLint index = uniform_location_to_index_map_[location];
    if (index >= 0) {
      UniformInfo& info = uniform_infos_[index];
      if (info.IsSampler() && count <= info.size) {
        std::copy(value, value + count, info.texture_units.begin());
        return true;
      }
    }
  }
  return false;
}

void ProgramManager::ProgramInfo::GetProgramiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_ACTIVE_ATTRIBUTES:
      *params = attrib_infos_.size();
      break;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_attrib_name_length_ + 1;
      break;
    case GL_ACTIVE_UNIFORMS:
      *params = uniform_infos_.size();
      break;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_uniform_name_length_ + 1;
      break;
    case GL_LINK_STATUS:
      *params = valid_;
      break;
    case GL_INFO_LOG_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = log_info_.size() + 1;
      break;
    case GL_VALIDATE_STATUS:
      if (!CanLink()) {
        *params = GL_FALSE;
      } else {
        glGetProgramiv(service_id_, pname, params);
      }
      break;
    default:
      glGetProgramiv(service_id_, pname, params);
      break;
  }
}

void ProgramManager::ProgramInfo::AttachShader(
    ShaderManager::ShaderInfo* info) {
  attached_shaders_[ShaderTypeToIndex(info->shader_type())] =
      ShaderManager::ShaderInfo::Ref(info);
}

void ProgramManager::ProgramInfo::DetachShader(
    ShaderManager::ShaderInfo* info) {
  attached_shaders_[ShaderTypeToIndex(info->shader_type())] = NULL;
}

bool ProgramManager::ProgramInfo::CanLink() const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    if (!attached_shaders_[ii] || !attached_shaders_[ii]->IsValid()) {
      return false;
    }
  }
  return true;
}

void ProgramManager::CreateProgramInfo(GLuint client_id, GLuint service_id) {
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(
          std::make_pair(client_id,
                         ProgramInfo::Ref(new ProgramInfo(service_id))));
  DCHECK(result.second);
}

ProgramManager::ProgramInfo* ProgramManager::GetProgramInfo(GLuint client_id) {
  ProgramInfoMap::iterator it = program_infos_.find(client_id);
  return it != program_infos_.end() ? it->second : NULL;
}

void ProgramManager::RemoveProgramInfo(GLuint client_id) {
  ProgramInfoMap::iterator it = program_infos_.find(client_id);
  if (it != program_infos_.end()) {
    it->second->MarkAsDeleted();
    program_infos_.erase(it);
  }
}

bool ProgramManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ProgramInfoMap::const_iterator it = program_infos_.begin();
       it != program_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


