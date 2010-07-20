// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "base/bits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/GLES2/gles2_command_buffer.h"

namespace gpu {
namespace gles2 {

static GLsizei ComputeMipMapCount(
    GLsizei width, GLsizei height, GLsizei depth) {
  return 1 + base::bits::Log2Floor(std::max(std::max(width, height), depth));
}

static size_t GLTargetToFaceIndex(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return 0;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      return 0;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      return 1;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      return 2;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      return 3;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      return 4;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return 5;
    default:
      NOTREACHED();
      return 0;
  }
}

static size_t FaceIndexToGLTarget(size_t index) {
  switch (index) {
    case 0:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    case 1:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
    case 2:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
    case 3:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
    case 4:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
    case 5:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    default:
      NOTREACHED();
      return 0;
  }
}

TextureManager::~TextureManager() {
  DCHECK(texture_infos_.empty());
}

void TextureManager::Destroy(bool have_context) {
  while (!texture_infos_.empty()) {
    if (have_context) {
      TextureInfo* info = texture_infos_.begin()->second;
      if (!info->IsDeleted()) {
        GLuint service_id = info->service_id();
        glDeleteTextures(1, &service_id);
        info->MarkAsDeleted();
      }
    }
    texture_infos_.erase(texture_infos_.begin());
  }
}

bool TextureManager::TextureInfo::CanRender(
    const TextureManager* manager) const {
  DCHECK(manager);
  if (target_ == 0 || IsDeleted()) {
    return false;
  }
  bool needs_mips = NeedsMips();
  if (npot() && !manager->npot_ok()) {
    return !needs_mips &&
           wrap_s_ == GL_CLAMP_TO_EDGE &&
           wrap_t_ == GL_CLAMP_TO_EDGE;
  }
  if (needs_mips) {
    if (target_ == GL_TEXTURE_2D) {
      return texture_complete();
    } else {
      return texture_complete() && cube_complete();
    }
  } else {
    return true;
  }
}

bool TextureManager::TextureInfo::MarkMipmapsGenerated(
    const TextureManager* manager) {
  if (!CanGenerateMipmaps(manager)) {
    return false;
  }
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info1 = level_infos_[ii][0];
    GLsizei width = info1.width;
    GLsizei height = info1.height;
    GLsizei depth = info1.depth;
    int num_mips = ComputeMipMapCount(width, height, depth);
    for (int level = 1; level < num_mips; ++level) {
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      SetLevelInfo(manager,
                   target_ == GL_TEXTURE_2D ? GL_TEXTURE_2D :
                                              FaceIndexToGLTarget(ii),
                   level,
                   info1.internal_format,
                   width,
                   height,
                   depth,
                   info1.border,
                   info1.format,
                   info1.type);
    }
  }
  return true;
}

bool TextureManager::TextureInfo::CanGenerateMipmaps(
    const TextureManager* manager) const {
  if ((npot() && !manager->npot_ok()) || level_infos_.empty() || IsDeleted()) {
    return false;
  }
  const TextureInfo::LevelInfo& first = level_infos_[0][0];
  // TODO(gman): Check internal_format, format and type.
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const LevelInfo& info = level_infos_[ii][0];
    if ((!info.valid) ||
        (info.width != first.width) ||
        (info.height != first.height) ||
        (info.depth != 1) ||
        (info.format != first.format) ||
        (info.internal_format != first.internal_format) ||
        (info.type != first.type)) {
        return false;
    }
  }
  return true;
}

void TextureManager::TextureInfo::SetLevelInfo(
    const TextureManager* manager,
    GLenum target,
    GLint level,
    GLint internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_GE(depth, 0);
  TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  info.valid = true;
  info.internal_format = internal_format;
  info.width = width;
  info.height = height;
  info.depth = depth;
  info.border = border;
  info.format = format;
  info.type = type;
  max_level_set_ = std::max(max_level_set_, level);
  Update(manager);
}

