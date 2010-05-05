// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_wtl/wtl_renderer.h"

#include "media/tools/player_wtl/view.h"

WtlVideoRenderer::WtlVideoRenderer(WtlVideoWindow* window)
    : window_(window) {
}

WtlVideoRenderer::~WtlVideoRenderer() {
}

// static
bool WtlVideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  bool uses_egl_image = false;
  return ParseMediaFormat(media_format, &width, &height, &uses_egl_image);
}

void WtlVideoRenderer::OnStop() {
}

bool WtlVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  int width = 0;
  int height = 0;
  bool uses_egl_image = false;
  if (!ParseMediaFormat(decoder->media_format(), &width, &height,
                        &uses_egl_image))
    return false;
  window_->SetSize(width, height);
  return true;
}

void WtlVideoRenderer::OnFrameAvailable() {
  window_->Invalidate();
}
