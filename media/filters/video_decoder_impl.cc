// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_decoder_impl.h"

#include "base/task.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/video_decode_engine.h"
#include "media/ffmpeg/ffmpeg_util.h"

namespace media {

VideoDecoderImpl::VideoDecoderImpl(VideoDecodeEngine* engine)
    : width_(0),
      height_(0),
      time_base_(new AVRational()),
      state_(kNormal),
      decode_engine_(engine) {
}

VideoDecoderImpl::~VideoDecoderImpl() {
}

void VideoDecoderImpl::DoInitialize(DemuxerStream* demuxer_stream,
                                    bool* success,
                                    Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);
  *success = false;

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  *time_base_ = av_stream->time_base;

  // TODO(ajwong): We don't need these extra variables if |media_format_| has
  // them.  Remove.
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  if (width_ > Limits::kMaxDimension ||
      height_ > Limits::kMaxDimension ||
      (width_ * height_) > Limits::kMaxCanvas) {
    return;
  }

  media_format_.SetAsString(MediaFormat::kMimeType,
                            mime_type::kUncompressedVideo);
  media_format_.SetAsInteger(MediaFormat::kWidth, width_);
  media_format_.SetAsInteger(MediaFormat::kHeight, height_);

  decode_engine_->Initialize(
      av_stream,
      NewRunnableMethod(this,
                        &VideoDecoderImpl::OnInitializeComplete,
                        success,
                        done_runner.release()));
}

void VideoDecoderImpl::OnInitializeComplete(bool* success, Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  *success = decode_engine_->state() == VideoDecodeEngine::kNormal;
}

void VideoDecoderImpl::DoSeek(base::TimeDelta time, Task* done_cb) {
  // Everything in the presentation time queue is invalid, clear the queue.
  while (!pts_heap_.IsEmpty())
    pts_heap_.Pop();

  // We're back where we started.  It should be completely safe to flush here
  // since DecoderBase uses |expecting_discontinuous_| to verify that the next
  // time DoDecode() is called we will have a discontinuous buffer.
  //
  // TODO(ajwong): Should we put a guard here to prevent leaving kError.
  state_ = kNormal;

  decode_engine_->Flush(done_cb);
}

void VideoDecoderImpl::DoDecode(Buffer* buffer, Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  // TODO(ajwong): This DoDecode() and OnDecodeComplete() set of functions is
  // too complicated to easily unittest.  The test becomes fragile. Try to
  // find a way to reorganize into smaller units for testing.

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each read is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_video2
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->IsEndOfStream() is first true.
  // kNormal -> kDecodeFinished:
  //     A catastrophic failure occurs, and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data or errors out.
  // (any state) -> kNormal:
  //     Any time buffer->IsDiscontinuous() is true.
  //
  // If the decoding is finished, we just always return empty frames.
  if (state_ == kDecodeFinished) {
    EnqueueEmptyFrame();
    return;
  }

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  // Push all incoming timestamps into the priority queue as long as we have
  // not yet received an end of stream buffer.  It is important that this line
  // stay below the state transition into kFlushCodec done above.
  //
  // TODO(ajwong): This push logic, along with the pop logic below needs to
  // be reevaluated to correctly handle decode errors.
  if (state_ == kNormal) {
    pts_heap_.Push(buffer->GetTimestamp());
  }

  // Otherwise, attempt to decode a single frame.
  AVFrame* yuv_frame = avcodec_alloc_frame();
  bool* got_frame = new bool;
  decode_engine_->DecodeFrame(
      buffer,
      yuv_frame,
      got_frame,
      NewRunnableMethod(this,
                        &VideoDecoderImpl::OnDecodeComplete,
                        yuv_frame,
                        got_frame,
                        done_runner.release()));
}

void VideoDecoderImpl::OnDecodeComplete(AVFrame* yuv_frame, bool* got_frame,
                                        Task* done_cb) {
  // Note: The |done_runner| must be declared *last* to ensure proper
  // destruction order.
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> yuv_frame_deleter(yuv_frame);
  scoped_ptr<bool> got_frame_deleter(got_frame);
  AutoTaskRunner done_runner(done_cb);

  // If we actually got data back, enqueue a frame.
  if (*got_frame) {
    last_pts_ = FindPtsAndDuration(*time_base_, pts_heap_, last_pts_,
                                   yuv_frame);

    // Pop off a pts on a successful decode since we are "using up" one
    // timestamp.
    //
    // TODO(ajwong): Do we need to pop off a pts when avcodec_decode_video2()
    // returns < 0?  The rationale is that when get_picture_ptr == 0, we skip
    // popping a pts because no frame was produced.  However, when
    // avcodec_decode_video2() returns false, it is a decode error, which
    // if it means a frame is dropped, may require us to pop one more time.
    if (!pts_heap_.IsEmpty()) {
      pts_heap_.Pop();
    } else {
      NOTREACHED() << "Attempting to decode more frames than were input.";
    }

    if (!EnqueueVideoFrame(
            decode_engine_->GetSurfaceFormat(), last_pts_, yuv_frame)) {
      // On an EnqueueEmptyFrame error, error out the whole pipeline and
      // set the state to kDecodeFinished.
      SignalPipelineError();
    }
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      EnqueueEmptyFrame();
    }
  }
}

