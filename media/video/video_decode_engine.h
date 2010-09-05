// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/video_frame.h"

namespace media {

class Buffer;

enum VideoCodec {
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
};

static const uint32 kProfileDoNotCare = static_cast<uint32>(-1);
static const uint32 kLevelDoNotCare = static_cast<uint32>(-1);

struct VideoCodecConfig {
  VideoCodecConfig() : codec_(kCodecH264),
                       profile_(kProfileDoNotCare),
                       level_(kLevelDoNotCare),
                       width_(0),
                       height_(0),
                       opaque_context_(NULL) {}

  VideoCodec codec_;

  // TODO(jiesun): video profile and level are specific to individual codec.
  // Define enum to.
  uint32 profile_;
  uint32 level_;

  // Container's concept of width and height of this video.
  int32 width_;
  int32 height_;  // TODO(jiesun): Do we allow height to be negative to
                  // indicate output is upside-down?

  // FFMPEG's will use this to pass AVStream. Otherwise, we should remove this.
  void* opaque_context_;
};

struct VideoStreamInfo {
  VideoFrame::Format surface_format_;
  VideoFrame::SurfaceType surface_type_;
  uint32 surface_width_;  // Can be different with container's value.
  uint32 surface_height_;  // Can be different with container's value.
};

struct VideoCodecInfo {
  // Other parameter is only meaningful when this is true.
  bool success_;

  // Whether decoder provides output buffer pool.
  bool provides_buffers_;

  // Initial Stream Info. Only part of them could be valid.
  // If they are not valid, Engine should update with OnFormatChange.
  VideoStreamInfo stream_info_;
};

class VideoDecodeEngine {
 public:
  struct EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnInitializeComplete(const VideoCodecInfo& info) = 0;
    virtual void OnUninitializeComplete() = 0;
    virtual void OnFlushComplete() = 0;
    virtual void OnSeekComplete() = 0;
    virtual void OnError() = 0;
    virtual void OnFormatChange(VideoStreamInfo stream_info) = 0;

    // TODO(hclam): The following two methods shouldn't belong to this class
    // because they are not video decode events but used to send decoded
    // video frames and request video packets.
    //
    // Signal the user of VideoDecodeEngine to provide a video sample.
    //
    // In the normal running state, this method is called by the video decode
    // engine to request video samples used for decoding.
    //
    // In the case when the video decode engine is flushing, this method is
    // called to return video samples acquired by the video decode engine.
    //
    // |buffer| can be NULL in which case this method call is purely for
    // requesting new video samples. If |buffer| is non-NULL, the buffer is
    // returned to the owner at the sample time as a request for video sample
    // is made.
    virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer) = 0;

    // Signal the user of VideoDecodeEngine that a video frame is ready to
    // be consumed or a video frame is returned to the owner.
    //
    // In the normal running state, this method is called to signal that
    // |frame| contains a decoded video frame and is ready to be used.
    //
    // In the case of flushing and video frame is provided externally, this
    // method is called to return the video frame object to the owner.
    // The content of the video frame may be invalid.
    virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame) = 0;
  };

  virtual ~VideoDecodeEngine() {}

  // Initialized the engine with specified configuration. |message_loop| could
  // be NULL if every operation is synchronous. Engine should call the
  // EventHandler::OnInitializeDone() no matter finished successfully or not.
  // TODO(jiesun): remove message_loop and create thread inside openmax engine?
  // or create thread in GpuVideoDecoder and pass message loop here?
  virtual void Initialize(MessageLoop* message_loop,
                          EventHandler* event_handler,
                          const VideoCodecConfig& config) = 0;

  // Uninitialize the engine. Engine should destroy all resources and call
  // EventHandler::OnUninitializeComplete().
  virtual void Uninitialize() = 0;

  // Flush the engine. Engine should return all the buffers to owner ( which
  // could be itself. ) then call EventHandler::OnFlushDone().
  virtual void Flush() = 0;

  // This method is used as a signal for the decode engine to prefoll and
  // issue read requests after Flush() us made.
  virtual void Seek() = 0;

  // Provide a video sample to be used by the video decode engine.
  //
  // This method is called in response to ProvideVideoSample() called to the
  // user.
  virtual void ConsumeVideoSample(scoped_refptr<Buffer> buffer) = 0;

  // Signal the video decode engine to produce a video frame or return the
  // video frame object to the video decode engine.
  //
  // In the normal running state, this method is called by the user of the
  // video decode engine to request a decoded video frame. If |frame| is
  // NULL the video decode engine should allocate a video frame object.
  // Otherwise video decode engine should try to use the video frame object
  // provided as output.
  //
  // In flushing state and video frames are allocated internally this method
  // is called by the user to return the video frame object.
  //
  // In response to this method call, ConsumeVideoFrame() is called with a
  // video frame object containing decoded video content.
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
