// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <deque>

#include "base/task.h"
#include "media/base/callback.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decode_engine.h"
#include "media/filters/video_decode_engine.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(VideoDecodeEngine* engine)
    : width_(0),
      height_(0),
      time_base_(new AVRational()),
      state_(kUnInitialized),
      decode_engine_(engine),
      pending_reads_(0),
      pending_requests_(0) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
}

void FFmpegVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                    FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Initialize,
                                               demuxer_stream,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK(!demuxer_stream_);

  demuxer_stream_ = demuxer_stream;

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    FFmpegVideoDecoder::OnInitializeComplete(callback);
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  time_base_->den = av_stream->r_frame_rate.num;
  time_base_->num = av_stream->r_frame_rate.den;

  // TODO(ajwong): We don't need these extra variables if |media_format_| has
  // them.  Remove.
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  if (width_ > Limits::kMaxDimension ||
      height_ > Limits::kMaxDimension ||
      (width_ * height_) > Limits::kMaxCanvas) {
    FFmpegVideoDecoder::OnInitializeComplete(callback);
    return;
  }

  decode_engine_->Initialize(
      message_loop(),
      av_stream,
      NewCallback(this, &FFmpegVideoDecoder::OnEngineEmptyBufferDone),
      NewCallback(this, &FFmpegVideoDecoder::OnEngineFillBufferDone),
      NewRunnableMethod(this,
                        &FFmpegVideoDecoder::OnInitializeComplete,
                        callback));
}

void FFmpegVideoDecoder::OnInitializeComplete(FilterCallback* callback) {
  CHECK_EQ(MessageLoop::current(), message_loop());

  AutoCallbackRunner done_runner(callback);

  bool success = decode_engine_->state() == VideoDecodeEngine::kNormal;
  if (success) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(MediaFormat::kHeight, height_);
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceType,
        static_cast<int>(VideoFrame::TYPE_SYSTEM_MEMORY));
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceFormat,
        static_cast<int>(decode_engine_->GetSurfaceFormat()));
    state_ = kNormal;
  } else {
    host()->SetError(PIPELINE_ERROR_DECODE);
  }
}

void FFmpegVideoDecoder::Stop(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Stop,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());

  decode_engine_->Stop(
      NewRunnableMethod(this, &FFmpegVideoDecoder::OnStopComplete, callback));
}

void FFmpegVideoDecoder::OnStopComplete(FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AutoCallbackRunner done_runner(callback);
  state_ = kStopped;
}

void FFmpegVideoDecoder::Flush(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Flush,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());

  // Everything in the presentation time queue is invalid, clear the queue.
  while (!pts_heap_.IsEmpty())
    pts_heap_.Pop();

  decode_engine_->Flush(
      NewRunnableMethod(this, &FFmpegVideoDecoder::OnFlushComplete, callback));
}

void FFmpegVideoDecoder::OnFlushComplete(FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AutoCallbackRunner done_runner(callback);
}

void FFmpegVideoDecoder::Seek(base::TimeDelta time,
                              FilterCallback* callback) {
  if (MessageLoop::current() != message_loop()) {
     message_loop()->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &FFmpegVideoDecoder::Seek,
                                                time,
                                                callback));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  DCHECK_EQ(0u, pending_requests_) << "Pending requests should be empty";

  decode_engine_->Seek(
      NewRunnableMethod(this, &FFmpegVideoDecoder::OnSeekComplete, callback));
}

void FFmpegVideoDecoder::OnSeekComplete(FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AutoCallbackRunner done_runner(callback);
  state_ = kNormal;
}

void FFmpegVideoDecoder::OnReadComplete(Buffer* buffer_in) {
  scoped_refptr<Buffer> buffer = buffer_in;
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &FFmpegVideoDecoder::OnReadCompleteTask,
                        buffer));
}

void FFmpegVideoDecoder::OnReadCompleteTask(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK_GT(pending_reads_, 0u);

  --pending_reads_;

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
  if (state_ == kDecodeFinished || state_ == kStopped) {
    DCHECK(buffer->IsEndOfStream());

    // Signal VideoRenderer the end of the stream event.
    scoped_refptr<VideoFrame> video_frame;
    VideoFrame::CreateEmptyFrame(&video_frame);
    fill_buffer_done_callback()->Run(video_frame);
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
  if (state_ == kNormal && !buffer->IsEndOfStream() &&
      buffer->GetTimestamp() != StreamSample::kInvalidTimestamp) {
    pts_heap_.Push(buffer->GetTimestamp());
  }

  // Otherwise, attempt to decode a single frame.
  decode_engine_->EmptyThisBuffer(buffer);
}

void FFmpegVideoDecoder::FillThisBuffer(
    scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FFmpegVideoDecoder::FillThisBuffer,
                          video_frame));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop());

  // Synchronized flushing before stop should prevent this.
  if (state_ == kStopped)
    return;  // Discard the video frame.

  // Notify decode engine the available of new frame.
  ++pending_requests_;
  decode_engine_->FillThisBuffer(video_frame);
}

