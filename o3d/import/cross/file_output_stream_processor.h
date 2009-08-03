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


// This file contains the declaration of class FileOutputStreamProcessor.

#ifndef O3D_IMPORT_CROSS_FILE_OUTPUT_STREAM_PROCESSOR_H_
#define O3D_IMPORT_CROSS_FILE_OUTPUT_STREAM_PROCESSOR_H_

#include "import/cross/memory_stream.h"

namespace o3d {

// A StringReader accepts binary data and writes it to a file.
class FileOutputStreamProcessor : public StreamProcessor {
 public:
  explicit FileOutputStreamProcessor(FILE* file);

  virtual Status ProcessBytes(MemoryReadStream *stream,
                              size_t bytes_to_process);
  virtual void Close(bool success);

 private:
  FILE* file_;
  DISALLOW_COPY_AND_ASSIGN(FileOutputStreamProcessor);
};
}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_FILE_OUTPUT_STREAM_PROCESSOR_H_
