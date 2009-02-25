// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/data_buffer.h"

namespace media {

DataBuffer::DataBuffer(char* data, size_t buffer_size, size_t data_size,
                       const base::TimeDelta& timestamp,
                       const base::TimeDelta& duration)
    : data_(data),
      buffer_size_(buffer_size),
      data_size_(data_size) {
  DCHECK(data);
  DCHECK(buffer_size >= 0);
  DCHECK(data_size <= buffer_size);
  SetTimestamp(timestamp);
  SetDuration(duration);
}

DataBuffer::~DataBuffer() {
  delete [] data_;
}

const char* DataBuffer::GetData() const {
  return data_;
}

size_t DataBuffer::GetDataSize() const {
  return data_size_;
}

char* DataBuffer::GetWritableData() {
  return data_;
}

size_t DataBuffer::GetBufferSize() const {
  return buffer_size_;
}

void DataBuffer::SetDataSize(size_t data_size) {
  data_size_ = data_size;
}

}  // namespace media
