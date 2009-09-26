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


// This file contains definitions for resource flags, enums, and helper
// functions.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_RESOURCE_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_RESOURCE_H_

#include <algorithm>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/logging.h"

namespace o3d {
namespace command_buffer {

// A resource ID, key to the resource maps.
typedef uint32 ResourceId;
// Invalid resource ID.
static const ResourceId kInvalidResource = 0xffffffffU;

namespace vertex_buffer {
// Vertex buffer flags.
enum Flags {
  kNone = 0x00,
  kDynamic = 0x01,  // This vertex buffer is dynamic and is expected to have
                    // its data updated often.
};
}  // namespace vertex_buffer

namespace index_buffer {
// Index buffer flags.
enum Flags {
  kNone = 0x00,
  kDynamic = 0x01,  // This index buffer is dynamic and is expected to have
                    // its data updated often.
  kIndex32Bit = 0x02,  // Indices contained in this index buffer are 32 bits
                       // (unsigned int) instead of 16 bit (unsigned short).
};
}  // namespace index_buffer

namespace vertex_struct {
// Semantics for input data.
enum Semantic {
  kUnknownSemantic = -1,
  kPosition = 0,
  kNormal,
  kColor,
  kTexCoord,
  kNumSemantics
};

// Input data types.
enum Type {
  kFloat1,
  kFloat2,
  kFloat3,
  kFloat4,
  kUChar4N,
  kNumTypes
};
}  // namespace vertex_struct

namespace effect_param {
enum DataType {
  kUnknown,  // A parameter exists in the effect, but the type is not
  // representable (e.g. MATRIX3x4).
  kFloat1,
  kFloat2,
  kFloat3,
  kFloat4,
  kMatrix4,
  kInt,
  kBool,
  kSampler,
  kTexture,
  kNumTypes,
  kMake32Bit = 0x7fffffff,
};
COMPILE_ASSERT(sizeof(DataType) == 4, DataType_should_be_32_bits);

// Gets the size of the data of a particular type.
unsigned int GetDataSize(DataType type);

// Structure describing a parameter, filled in by the
// GAPIInterface::GetParamDesc call.
struct Desc {
  Uint32 size;             // the total memory size needed for the complete
                           // description.
  Uint32 name_offset;      // the offset of the parameter name, relative to
                           // the beginning of the structure. May be 0 if the
                           // name doesn't fit into the memory buffer.
  Uint32 name_size;        // the size of the parameter name, including the
                           // terminating nul character. Will always be set
                           // even if the name doesn't fit into the buffer.
  Uint32 semantic_offset;  // the offset of the parameter semantic, relative
                           // to the beginning of the structure. May be 0 if
                           // the semantic doesn't fit into the memory
                           // buffer.
  Uint32 semantic_size;    // the size of the parameter semantic, including
                           // the terminating nul character. Will always be
                           // set even if the semantic doesn't fit into the
                           // buffer.
  Uint32 num_elements;     // the number of entries if the parameter is an array
                           // 0 otherwise.
  DataType data_type;      // the data type of the parameter.
  Uint32 data_size;        // the size of the parameter data, in bytes.
};
}  // namespace effect_param

namespace effect_stream {
struct Desc {
  Desc()
    : semantic(vertex_struct::kUnknownSemantic),
      semantic_index(0) {}
  Desc(Uint32 semantic, Uint32 semantic_index)
    : semantic(semantic),
      semantic_index(semantic_index) {}
  Uint32 semantic;         // the semantic type
  Uint32 semantic_index;
};
}  // namespace effect_stream

namespace texture {
// Texture flags.
enum Flags {
  kNone = 0x00,
  kDynamic = 0x01,  // This texture is dynamic and is expected to have
                    // its data updated often.
};

// Texel formats.
enum Format {
  kXRGB8,
  kARGB8,
  kABGR16F,
  kDXT1,
  kNumFormats
};

// Texture type.
enum Type {
  kTexture2d,
  kTexture3d,
  kTextureCube,
};

// Cube map face.
enum Face {
  kFacePositiveX,
  kFaceNegativeX,
  kFacePositiveY,
  kFaceNegativeY,
  kFacePositiveZ,
  kFaceNegativeZ,
  kFaceNone = kFacePositiveX,  // For non-cube maps.
};

// Gets the number of bytes per block for a given texture format. For most
// texture formats, a block is 1x1 texels, but DXT* formats have 4x4 blocks.
unsigned int GetBytesPerBlock(Format format);
// Gets the x dimension of a texel block for a given texture format. For most
// texture formats, a block is 1x1 texels, but DXT* formats have 4x4 blocks.
unsigned int GetBlockSizeX(Format format);
// Gets the y dimension of a texel block for a given texture format. For most
// texture formats, a block is 1x1 texels, but DXT* formats have 4x4 blocks.
unsigned int GetBlockSizeY(Format format);
// Gets the dimension of a mipmap level given the dimension of the base
// level. Every mipmap level is half the size of the previous level, rounding
// down.
static unsigned int GetMipMapDimension(unsigned int base,
                                       unsigned int level) {
  DCHECK_GT(base, 0U);
  return std::max(1U, base >> level);
}
}  // namespace texture

namespace sampler {
enum AddressingMode {
  kWrap,
  kMirrorRepeat,
  kClampToEdge,
  kClampToBorder,
  kNumAddressingMode
};

enum FilteringMode {
  kNone,
  kPoint,
  kLinear,
  kNumFilteringMode
};
}  // namespace sampler

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_RESOURCE_H_