bool VideoDecoderImpl::EnqueueVideoFrame(VideoFrame::Format surface_format,
                                         const TimeTuple& time,
                                         const AVFrame* frame) {
  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!frame->data[VideoFrame::kYPlane] ||
      !frame->data[VideoFrame::kUPlane] ||
      !frame->data[VideoFrame::kVPlane]) {
    return true;
  }

  scoped_refptr<VideoFrame> video_frame;
  VideoFrame::CreateFrame(surface_format, width_, height_,
                          time.timestamp, time.duration, &video_frame);
  if (!video_frame) {
    return false;
  }

  // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
  // output, meaning the data is only valid until the next
  // avcodec_decode_video() call.
  // TODO(scherkus): figure out pre-allocation/buffer cycling scheme.
  // TODO(scherkus): is there a cleaner way to figure out the # of planes?
  CopyPlane(VideoFrame::kYPlane, *video_frame, frame);
  CopyPlane(VideoFrame::kUPlane, *video_frame, frame);
  CopyPlane(VideoFrame::kVPlane, *video_frame, frame);
  EnqueueResult(video_frame);
  return true;
}

void VideoDecoderImpl::CopyPlane(size_t plane,
                                 const VideoFrame& video_frame,
                                 const AVFrame* frame) {
  DCHECK(video_frame.width() % 2 == 0);
  const uint8* source = frame->data[plane];
  const size_t source_stride = frame->linesize[plane];
  uint8* dest = video_frame.data(plane);
  const size_t dest_stride = video_frame.stride(plane);
  size_t bytes_per_line = video_frame.width();
  size_t copy_lines = video_frame.height();
  if (plane != VideoFrame::kYPlane) {
    bytes_per_line /= 2;
    if (video_frame.format() == VideoFrame::YV12) {
      copy_lines = (copy_lines + 1) / 2;
    }
  }
  DCHECK(bytes_per_line <= source_stride && bytes_per_line <= dest_stride);
  for (size_t i = 0; i < copy_lines; ++i) {
    memcpy(dest, source, bytes_per_line);
    source += source_stride;
    dest += dest_stride;
  }
}

void VideoDecoderImpl::EnqueueEmptyFrame() {
  scoped_refptr<VideoFrame> video_frame;
  VideoFrame::CreateEmptyFrame(&video_frame);
  EnqueueResult(video_frame);
}

VideoDecoderImpl::TimeTuple VideoDecoderImpl::FindPtsAndDuration(
    const AVRational& time_base,
    const PtsHeap& pts_heap,
    const TimeTuple& last_pts,
    const AVFrame* frame) {
  TimeTuple pts;

  // Default |repeat_pict| to 0 because if there is no frame information,
  // we just assume the frame only plays for one time_base.
  int repeat_pict = 0;

  // First search the AVFrame for the pts. This is the most authoritative.
  // Make a special exclusion for the value frame->pts == 0.  Though this
  // is technically a valid value, it seems a number of ffmpeg codecs will
  // mistakenly always set frame->pts to 0.
  //
  // Oh, and we have to cast AV_NOPTS_VALUE since it ends up becoming unsigned
  // because the value they use doesn't fit in a signed 64-bit number which
  // produces a signedness comparison warning on gcc.
  if (frame &&
      (frame->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) &&
      (frame->pts != 0)) {
    pts.timestamp = ConvertTimestamp(time_base, frame->pts);
    repeat_pict = frame->repeat_pict;
  } else if (!pts_heap.IsEmpty()) {
    // If the frame did not have pts, try to get the pts from the
    // |pts_heap|.
    pts.timestamp = pts_heap.Top();
  } else {
    DCHECK(last_pts.timestamp != StreamSample::kInvalidTimestamp);
    DCHECK(last_pts.duration != StreamSample::kInvalidTimestamp);
    // Unable to read the pts from anywhere. Time to guess.
    pts.timestamp = last_pts.timestamp + last_pts.duration;
  }

  // Fill in the duration while accounting for repeated frames.
  //
  // TODO(ajwong): Make sure this formula is correct.
  pts.duration = ConvertTimestamp(time_base, 1 + repeat_pict);

  return pts;
}

void VideoDecoderImpl::SignalPipelineError() {
  host()->SetError(PIPELINE_ERROR_DECODE);
  state_ = kDecodeFinished;
}

void VideoDecoderImpl::SetVideoDecodeEngineForTest(
    VideoDecodeEngine* engine) {
  decode_engine_.reset(engine);
}

}  // namespace media
