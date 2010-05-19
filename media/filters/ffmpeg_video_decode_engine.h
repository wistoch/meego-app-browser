// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_

#include "base/scoped_ptr.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/video_decode_engine.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;
struct AVStream;

namespace media {

class InputBuffer;
class OmxCodec;

class FFmpegVideoDecodeEngine : public VideoDecodeEngine {
 public:
  FFmpegVideoDecodeEngine();
  virtual ~FFmpegVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(AVStream* stream, Task* done_cb);
  virtual void DecodeFrame(Buffer* buffer,
                           scoped_refptr<VideoFrame>* video_frame,
                           bool* got_result, Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoFrame::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  virtual AVCodecContext* codec_context() const { return codec_context_; }

  virtual void SetCodecContextForTest(AVCodecContext* context) {
    codec_context_ = context;
  }

 private:
  AVCodecContext* codec_context_;
  State state_;
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> av_frame_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
