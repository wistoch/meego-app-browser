/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Implementations of the abstract Texture2D and TextureCUBE classes using
// the OpenCB graphics API.

#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/texture_cb.h"

#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/common/cross/resource.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"

namespace o3d {

using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::FencedAllocatorWrapper;
using command_buffer::ResourceID;
namespace texture = command_buffer::texture;
namespace set_texture_data_cmd = command_buffer::set_texture_data_cmd;
namespace get_texture_data_cmd = command_buffer::get_texture_data_cmd;
namespace create_texture_2d_cmd = command_buffer::create_texture_2d_cmd;
namespace create_texture_cube_cmd = command_buffer::create_texture_cube_cmd;

namespace {

const Texture::RGBASwizzleIndices g_cb_abgr32f_swizzle_indices =
    {0, 1, 2, 3};

// Converts an O3D texture format to a command buffer texture format.
texture::Format CBFormatFromO3DFormat(Texture::Format format) {
  switch (format) {
    case Texture::XRGB8:
      return texture::XRGB8;
    case Texture::ARGB8:
      return texture::ARGB8;
    case Texture::ABGR16F:
      return texture::ABGR16F;
    case Texture::DXT1:
      return texture::DXT1;
      // TODO: DXT3/5. It's not yet supported by the command buffer
      // renderer, though it would be a simple addition.
    default:
      break;
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  return texture::NUM_FORMATS;
}

// Checks that enums match in value, so that they can be directly used in
// set_texture_data_cmd::Face bitfields.
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_X == texture::FACE_POSITIVE_X,
               FACE_POSITIVE_X_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_X == texture::FACE_NEGATIVE_X,
               FACE_NEGATIVE_X_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_Y == texture::FACE_POSITIVE_Y,
               FACE_POSITIVE_Y_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_Y == texture::FACE_NEGATIVE_Y,
               FACE_NEGATIVE_Y_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_Z == texture::FACE_POSITIVE_Z,
               FACE_POSITIVE_Z_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_Z == texture::FACE_NEGATIVE_Z,
               FACE_NEGATIVE_Z_enums_don_t_match);

// Writes the data information into a buffer to be sent to the server side.
void SetTextureDataBuffer(Texture::Format format,
                          const void* src_data,
                          int src_pitch,
                          unsigned src_width,
                          unsigned src_height,
                          void* dst_buffer,
                          unsigned int dst_pitch) {
  const uint8* src = static_cast<const uint8*>(src_data);
  uint8* dst = static_cast<uint8*>(dst_buffer);
  size_t bytes_per_line = image::ComputePitch(format, src_width);
  for (unsigned yy = 0; yy < src_height; ++yy) {
    memcpy(dst, src, bytes_per_line);
    src += src_pitch;
    dst += dst_pitch;
  }
}
// Sends the SET_TEXTURE_DATA command after formatting the args properly.
void SetTextureData(RendererCB *renderer,
                    ResourceID texture_id,
                    unsigned int x,
                    unsigned int y,
                    unsigned int mip_width,
                    unsigned int mip_height,
                    unsigned int z,
                    unsigned int depth,
                    unsigned int level,
                    TextureCUBE::CubeFace face,
                    int pitch,
                    size_t mip_size,
                    unsigned char* mip_data) {
  FencedAllocatorWrapper *allocator = renderer->allocator();
  CommandBufferHelper *helper = renderer->helper();

  CommandBufferEntry args[10];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      set_texture_data_cmd::X::MakeValue(x) |
      set_texture_data_cmd::Y::MakeValue(y);
  args[2].value_uint32 =
      set_texture_data_cmd::Width::MakeValue(mip_width) |
      set_texture_data_cmd::Height::MakeValue(mip_height);
  args[3].value_uint32 =
      set_texture_data_cmd::Z::MakeValue(z) |
      set_texture_data_cmd::Depth::MakeValue(depth);
  args[4].value_uint32 =
      set_texture_data_cmd::Level::MakeValue(level) |
      set_texture_data_cmd::Face::MakeValue(face);
  args[5].value_uint32 = pitch;
  args[6].value_uint32 = 0;  // slice_pitch
  args[7].value_uint32 = mip_size;
  args[8].value_uint32 = renderer->transfer_shm_id();
  args[9].value_uint32 = allocator->GetOffset(mip_data);
  helper->AddCommand(command_buffer::SET_TEXTURE_DATA, 10, args);
  allocator->FreePendingToken(mip_data, helper->InsertToken());
}
// Updates a command buffer texture resource from a bitmap, rescaling if
// necessary.
void UpdateResourceFromBitmap(RendererCB *renderer,
                              ResourceID texture_id,
                              unsigned int level,
                              TextureCUBE::CubeFace face,
                              const Bitmap &bitmap) {
  DCHECK(bitmap.image_data());
  FencedAllocatorWrapper *allocator = renderer->allocator();
  CommandBufferHelper *helper = renderer->helper();
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  unsigned char *mip_data = bitmap.GetMipData(level);
  size_t mip_size =
      image::ComputeBufferSize(mip_width, mip_height, bitmap.format());
  unsigned char *buffer = allocator->AllocTyped<unsigned char>(mip_size);
  DCHECK(buffer);
  memcpy(buffer, mip_data, mip_size);
  mip_data = buffer;

  size_t pitch = image::ComputeBufferSize(mip_width, 1, bitmap.format());
  SetTextureData(renderer,
                 texture_id,
                 0,
                 0,
                 mip_width,
                 mip_height,
                 0,
                 1,
                 level,
                 face,
                 pitch,
                 mip_size,
                 mip_data);
}

// Copies back texture resource data into a bitmap.
void CopyBackResourceToBitmap(RendererCB *renderer,
                              ResourceID texture_id,
                              unsigned int level,
                              TextureCUBE::CubeFace face,
                              const Bitmap &bitmap) {
  DCHECK(bitmap.image_data());
  FencedAllocatorWrapper *allocator = renderer->allocator();
  CommandBufferHelper *helper = renderer->helper();
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  size_t mip_size =
      image::ComputeBufferSize(mip_width, mip_height, bitmap.format());
  unsigned char *buffer = allocator->AllocTyped<unsigned char>(mip_size);
  DCHECK(buffer);

  size_t pitch = image::ComputeBufferSize(mip_width, 1, bitmap.format());

  CommandBufferEntry args[10];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      get_texture_data_cmd::X::MakeValue(0) |
      get_texture_data_cmd::Y::MakeValue(0);
  args[2].value_uint32 =
      get_texture_data_cmd::Width::MakeValue(mip_width) |
      get_texture_data_cmd::Height::MakeValue(mip_height);
  args[3].value_uint32 =
      get_texture_data_cmd::Z::MakeValue(0) |
      get_texture_data_cmd::Depth::MakeValue(1);
  args[4].value_uint32 =
      get_texture_data_cmd::Level::MakeValue(level) |
      get_texture_data_cmd::Face::MakeValue(face);
  args[5].value_uint32 = pitch;
  args[6].value_uint32 = 0;  // slice_pitch
  args[7].value_uint32 = mip_size;
  args[8].value_uint32 = renderer->transfer_shm_id();
  args[9].value_uint32 = allocator->GetOffset(buffer);
  helper->AddCommand(command_buffer::GET_TEXTURE_DATA, 10, args);
  helper->Finish();
  memcpy(bitmap.GetMipData(level), buffer, mip_size);
  allocator->Free(buffer);
}

}  // anonymous namespace

static const unsigned int kMaxTextureSize = 2048;

// Texture2DCB -----------------------------------------------------------------

// Constructs a 2D texture object from an existing command-buffer 2D texture
// resource.
// NOTE: the Texture2DCB now owns the texture resource and will destroy it on
// exit.
Texture2DCB::Texture2DCB(ServiceLocator* service_locator,
                         ResourceID resource_id,
                         Texture::Format format,
                         int levels,
                         int width,
                         int height,
                         bool enable_render_surfaces)
    : Texture2D(service_locator,
                width,
                height,
                format,
                levels,
                enable_render_surfaces),
      renderer_(static_cast<RendererCB*>(
                    service_locator->GetService<Renderer>())),
      resource_id_(resource_id),
      backing_bitmap_(Bitmap::Ref(new Bitmap(service_locator))),
      has_levels_(0),
      locked_levels_(0) {
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
}

Texture2DCB::~Texture2DCB() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    renderer_->helper()->AddCommand(command_buffer::DESTROY_TEXTURE, 1, args);
  }
}

