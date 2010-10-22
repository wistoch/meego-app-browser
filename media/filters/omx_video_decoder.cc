// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/omx_video_decoder.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/callback.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/video/omx_video_decode_engine.h"

namespace media {

// static
FilterFactory* OmxVideoDecoder::CreateFactory(
    VideoDecodeContext* decode_context) {
  return new FilterFactoryImpl2<OmxVideoDecoder,
                                VideoDecodeEngine*,
                                VideoDecodeContext*>(
      new OmxVideoDecodeEngine(), decode_context);
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

OmxVideoDecoder::OmxVideoDecoder(
    VideoDecodeEngine* engine,
    VideoDecodeContext* context)
    : decode_engine_(engine),
      decode_context_(context),
      width_(0), height_(0) {
  DCHECK(decode_engine_.get());
  memset(&info_, 0, sizeof(info_));
}

OmxVideoDecoder::~OmxVideoDecoder() {
  // TODO(hclam): Make sure OmxVideoDecodeEngine is stopped.
}

void OmxVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &OmxVideoDecoder::Initialize,
                          make_scoped_refptr(demuxer_stream),
                          callback));
    return;
  }

  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(!demuxer_stream_);
  DCHECK(!initialize_callback_.get());

  initialize_callback_.reset(callback);
  demuxer_stream_ = demuxer_stream;

  // We require bit stream converter for openmax hardware decoder.
  demuxer_stream->EnableBitstreamConverter();

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    VideoCodecInfo info = {0};
    OmxVideoDecoder::OnInitializeComplete(info);
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  // TODO(jiesun): shouldn't we check this in demuxer?
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  if (width_ > Limits::kMaxDimension ||
      height_ > Limits::kMaxDimension ||
      (width_ * height_) > Limits::kMaxCanvas) {
    VideoCodecInfo info = {0};
    OmxVideoDecoder::OnInitializeComplete(info);
    return;
  }

  VideoCodecConfig config;
  switch (av_stream->codec->codec_id) {
    case CODEC_ID_VC1:
      config.codec = kCodecVC1; break;
    case CODEC_ID_H264:
      config.codec = kCodecH264; break;
    case CODEC_ID_THEORA:
      config.codec = kCodecTheora; break;
    case CODEC_ID_MPEG2VIDEO:
      config.codec = kCodecMPEG2; break;
    case CODEC_ID_MPEG4:
      config.codec = kCodecMPEG4; break;
    default:
      NOTREACHED();
  }
  config.opaque_context = NULL;
  config.width = width_;
  config.height = height_;
  decode_engine_->Initialize(message_loop(), this, NULL, config);
}

void OmxVideoDecoder::OnInitializeComplete(const VideoCodecInfo& info) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(initialize_callback_.get());

  info_ = info;  // Save a copy.
  AutoCallbackRunner done_runner(initialize_callback_.release());

  if (info.success) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(MediaFormat::kHeight, height_);
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceType,
        static_cast<int>(info.stream_info.surface_type));
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceFormat,
        static_cast<int>(info.stream_info.surface_format));
  } else {
    host()->SetError(PIPELINE_ERROR_DECODE);
  }
}

void OmxVideoDecoder::Stop(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &OmxVideoDecoder::Stop,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(!uninitialize_callback_.get());

  uninitialize_callback_.reset(callback);
  decode_engine_->Uninitialize();
}

void OmxVideoDecoder::OnUninitializeComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(uninitialize_callback_.get());

  AutoCallbackRunner done_runner(uninitialize_callback_.release());

  // TODO(jiesun): Destroy the decoder context.
}

void OmxVideoDecoder::Flush(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &OmxVideoDecoder::Flush,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(!flush_callback_.get());

  flush_callback_.reset(callback);

  decode_engine_->Flush();
}


void OmxVideoDecoder::OnFlushComplete() {
  DCHECK(flush_callback_.get());

  AutoCallbackRunner done_runner(flush_callback_.release());
}

void OmxVideoDecoder::Seek(base::TimeDelta time,
                           FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
     message_loop()->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &OmxVideoDecoder::Seek,
                                                time,
                                                callback));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(!seek_callback_.get());

  seek_callback_.reset(callback);
  decode_engine_->Seek();
}

void OmxVideoDecoder::OnSeekComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(seek_callback_.get());

  AutoCallbackRunner done_runner(seek_callback_.release());
}

void OmxVideoDecoder::OnError() {
  NOTIMPLEMENTED();
}
void OmxVideoDecoder::OnFormatChange(VideoStreamInfo stream_info) {
  NOTIMPLEMENTED();
}

void OmxVideoDecoder::ProduceVideoSample(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Issue more demux.
  demuxer_stream_->Read(NewCallback(this, &OmxVideoDecoder::DemuxCompleteTask));
}

void OmxVideoDecoder::ConsumeVideoFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  VideoFrameReady(frame);
}

void OmxVideoDecoder::ProduceVideoFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK(decode_engine_.get());
  message_loop()->PostTask(
     FROM_HERE,
     NewRunnableMethod(decode_engine_.get(),
                       &VideoDecodeEngine::ProduceVideoFrame, frame));
}

bool OmxVideoDecoder::ProvidesBuffer() {
  DCHECK(info_.success);
  return info_.provides_buffers;
}

void OmxVideoDecoder::DemuxCompleteTask(Buffer* buffer) {
  // We simply delicate the buffer to the right message loop.
  scoped_refptr<Buffer> ref_buffer = buffer;
  DCHECK(decode_engine_.get());
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(decode_engine_.get(),
                        &VideoDecodeEngine::ConsumeVideoSample, ref_buffer));
}

}  // namespace media

// Disable refcounting for the decode engine because it only lives on the
// video decoder thread.
DISABLE_RUNNABLE_METHOD_REFCOUNT(media::VideoDecodeEngine);
