// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_

#include <functional>
#include <list>
#include <vector>

#include "base/callback.h"
#include "base/lock.h"
#include "base/task.h"
#include "media/filters/video_decode_engine.h"
#include "media/omx/omx_codec.h"

class MessageLoop;

// FFmpeg types.
struct AVRational;
struct AVStream;

namespace media {

class OmxVideoDecodeEngine : public VideoDecodeEngine {
 public:
  OmxVideoDecodeEngine();
  virtual ~OmxVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(MessageLoop* message_loop,
                          AVStream* av_stream,
                          EmptyThisBufferCallback* empty_buffer_callback,
                          FillThisBufferCallback* fill_buffer_callback,
                          Task* done_cb);
  virtual void EmptyThisBuffer(scoped_refptr<Buffer> buffer);
  virtual void Flush(Task* done_cb);
  virtual VideoFrame::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  // Stops the engine.
  //
  // TODO(ajwong): Normalize this interface with Task like the others, and
  // promote to the abstract interface.
  virtual void Stop(Callback0::Type* done_cb);

 private:
  virtual void OnFeedDone(scoped_refptr<Buffer> buffer);
  virtual void OnHardwareError();
  virtual void OnReadComplete(OMX_BUFFERHEADERTYPE* buffer);
  virtual void OnFormatChange(
      const OmxConfigurator::MediaFormat& input_format,
      const OmxConfigurator::MediaFormat& output_format);

  State state_;
  size_t width_;
  size_t height_;

  scoped_refptr<media::OmxCodec> omx_codec_;
  scoped_ptr<media::OmxConfigurator> omx_configurator_;
  scoped_ptr<EmptyThisBufferCallback> empty_this_buffer_callback_;
  scoped_ptr<FillThisBufferCallback> fill_this_buffer_callback_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