// Creates a new texture object from scratch.
Texture2DCB* Texture2DCB::Create(ServiceLocator* service_locator,
                                 Texture::Format format,
                                 int levels,
                                 int width,
                                 int height,
                                 bool enable_render_surfaces) {
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
  RendererCB *renderer = static_cast<RendererCB *>(
      service_locator->GetService<Renderer>());
  texture::Format cb_format = CBFormatFromO3DFormat(format);
  if (cb_format == texture::NUM_FORMATS) {
    O3D_ERROR(service_locator)
        << "Unsupported format in Texture2DCB::Create.";
    return NULL;
  }
  if (width > kMaxTextureSize || height > kMaxTextureSize) {
    O3D_ERROR(service_locator) << "Texture dimensions (" << width
                               << ", " << height << ") too big.";
    return NULL;
  }

  ResourceID texture_id = renderer->texture_ids().AllocateID();
  CommandBufferEntry args[3];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      create_texture_2d_cmd::Width::MakeValue(width) |
      create_texture_2d_cmd::Height::MakeValue(height);
  args[2].value_uint32 =
      create_texture_2d_cmd::Levels::MakeValue(levels) |
      create_texture_2d_cmd::Format::MakeValue(cb_format) |
      create_texture_2d_cmd::Flags::MakeValue(enable_render_surfaces);
  renderer->helper()->AddCommand(command_buffer::CREATE_TEXTURE_2D, 3, args);

  Texture2DCB *texture = new Texture2DCB(service_locator, texture_id,
                                         format, levels, width, height,
                                         enable_render_surfaces);

  return texture;
}