bool TextureManager::TextureInfo::GetLevelSize(
    GLint face, GLint level, GLsizei* width, GLsizei* height) const {
  size_t face_index = GLTargetToFaceIndex(face);
  if (!IsDeleted() && level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(face)][level];
    *width = info.width;
    *height = info.height;
    return true;
  }
  return false;
}

void TextureManager::TextureInfo::SetParameter(
    const TextureManager* manager, GLenum pname, GLint param) {
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      min_filter_ = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      mag_filter_ = param;
      break;
    case GL_TEXTURE_WRAP_S:
      wrap_s_ = param;
      break;
    case GL_TEXTURE_WRAP_T:
      wrap_t_ = param;
      break;
    default:
      NOTREACHED();
      break;
  }
  Update(manager);
}

void TextureManager::TextureInfo::Update(const TextureManager* manager) {
  // Update npot status.
  npot_ = false;
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info = level_infos_[ii][0];
    if (GLES2Util::IsNPOT(info.width) ||
        GLES2Util::IsNPOT(info.height) ||
        GLES2Util::IsNPOT(info.depth)) {
      npot_ = true;
      break;
    }
  }

  // Update texture_complete and cube_complete status.
  const TextureInfo::LevelInfo& first_face = level_infos_[0][0];
  texture_complete_ =
      (max_level_set_ == ComputeMipMapCount(first_face.width,
                                            first_face.height,
                                            first_face.depth) - 1) &&
      max_level_set_ >= 0;
  cube_complete_ = (level_infos_.size() == 6) &&
                   (first_face.width == first_face.height);
  if (first_face.type == GL_FLOAT && !manager->enable_float_linear() &&
      (min_filter_ != GL_NEAREST_MIPMAP_NEAREST ||
       mag_filter_ != GL_NEAREST)) {
    texture_complete_ = false;
  } else if (first_face.type == GL_HALF_FLOAT_OES &&
             !manager->enable_half_float_linear() &&
             (min_filter_ != GL_NEAREST_MIPMAP_NEAREST ||
              mag_filter_ != GL_NEAREST)) {
    texture_complete_ = false;
  }
  for (size_t ii = 0;
       ii < level_infos_.size() && (cube_complete_ || texture_complete_);
       ++ii) {
    const TextureInfo::LevelInfo& level0 = level_infos_[ii][0];
    if (!level0.valid ||
        level0.width != first_face.width ||
        level0.height != first_face.height ||
        level0.depth != 1 ||
        level0.internal_format != first_face.internal_format ||
        level0.format != first_face.format ||
        level0.type != first_face.type) {
      cube_complete_ = false;
    }
    // Get level0 dimensions
    GLsizei width = level0.width;
    GLsizei height = level0.height;
    GLsizei depth = level0.depth;
    for (GLint jj = 1; jj <= max_level_set_; ++jj) {
      // compute required size for mip.
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      const TextureInfo::LevelInfo& info = level_infos_[ii][jj];
      if (!info.valid ||
          info.width != width ||
          info.height != height ||
          info.depth != depth ||
          info.internal_format != level0.internal_format ||
          info.format != level0.format ||
          info.type != level0.type) {
        texture_complete_ = false;
        break;
      }
    }
  }
}

