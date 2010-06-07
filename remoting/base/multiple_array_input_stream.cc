// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/logging.h"
#include "remoting/base/multiple_array_input_stream.h"

namespace remoting {

MultipleArrayInputStream::MultipleArrayInputStream(int count)
    : buffer_count_(count),
      current_buffer_(0),
      current_buffer_offset_(0),
      position_(0),
      last_returned_size_(0) {
  DCHECK_GT(buffer_count_, 0);
  buffers_.reset(new const uint8*[buffer_count_]);
  buffer_sizes_.reset(new int[buffer_count_]);
}

MultipleArrayInputStream::~MultipleArrayInputStream() {
}

bool MultipleArrayInputStream::Next(const void** data, int* size) {
  if (current_buffer_ < buffer_count_) {
    // Also reply with that is remaining in the current buffer.
    last_returned_size_ =
        buffer_sizes_[current_buffer_] - current_buffer_offset_;
    *data = buffers_[current_buffer_] + current_buffer_offset_;
    *size = last_returned_size_;

    // After reading the current buffer then advance to the next buffer.
    current_buffer_offset_ = 0;
    ++current_buffer_;
    position_ += last_returned_size_;
    return true;
  }

  // We've reached the end of the stream. So reset |last_returned_size_|
  // to zero to prevent any backup request.
  // This is the same as in ArrayInputStream.
  // See google/protobuf/io/zero_copy_stream_impl_lite.cc.
  last_returned_size_ = 0;
  return false;
}

void MultipleArrayInputStream::BackUp(int count) {
  DCHECK_LE(count, last_returned_size_);
  DCHECK_EQ(0, current_buffer_offset_);
  DCHECK_GT(current_buffer_, 0);

  // Rewind one buffer.
  --current_buffer_;
  current_buffer_offset_ = buffer_sizes_[current_buffer_] - count;
  position_ -= count;
  DCHECK_GE(current_buffer_offset_, 0);
  DCHECK_GE(position_, 0);
}

bool MultipleArrayInputStream::Skip(int count) {
  DCHECK_GE(count, 0);
  last_returned_size_ = 0;

  while (count && current_buffer_ < buffer_count_) {
    int read = std::min(
        count,
        buffer_sizes_[current_buffer_] - current_buffer_offset_);

    // Advance the current buffer offset and position.
    current_buffer_offset_ += read;
    position_ += read;
    count -= read;

    // If the current buffer is fully read, then advance to the next buffer.
    if (current_buffer_offset_ == buffer_sizes_[current_buffer_]) {
      ++current_buffer_;
      current_buffer_offset_ = 0;
    }
  }
  return count == 0;
}

int64 MultipleArrayInputStream::ByteCount() const {
  return position_;
}

void MultipleArrayInputStream::SetBuffer(
    int n, const uint8* buffer, int size) {
  CHECK(n < buffer_count_);
  buffers_[n] = buffer;
  buffer_sizes_[n] = size;
}

}  // namespace remoting