void Texture2DCB::SetRect(int level,
                          unsigned dst_left,
                          unsigned dst_top,
                          unsigned src_width,
                          unsigned src_height,
                          const void* src_data,
                          int src_pitch) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect on non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, width());
  unsigned mip_height = image::ComputeMipDimension(level, height());

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }
  unsigned int dst_pitch = image::ComputePitch(format(), src_width);
  size_t size = dst_pitch * src_height;

  FencedAllocatorWrapper *allocator = renderer_->allocator();
  uint8 *buffer = allocator->AllocTyped<uint8>(size);
  DCHECK(buffer);
  SetTextureDataBuffer(format(), src_data, src_pitch, src_width, src_height,
                       buffer, dst_pitch);

  SetTextureData(renderer_,
                 resource_id(),
                 dst_left,
                 dst_top,
                 src_width,
                 src_height,
                 0,
                 1,
                 level,
                 TextureCUBE::CubeFace(0),
                 dst_pitch,
                 size,
                 buffer);
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DCB::PlatformSpecificLock(
    int level, void** data, int* pitch, AccessMode mode) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  if (!backing_bitmap_->image_data()) {
    DCHECK_EQ(has_levels_, 0);
    backing_bitmap_->Allocate(format(), width(), height(), levels(),
                              Bitmap::IMAGE);
  }
  *data = backing_bitmap_->GetMipData(level);
  unsigned int mip_width = image::ComputeMipDimension(level, width());
  unsigned int mip_height = image::ComputeMipDimension(level, height());
  if (!IsCompressed()) {
    *pitch = image::ComputePitch(format(), mip_width);
  } else {
    unsigned blocks_across = (mip_width + 3) / 4;
    unsigned bytes_per_block = format() == Texture::DXT1 ? 8 : 16;
    unsigned bytes_per_row = bytes_per_block * blocks_across;
    *pitch = bytes_per_row;
  }
  if (mode != kWriteOnly && !HasLevel(level)) {
    DCHECK_EQ(backing_bitmap_->width(), width());
    DCHECK_EQ(backing_bitmap_->height(), height());
    DCHECK_EQ(backing_bitmap_->format(), format());
    DCHECK_GT(backing_bitmap_->num_mipmaps(), level);
    CopyBackResourceToBitmap(renderer_, resource_id_, level,
                             TextureCUBE::FACE_POSITIVE_X,
                             *backing_bitmap_.Get());
    has_levels_ |= 1 << level;
  }
  locked_levels_ |= 1 << level;
  return true;
}

