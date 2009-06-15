// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mem_entry_impl.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/mem_backend_impl.h"

using base::Time;

namespace disk_cache {

MemEntryImpl::MemEntryImpl(MemBackendImpl* backend) {
  doomed_ = false;
  backend_ = backend;
  ref_count_ = 0;
  parent_ = NULL;
  next_ = NULL;
  prev_ = NULL;
  for (int i = 0; i < NUM_STREAMS; i++)
    data_size_[i] = 0;
}

MemEntryImpl::~MemEntryImpl() {
  for (int i = 0; i < NUM_STREAMS; i++)
    backend_->ModifyStorageSize(data_size_[i], 0);
  backend_->ModifyStorageSize(static_cast<int32>(key_.size()), 0);
}

bool MemEntryImpl::CreateEntry(const std::string& key) {
  key_ = key;
  last_modified_ = Time::Now();
  last_used_ = Time::Now();
  type_ = kParentEntry;
  Open();
  backend_->ModifyStorageSize(0, static_cast<int32>(key.size()));
  return true;
}

bool MemEntryImpl::CreateChildEntry(MemEntryImpl* parent) {
  parent_ = parent;
  last_modified_ = Time::Now();
  last_used_ = Time::Now();
  type_ = kChildEntry;
  // Insert this to the backend's ranking list.
  backend_->InsertIntoRankingList(this);
  return true;
}

void MemEntryImpl::Close() {
  // Only a parent entry can be closed.
  DCHECK(type_ == kParentEntry);
  ref_count_--;
  DCHECK(ref_count_ >= 0);
  if (!ref_count_ && doomed_)
    delete this;
}

void MemEntryImpl::Open() {
  // Only a parent entry can be opened.
  DCHECK(type_ == kParentEntry);
  ref_count_++;
  DCHECK(ref_count_ >= 0);
  DCHECK(!doomed_);
}

bool MemEntryImpl::InUse() {
  if (type_ == kParentEntry) {
    return ref_count_ > 0;
  } else {
    // A child entry is always not in use. The consequence is that a child entry
    // can always be evicted while the associated parent entry is currently in
    // used (i.e. opened).
    return false;
  }
}

void MemEntryImpl::Doom() {
  if (doomed_)
    return;
  if (type_ == kParentEntry) {
    // Perform internal doom from the backend if this is a parent entry.
    backend_->InternalDoomEntry(this);
  } else {
    // Manually detach from the parent entry and perform internal doom.
    backend_->RemoveFromRankingList(this);
    InternalDoom();
  }
}

void MemEntryImpl::InternalDoom() {
  doomed_ = true;
  if (!ref_count_) {
    if (type_ == kParentEntry) {
      // TODO(hclam): doom all child entries associated with this entry.
    } else {
      // TODO(hclam): detach this child entry from the parent entry.
    }
    delete this;
  }
}

std::string MemEntryImpl::GetKey() const {
  // A child entry doesn't have key so this method should not be called.
  DCHECK(type_ == kParentEntry);
  return key_;
}

Time MemEntryImpl::GetLastUsed() const {
  return last_used_;
}

Time MemEntryImpl::GetLastModified() const {
  return last_modified_;
}

int32 MemEntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= NUM_STREAMS)
    return 0;

  // TODO(hclam): handle the case when this is a parent entry and has associated
  // child entries.
  return data_size_[index];
}

int MemEntryImpl::ReadData(int index, int offset, net::IOBuffer* buf,
    int buf_len, net::CompletionCallback* completion_callback) {
  // This method can only be called with a parent entry.
  DCHECK(type_ == kParentEntry);

  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = GetDataSize(index);
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (offset + buf_len > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  memcpy(buf->data() , &(data_[index])[offset], buf_len);
  return buf_len;
}

int MemEntryImpl::WriteData(int index, int offset, net::IOBuffer* buf,
    int buf_len, net::CompletionCallback* completion_callback, bool truncate) {
  // This method can only be called with a parent entry.
  DCHECK(type_ == kParentEntry);

  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int max_file_size = backend_->MaxFileSize();

  // offset of buf_len could be negative numbers.
  if (offset > max_file_size || buf_len > max_file_size ||
      offset + buf_len > max_file_size) {
    int size = offset + buf_len;
    if (size <= max_file_size)
      size = kint32max;
    return net::ERR_FAILED;
  }

  // Read the size at this point.
  int entry_size = GetDataSize(index);

  PrepareTarget(index, offset, buf_len);

  if (entry_size < offset + buf_len) {
    backend_->ModifyStorageSize(entry_size, offset + buf_len);
    data_size_[index] = offset + buf_len;
  } else if (truncate) {
    if (entry_size > offset + buf_len) {
      backend_->ModifyStorageSize(entry_size, offset + buf_len);
      data_size_[index] = offset + buf_len;
    }
  }

  UpdateRank(true);

  if (!buf_len)
    return 0;

  memcpy(&(data_[index])[offset], buf->data(), buf_len);
  return buf_len;
}

int MemEntryImpl::ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                                 net::CompletionCallback* completion_callback) {
  return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
}

int MemEntryImpl::WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
    net::CompletionCallback* completion_callback) {
  return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
}

int MemEntryImpl::GetAvailableRange(int64 offset, int len, int64* start) {
  return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
}

void MemEntryImpl::PrepareTarget(int index, int offset, int buf_len) {
  int entry_size = GetDataSize(index);

  if (entry_size >= offset + buf_len)
    return;  // Not growing the stored data.

  if (static_cast<int>(data_[index].size()) < offset + buf_len)
    data_[index].resize(offset + buf_len);

  if (offset <= entry_size)
    return;  // There is no "hole" on the stored data.

  // Cleanup the hole not written by the user. The point is to avoid returning
  // random stuff later on.
  memset(&(data_[index])[entry_size], 0, offset - entry_size);
}

void MemEntryImpl::UpdateRank(bool modified) {
  Time current = Time::Now();
  last_used_ = current;

  if (modified)
    last_modified_ = current;

  if (!doomed_)
    backend_->UpdateRank(this);
}

}  // namespace disk_cache
