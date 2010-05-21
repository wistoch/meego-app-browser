// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/omx_video_decoder.h"

#include "base/callback.h"
#include "base/waitable_event.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/omx_video_decode_engine.h"

namespace media {

// static
FilterFactory* OmxVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<OmxVideoDecoder, OmxVideoDecodeEngine*>(
      new OmxVideoDecodeEngine());
}

// static
bool OmxVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  if (!format.GetAsString(MediaFormat::kMimeType, &mime_type) ||
      mime_type::kFFmpegVideo != mime_type) {
    return false;
  }

  // TODO(ajwong): Find a good way to white-list formats that OpenMAX can
  // handle.
  int codec_id;
  if (format.GetAsInteger(MediaFormat::kFFmpegCodecID, &codec_id) &&
      codec_id == CODEC_ID_H264) {
    return true;
  }

  return false;
}

OmxVideoDecoder::OmxVideoDecoder(OmxVideoDecodeEngine* engine)
    : VideoDecoderImpl(engine),
      omx_engine_(engine) {
#if defined(ENABLE_EGLIMAGE)
  supports_egl_image_ = true;
#else
  supports_egl_image_ = false;
#endif
}

OmxVideoDecoder::~OmxVideoDecoder() {
}

void OmxVideoDecoder::DoInitialize(DemuxerStream* demuxer_stream,
                                   bool* success,
                                   Task* done_cb) {
  if (supports_egl_image_)
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideoEglImage);

  VideoDecoderImpl::DoInitialize(demuxer_stream, success, done_cb);
}

void OmxVideoDecoder::Stop() {
  // TODO(ajwong): This is a total hack. Make async.
  base::WaitableEvent event(false, false);
  omx_engine_->Stop(NewCallback(&event, &base::WaitableEvent::Signal));
  event.Wait();
}

}  // namespace media