// Unlocks the given mipmap level of this texture, uploading the main memory
// data buffer to the command buffer service.
bool Texture2DCB::PlatformSpecificUnlock(int level) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  DCHECK(backing_bitmap_->image_data());
  DCHECK_EQ(backing_bitmap_->width(), width());
  DCHECK_EQ(backing_bitmap_->height(), height());
  DCHECK_EQ(backing_bitmap_->format(), format());
  DCHECK_GT(backing_bitmap_->num_mipmaps(), level);
  if (LockedMode(level) != kReadOnly) {
    DCHECK(HasLevel(level));
    UpdateResourceFromBitmap(renderer_, resource_id_, level,
                             TextureCUBE::FACE_POSITIVE_X,
                             *backing_bitmap_.Get());
  }
  locked_levels_ &= ~(1 << level);
  if (locked_levels_ == 0) {
    backing_bitmap_->FreeData();
    has_levels_ = 0;
  }
  return true;
}

RenderSurface::Ref Texture2DCB::PlatformSpecificGetRenderSurface(
    int mip_level) {
  DCHECK_LT(mip_level, levels());
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }
  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  return RenderSurface::Ref(new RenderSurfaceCB(service_locator(),
                                                width() >> mip_level,
                                                height() >> mip_level,
                                                mip_level,
                                                0,
                                                this,
                                                renderer_));
}

const Texture::RGBASwizzleIndices& Texture2DCB::GetABGR32FSwizzleIndices() {
  return g_cb_abgr32f_swizzle_indices;
}
// TextureCUBECB ---------------------------------------------------------------

// Creates a texture from a pre-existing texture resource.
TextureCUBECB::TextureCUBECB(ServiceLocator* service_locator,
                             ResourceID resource_id,
                             Texture::Format format,
                             int levels,
                             int edge_length,
                             bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  edge_length,
                  format,
                  levels,
                  enable_render_surfaces),
      renderer_(static_cast<RendererCB*>(
                    service_locator->GetService<Renderer>())),
      resource_id_(resource_id) {
  for (int ii = 0; ii < static_cast<int>(NUMBER_OF_FACES); ++ii) {
    backing_bitmaps_[ii] = Bitmap::Ref(new Bitmap(service_locator));
    has_levels_[ii] = 0;
    locked_levels_[ii] = 0;
  }
}

TextureCUBECB::~TextureCUBECB() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    renderer_->helper()->AddCommand(command_buffer::DESTROY_TEXTURE, 1, args);
  }
}

// Create a new Cube texture from scratch.
TextureCUBECB* TextureCUBECB::Create(ServiceLocator* service_locator,
                                     Texture::Format format,
                                     int levels,
                                     int edge_length,
                                     bool enable_render_surfaces) {
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
  RendererCB *renderer = static_cast<RendererCB *>(
      service_locator->GetService<Renderer>());
  texture::Format cb_format = CBFormatFromO3DFormat(format);
  if (cb_format == texture::NUM_FORMATS) {
    O3D_ERROR(service_locator)
        << "Unsupported format in Texture2DCB::Create.";
    return NULL;
  }
  if (edge_length > kMaxTextureSize) {
    O3D_ERROR(service_locator) << "Texture dimensions (" << edge_length
                               << ", " << edge_length << ") too big.";
    return NULL;
  }

  ResourceID texture_id = renderer->texture_ids().AllocateID();
  CommandBufferEntry args[3];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 = create_texture_cube_cmd::Side::MakeValue(edge_length);
  args[2].value_uint32 =
      create_texture_cube_cmd::Levels::MakeValue(levels) |
      create_texture_cube_cmd::Format::MakeValue(cb_format) |
      create_texture_cube_cmd::Flags::MakeValue(enable_render_surfaces);
  renderer->helper()->AddCommand(command_buffer::CREATE_TEXTURE_CUBE, 3, args);

  TextureCUBECB* texture =
      new TextureCUBECB(service_locator, texture_id, format, levels,
                        edge_length, enable_render_surfaces);

  return texture;
}