TextureManager::TextureManager(
    bool npot_ok,
    bool enable_float_linear,
    bool enable_half_float_linear,
    GLint max_texture_size,
    GLint max_cube_map_texture_size)
    : npot_ok_(npot_ok),
      enable_float_linear_(enable_float_linear),
      enable_half_float_linear_(enable_half_float_linear),
      max_texture_size_(max_texture_size),
      max_cube_map_texture_size_(max_cube_map_texture_size),
      max_levels_(ComputeMipMapCount(max_texture_size,
                                     max_texture_size,
                                     max_texture_size)),
      max_cube_map_levels_(ComputeMipMapCount(max_cube_map_texture_size,
                                              max_cube_map_texture_size,
                                              max_cube_map_texture_size)),
      num_unrenderable_textures_(0) {
  default_texture_2d_ = TextureInfo::Ref(new TextureInfo(0));
  SetInfoTarget(default_texture_2d_, GL_TEXTURE_2D);
  default_texture_2d_->SetLevelInfo(
    this, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  default_texture_cube_map_ = TextureInfo::Ref(new TextureInfo(0));
  SetInfoTarget(default_texture_cube_map_, GL_TEXTURE_CUBE_MAP);
  for (int ii = 0; ii < GLES2Util::kNumFaces; ++ii) {
    default_texture_cube_map_->SetLevelInfo(
      this, GLES2Util::IndexToGLFaceTarget(ii),
      0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  }
}

bool TextureManager::ValidForTarget(
    GLenum target, GLint level,
    GLsizei width, GLsizei height, GLsizei depth) {
  GLsizei max_size = MaxSizeForTarget(target);
  return level >= 0 &&
         width >= 0 &&
         height >= 0 &&
         depth >= 0 &&
         level < MaxLevelsForTarget(target) &&
         width <= max_size &&
         height <= max_size &&
         depth <= max_size &&
         (level == 0 ||
          (!GLES2Util::IsNPOT(width) &&
           !GLES2Util::IsNPOT(height) &&
           !GLES2Util::IsNPOT(depth))) &&
         (target != GL_TEXTURE_CUBE_MAP || (width == height && depth == 1)) &&
         (target != GL_TEXTURE_2D || (depth == 1));
}

void TextureManager::SetLevelInfo(
    TextureManager::TextureInfo* info,
    GLenum target,
    GLint level,
    GLint internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  DCHECK(info);
  DCHECK(!info->IsDeleted());
  if (!info->CanRender(this)) {
    --num_unrenderable_textures_;
  }
  info->SetLevelInfo(
      this, target, level, internal_format, width, height, depth,
      border, format, type);
  if (!info->CanRender(this)) {
    ++num_unrenderable_textures_;
  }
}

void TextureManager::SetParameter(
    TextureManager::TextureInfo* info, GLenum pname, GLint param) {
  DCHECK(info);
  DCHECK(!info->IsDeleted());
  if (!info->CanRender(this)) {
    --num_unrenderable_textures_;
  }
  info->SetParameter(this, pname, param);
  if (!info->CanRender(this)) {
    ++num_unrenderable_textures_;
  }
}

bool TextureManager::MarkMipmapsGenerated(TextureManager::TextureInfo* info) {
  DCHECK(info);
  DCHECK(!info->IsDeleted());
  if (!info->CanRender(this)) {
    --num_unrenderable_textures_;
  }
  bool result = info->MarkMipmapsGenerated(this);
  if (!info->CanRender(this)) {
    ++num_unrenderable_textures_;
  }
  return result;
}

TextureManager::TextureInfo* TextureManager::CreateTextureInfo(
    GLuint client_id, GLuint service_id) {
  TextureInfo::Ref info(new TextureInfo(service_id));
  std::pair<TextureInfoMap::iterator, bool> result =
      texture_infos_.insert(std::make_pair(client_id, info));
  DCHECK(result.second);
  if (!info->CanRender(this)) {
    ++num_unrenderable_textures_;
  }
  return info.get();
}

TextureManager::TextureInfo* TextureManager::GetTextureInfo(
    GLuint client_id) {
  TextureInfoMap::iterator it = texture_infos_.find(client_id);
  return it != texture_infos_.end() ? it->second : NULL;
}

void TextureManager::RemoveTextureInfo(GLuint client_id) {
  TextureInfoMap::iterator it = texture_infos_.find(client_id);
  if (it != texture_infos_.end()) {
    TextureInfo* info = it->second;
    if (!info->CanRender(this)) {
      --num_unrenderable_textures_;
    }
    info->MarkAsDeleted();
    texture_infos_.erase(it);
  }
}

bool TextureManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (TextureInfoMap::const_iterator it = texture_infos_.begin();
       it != texture_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


