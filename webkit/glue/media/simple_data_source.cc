// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process_util.h"
#include "media/base/filter_host.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webappcachecontext.h"

namespace webkit_glue {

SimpleDataSource::SimpleDataSource(MessageLoop* render_loop, int32 routing_id)
    : routing_id_(routing_id),
      render_loop_(render_loop),
      size_(-1),
      position_(0),
      state_(UNINITIALIZED) {
  DCHECK(render_loop);
}

SimpleDataSource::~SimpleDataSource() {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == UNINITIALIZED || state_ == STOPPED);
}

void SimpleDataSource::Stop() {
  AutoLock auto_lock(lock_);
  state_ = STOPPED;

  // Post a task to the render thread to cancel loading the resource.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SimpleDataSource::CancelTask));
}

bool SimpleDataSource::Initialize(const std::string& url) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  state_ = INITIALIZING;

  // Validate the URL.
  SetURL(GURL(url));
  if (!url_.is_valid()) {
    return false;
  }

  // Post a task to the render thread to start loading the resource.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SimpleDataSource::StartTask));
  return true;
}

const media::MediaFormat& SimpleDataSource::media_format() {
  return media_format_;
}

size_t SimpleDataSource::Read(uint8* data, size_t size) {
  DCHECK_GE(size_, 0);
  size_t copied = std::min(size, static_cast<size_t>(size_ - position_));
  memcpy(data, data_.c_str() + position_, copied);
  position_ += copied;
  return copied;
}

bool SimpleDataSource::GetPosition(int64* position_out) {
  *position_out = position_;
  return true;
}

bool SimpleDataSource::SetPosition(int64 position) {
  if (position < 0 || position > size_)
    return false;
  position_ = position;
  return true;
}

bool SimpleDataSource::GetSize(int64* size_out) {
  *size_out = size_;
  return true;
}

bool SimpleDataSource::IsSeekable() {
  return true;
}

void SimpleDataSource::OnDownloadProgress(uint64 position, uint64 size) {}

void SimpleDataSource::OnUploadProgress(uint64 position, uint64 size) {}

void SimpleDataSource::OnReceivedRedirect(const GURL& new_url) {
  SetURL(new_url);
}

void SimpleDataSource::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  size_ = info.content_length;
}

void SimpleDataSource::OnReceivedData(const char* data, int len) {
  data_.append(data, len);
}

void SimpleDataSource::OnCompletedRequest(const URLRequestStatus& status,
                                          const std::string& security_info) {
  AutoLock auto_lock(lock_);
  // It's possible this gets called after Stop(), in which case |host_| is no
  // longer valid.
  if (state_ == STOPPED) {
    return;
  }

  // Otherwise we should be initializing and have created a bridge.
  DCHECK_EQ(state_, INITIALIZING);
  DCHECK(bridge_.get());
  bridge_.reset();

  // If we don't get a content length or the request has failed, report it
  // as a network error.
  DCHECK(size_ == -1 || size_ == data_.length());
  if (size_ == -1) {
    size_ = data_.length();
  }
  if (!status.is_success()) {
    host_->Error(media::PIPELINE_ERROR_NETWORK);
    return;
  }

  // We're initialized!
  state_ = INITIALIZED;
  host_->SetTotalBytes(size_);
  host_->SetBufferedBytes(size_);
  host_->InitializationComplete();
}

std::string SimpleDataSource::GetURLForDebugging() {
  return url_.spec();
}

void SimpleDataSource::SetURL(const GURL& url) {
  url_ = url;
  media_format_.Clear();
  media_format_.SetAsString(media::MediaFormat::kMimeType,
                            media::mime_type::kApplicationOctetStream);
  media_format_.SetAsString(media::MediaFormat::kURL, url.spec());
}

void SimpleDataSource::StartTask() {
  AutoLock auto_lock(lock_);
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK_EQ(state_, INITIALIZING);

  // Create our bridge and start loading the resource.
  bridge_.reset(webkit_glue::ResourceLoaderBridge::Create(
      "GET",
      url_,
      url_,
      GURL::EmptyGURL(),  // TODO(scherkus): provide referer here.
      "null",             // TODO(abarth): provide frame_origin
      "null",             // TODO(abarth): provide main_frame_origin
      "",
      net::LOAD_BYPASS_CACHE,
      base::GetCurrentProcId(),
      ResourceType::MEDIA,
      // TODO(michaeln): delegate->mediaplayer->frame->
      //                    app_cache_context()->context_id()
      // For now don't service media resource requests from the appcache.
      WebAppCacheContext::kNoAppCacheContextId,
      routing_id_));
  bridge_->Start(this);
}

void SimpleDataSource::CancelTask() {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, STOPPED);

  // Cancel any pending requests.
  if (bridge_.get()) {
    bridge_->Cancel();
    bridge_.reset();
  }
}

}  // namespace webkit_glue