void TextureCUBECB::SetRect(TextureCUBE::CubeFace face,
                            int level,
                            unsigned dst_left,
                            unsigned dst_top,
                            unsigned src_width,
                            unsigned src_height,
                            const void* src_data,
                            int src_pitch) {
  if (static_cast<int>(face) < 0 || static_cast<int>(face) >= NUMBER_OF_FACES) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect invalid face " << face << " on Texture \""
        << name() << "\"";
    return;
  }
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, edge_length());
  unsigned mip_height = mip_width;

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }

  unsigned int dst_pitch = image::ComputePitch(format(), src_width);
  size_t size = dst_pitch * src_height;

  FencedAllocatorWrapper *allocator = renderer_->allocator();
  uint8 *buffer = allocator->AllocTyped<uint8>(size);
  DCHECK(buffer);
  SetTextureDataBuffer(format(), src_data, src_pitch, src_width, src_height,
                       buffer, dst_pitch);

  SetTextureData(renderer_,
                 resource_id(),
                 dst_left,
                 dst_top,
                 src_width,
                 src_height,
                 0,
                 1,
                 level,
                 face,
                 dst_pitch,
                 size,
                 buffer);
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBECB::PlatformSpecificLock(
    CubeFace face, int level, void** data, int* pitch,
    Texture::AccessMode mode) {
  DCHECK(data);
  DCHECK(pitch);
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
  if (!backing_bitmap->image_data()) {
    DCHECK_EQ(has_levels_[face], 0);
    backing_bitmap->Allocate(format(), edge_length(), edge_length(), levels(),
                             Bitmap::IMAGE);
  }
  *data = backing_bitmap->GetMipData(level);
  unsigned int mip_width = image::ComputeMipDimension(level, edge_length());
  if (!IsCompressed()) {
    *pitch = image::ComputePitch(format(), mip_width);
  } else {
    unsigned blocks_across = (mip_width + 3) / 4;
    unsigned bytes_per_block = format() == Texture::DXT1 ? 8 : 16;
    unsigned bytes_per_row = bytes_per_block * blocks_across;
    *pitch = bytes_per_row;
  }
  if (mode != kWriteOnly && !HasLevel(face, level)) {
    DCHECK_EQ(backing_bitmap->width(), edge_length());
    DCHECK_EQ(backing_bitmap->height(), edge_length());
    DCHECK_EQ(backing_bitmap->format(), format());
    DCHECK_GT(backing_bitmap->num_mipmaps(), level);
    CopyBackResourceToBitmap(renderer_, resource_id_, level,
                             TextureCUBE::FACE_POSITIVE_X, *backing_bitmap);
    has_levels_[face] |= 1 << level;
  }
  locked_levels_[face] |= 1 << level;
  return false;
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBECB::PlatformSpecificUnlock(CubeFace face, int level) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
  DCHECK(backing_bitmap->image_data());
  DCHECK_EQ(backing_bitmap->width(), edge_length());
  DCHECK_EQ(backing_bitmap->height(), edge_length());
  DCHECK_EQ(backing_bitmap->format(), format());
  DCHECK_GT(backing_bitmap->num_mipmaps(), level);

  if (LockedMode(face, level) != kReadOnly) {
    DCHECK(HasLevel(face, level));
    UpdateResourceFromBitmap(renderer_, resource_id_, level, face,
                             *backing_bitmap);
  }
  locked_levels_[face] &= ~(1 << level);
  if (locked_levels_[face] == 0) {
    backing_bitmap->FreeData();
    has_levels_[face] = 0;
  }
  return false;
}

RenderSurface::Ref TextureCUBECB::PlatformSpecificGetRenderSurface(
    TextureCUBE::CubeFace face,
    int mip_level) {
  DCHECK_LT(mip_level, levels());
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }
  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  int edge = edge_length() >> mip_level;
  return RenderSurface::Ref(new RenderSurfaceCB(service_locator(),
                                                edge,
                                                edge,
                                                mip_level,
                                                face,
                                                this,
                                                renderer_));
}

const Texture::RGBASwizzleIndices& TextureCUBECB::GetABGR32FSwizzleIndices() {
  return g_cb_abgr32f_swizzle_indices;
}

}  // namespace o3d
