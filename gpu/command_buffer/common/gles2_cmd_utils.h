// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_

#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"

namespace gpu {
namespace gles2 {

// Utilties for GLES2 support.
class GLES2Util {
 public:
  explicit GLES2Util(
      int num_compressed_texture_formats)
      : num_compressed_texture_formats_(num_compressed_texture_formats) {
  }

  // Gets the number of values a particular id will return when a glGet
  // function is called. If 0 is returned the id is invalid.
  int GLGetNumValuesReturned(int id) const;

  // Computes the size of image data for TexImage2D and TexSubImage2D.
  static uint32 ComputeImageDataSize(
    int width, int height, int format, int type, int unpack_alignment);

  static uint32 GetGLDataTypeSizeForUniforms(int type);

  static size_t GetGLTypeSizeForTexturesAndBuffers(uint32 type);

  static uint32 GLErrorToErrorBit(uint32 gl_error);

  static uint32 GLErrorBitToGLError(uint32 error_bit);

 private:
  int num_compressed_texture_formats_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_

