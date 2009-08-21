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

namespace {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kFtpScheme[] = "ftp";

// A helper method that accepts only HTTP, HTTPS and FILE protocol.
bool IsSchemeSupported(const GURL& url) {
  return url.SchemeIs(kHttpScheme) ||
         url.SchemeIs(kHttpsScheme) ||
         url.SchemeIs(kFtpScheme) ||
         url.SchemeIsFile();
}

}  // namespace

namespace webkit_glue {

SimpleDataSource::SimpleDataSource(
    MessageLoop* render_loop,
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory)
    : render_loop_(render_loop),
      bridge_factory_(bridge_factory),
      size_(-1),
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

void SimpleDataSource::Initialize(const std::string& url,
                                  media::FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  DCHECK(callback);
  state_ = INITIALIZING;
  initialize_callback_.reset(callback);

  // Validate the URL.
  SetURL(GURL(url));
  if (!url_.is_valid() || !IsSchemeSupported(url_)) {
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
    initialize_callback_->Run();
    initialize_callback_.reset();
    return;
  }

  host()->SetLoaded(url_.SchemeIsFile());

  // Post a task to the render thread to start loading the resource.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SimpleDataSource::StartTask));
}

const media::MediaFormat& SimpleDataSource::media_format() {
  return media_format_;
}

void SimpleDataSource::Read(int64 position,
                            size_t size,
                            uint8* data,
                            ReadCallback* read_callback) {
  DCHECK_GE(size_, 0);
  if (position >= size_) {
    read_callback->RunWithParams(Tuple1<size_t>(0));
    delete read_callback;
  } else {
    size_t copied = std::min(size, static_cast<size_t>(size_ - position));
    memcpy(data, data_.c_str() + position, copied);
    read_callback->RunWithParams(Tuple1<size_t>(copied));
    delete read_callback;
  }
}

bool SimpleDataSource::GetSize(int64* size_out) {
  *size_out = size_;
  return true;
}

bool SimpleDataSource::IsStreaming() {
  return false;
}

void SimpleDataSource::OnDownloadProgress(uint64 position, uint64 size) {}

void SimpleDataSource::OnUploadProgress(uint64 position, uint64 size) {}

bool SimpleDataSource::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info) {
  SetURL(new_url);
  return true;
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

  // We're initialized!
  if (status.is_success()) {
    state_ = INITIALIZED;
    host()->SetTotalBytes(size_);
    host()->SetBufferedBytes(size_);
  } else {
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
  }
  initialize_callback_->Run();
  initialize_callback_.reset();
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

  // We may have stopped.
  if (state_ == STOPPED)
    return;

  DCHECK_EQ(state_, INITIALIZING);

  // Create our bridge and start loading the resource.
  bridge_.reset(bridge_factory_->CreateBridge(
      url_, net::LOAD_BYPASS_CACHE, -1, -1));
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