void FFmpegVideoDecoder::OnEngineFillBufferDone(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  // TODO(jiesun): Flush before stop will prevent this from happening.
  if (state_ == kStopped)
    return;  // Discard the video frame.

  if (video_frame.get()) {
    // If we actually got data back, enqueue a frame.
    last_pts_ = FindPtsAndDuration(*time_base_, &pts_heap_, last_pts_,
                                   video_frame.get());

    video_frame->SetTimestamp(last_pts_.timestamp);
    video_frame->SetDuration(last_pts_.duration);

    // Deliver this frame to VideoRenderer.
    --pending_requests_;
    fill_buffer_done_callback()->Run(video_frame);
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;

      // Signal VideoRenderer the end of the stream event.
      scoped_refptr<VideoFrame> video_frame;
      VideoFrame::CreateEmptyFrame(&video_frame);
      fill_buffer_done_callback()->Run(video_frame);
    }
  }
}

void FFmpegVideoDecoder::OnEngineEmptyBufferDone(
    scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  DCHECK_LE(pending_reads_, pending_requests_);

  if (state_ != kDecodeFinished) {
    demuxer_stream_->Read(
        NewCallback(this, &FFmpegVideoDecoder::OnReadComplete));
    ++pending_reads_;
  }
}

FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoder::FindPtsAndDuration(
    const AVRational& time_base,
    PtsHeap* pts_heap,
    const TimeTuple& last_pts,
    const VideoFrame* frame) {
  TimeTuple pts;

  // First search the VideoFrame for the pts. This is the most authoritative.
  // Make a special exclusion for the value pts == 0.  Though this is
  // technically a valid value, it seems a number of FFmpeg codecs will
  // mistakenly always set pts to 0.
  //
  // TODO(scherkus): FFmpegVideoDecodeEngine should be able to detect this
  // situation and set the timestamp to kInvalidTimestamp.
  DCHECK(frame);
  base::TimeDelta timestamp = frame->GetTimestamp();
  if (timestamp != StreamSample::kInvalidTimestamp &&
      timestamp.ToInternalValue() != 0) {
    pts.timestamp = timestamp;
    // We need to clean up the timestamp we pushed onto the |pts_heap|.
    if (!pts_heap->IsEmpty())
      pts_heap->Pop();
  } else if (!pts_heap->IsEmpty()) {
    // If the frame did not have pts, try to get the pts from the |pts_heap|.
    pts.timestamp = pts_heap->Top();
    pts_heap->Pop();
  } else if (last_pts.timestamp != StreamSample::kInvalidTimestamp &&
             last_pts.duration != StreamSample::kInvalidTimestamp) {
    // Guess assuming this frame was the same as the last frame.
    pts.timestamp = last_pts.timestamp + last_pts.duration;
  } else {
    // Now we really have no clue!!!  Mark an invalid timestamp and let the
    // video renderer handle it (i.e., drop frame).
    pts.timestamp = StreamSample::kInvalidTimestamp;
  }

  // Fill in the duration, using the frame itself as the authoratative source.
  base::TimeDelta duration = frame->GetDuration();
  if (duration != StreamSample::kInvalidTimestamp &&
      duration.ToInternalValue() != 0) {
    pts.duration = duration;
  } else {
    // Otherwise assume a normal frame duration.
    pts.duration = ConvertTimestamp(time_base, 1);
  }

  return pts;
}

bool FFmpegVideoDecoder::ProvidesBuffer() {
  if (!decode_engine_.get()) return false;
  return decode_engine_->ProvidesBuffer();
}

void FFmpegVideoDecoder::SetVideoDecodeEngineForTest(
    VideoDecodeEngine* engine) {
  decode_engine_.reset(engine);
}

// static
FilterFactory* FFmpegVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<FFmpegVideoDecoder, FFmpegVideoDecodeEngine*>(
      new FFmpegVideoDecodeEngine());
}

// static
bool FFmpegVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  return format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegVideo == mime_type;
}

}  // namespace media
