// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/audio_renderer_impl.h"

AudioRendererImpl::AudioRendererImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate) {
}

AudioRendererImpl::~AudioRendererImpl() {
}

void AudioRendererImpl::Stop() {
  // TODO(scherkus): implement Stop.
  NOTIMPLEMENTED();
}

bool AudioRendererImpl::Initialize(media::AudioDecoder* decoder) {
  // TODO(scherkus): implement Initialize.
  NOTIMPLEMENTED();
  return false;
}

void AudioRendererImpl::SetVolume(float volume) {
  // TODO(scherkus): implement SetVolume.
  NOTIMPLEMENTED();
}

bool AudioRendererImpl::IsMediaFormatSupported(
    const media::MediaFormat* format) {
  // TODO(hclam): check the format correct.
  return true;
}
