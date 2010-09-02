// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decode_engine.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_allocator.h"

namespace media {

FFmpegVideoDecodeEngine::FFmpegVideoDecodeEngine()
    : codec_context_(NULL),
      av_stream_(NULL),
      event_handler_(NULL),
      direct_rendering_(false),
      pending_input_buffers_(0),
      pending_output_buffers_(0),
      output_eos_reached_(false),
      flush_pending_(false) {
}

FFmpegVideoDecodeEngine::~FFmpegVideoDecodeEngine() {
}

void FFmpegVideoDecodeEngine::Initialize(
    MessageLoop* message_loop,
    VideoDecodeEngine::EventHandler* event_handler,
    const VideoCodecConfig& config) {
  allocator_.reset(new FFmpegVideoAllocator());

  // Always try to use three threads for video decoding.  There is little reason
  // not to since current day CPUs tend to be multi-core and we measured
  // performance benefits on older machines such as P4s with hyperthreading.
  //
  // Handling decoding on separate threads also frees up the pipeline thread to
  // continue processing. Although it'd be nice to have the option of a single
  // decoding thread, FFmpeg treats having one thread the same as having zero
  // threads (i.e., avcodec_decode_video() will execute on the calling thread).
  // Yet another reason for having two threads :)
  static const int kDecodeThreads = 2;
  static const int kMaxDecodeThreads = 16;

  av_stream_ = static_cast<AVStream*>(config.opaque_context_);
  codec_context_ = av_stream_->codec;
  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);

  if (codec) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
    direct_rendering_ = codec->capabilities & CODEC_CAP_DR1 ? true : false;
#endif
    if (direct_rendering_) {
      DLOG(INFO) << "direct rendering is used";
      allocator_->Initialize(codec_context_, GetSurfaceFormat());
    }
  }

  // TODO(fbarchard): Improve thread logic based on size / codec.
  // TODO(fbarchard): Fix bug affecting video-cookie.html
  int decode_threads = (codec_context_->codec_id == CODEC_ID_THEORA) ?
    1 : kDecodeThreads;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if ((!threads.empty() &&
      !base::StringToInt(threads, &decode_threads)) ||
      decode_threads < 0 || decode_threads > kMaxDecodeThreads) {
    decode_threads = kDecodeThreads;
  }

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  av_frame_.reset(avcodec_alloc_frame());
  VideoCodecInfo info;
  info.success_ = false;
  info.provides_buffers_ = true;
  info.stream_info_.surface_type_ = VideoFrame::TYPE_SYSTEM_MEMORY;
  info.stream_info_.surface_format_ = GetSurfaceFormat();
  info.stream_info_.surface_width_ = config.width_;
  info.stream_info_.surface_height_ = config.height_;

  // If we do not have enough buffers, we will report error too.
  bool buffer_allocated = true;
  frame_queue_available_.clear();
  if (!direct_rendering_) {
    // Create output buffer pool when direct rendering is not used.
    for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
      scoped_refptr<VideoFrame> video_frame;
      VideoFrame::CreateFrame(VideoFrame::YV12,
                              config.width_,
                              config.height_,
                              StreamSample::kInvalidTimestamp,
                              StreamSample::kInvalidTimestamp,
                              &video_frame);
      if (!video_frame.get()) {
        buffer_allocated = false;
        break;
      }
      frame_queue_available_.push_back(video_frame);
    }
  }

  if (codec &&
      avcodec_thread_init(codec_context_, decode_threads) >= 0 &&
      avcodec_open(codec_context_, codec) >= 0 &&
      av_frame_.get() &&
      buffer_allocated) {
    info.success_ = true;
  }
  event_handler_ = event_handler;
  event_handler_->OnInitializeComplete(info);
}

// TODO(fbarchard): Find way to remove this memcpy of the entire image.
static void CopyPlane(size_t plane,
                      scoped_refptr<VideoFrame> video_frame,
                      const AVFrame* frame) {
  DCHECK_EQ(video_frame->width() % 2, 0u);
  const uint8* source = frame->data[plane];
  const size_t source_stride = frame->linesize[plane];
  uint8* dest = video_frame->data(plane);
  const size_t dest_stride = video_frame->stride(plane);
  size_t bytes_per_line = video_frame->width();
  size_t copy_lines = video_frame->height();
  if (plane != VideoFrame::kYPlane) {
    bytes_per_line /= 2;
    if (video_frame->format() == VideoFrame::YV12) {
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

void FFmpegVideoDecodeEngine::EmptyThisBuffer(
    scoped_refptr<Buffer> buffer) {
  pending_input_buffers_--;
  if (flush_pending_) {
    TryToFinishPendingFlush();
  } else {
    // Otherwise try to decode this buffer.
    DecodeFrame(buffer);
  }
}

void FFmpegVideoDecodeEngine::FillThisBuffer(scoped_refptr<VideoFrame> frame) {
  // We should never receive NULL frame or EOS frame.
  DCHECK(frame.get() && !frame->IsEndOfStream());

  // Increment pending output buffer count.
  pending_output_buffers_++;

  // Return this frame to available pool or allocator after display.
  if (direct_rendering_)
    allocator_->DisplayDone(codec_context_, frame);
  else
    frame_queue_available_.push_back(frame);

  if (flush_pending_) {
    TryToFinishPendingFlush();
  } else if (!output_eos_reached_) {
    // If we already deliver EOS to renderer, we stop reading new input.
    ReadInput();
  }
}

// Try to decode frame when both input and output are ready.
void FFmpegVideoDecodeEngine::DecodeFrame(scoped_refptr<Buffer> buffer) {
  scoped_refptr<VideoFrame> video_frame;

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  // Let FFmpeg handle presentation timestamp reordering.
  codec_context_->reordered_opaque = buffer->GetTimestamp().InMicroseconds();

  // This is for codecs not using get_buffer to initialize
  // |av_frame_->reordered_opaque|
  av_frame_->reordered_opaque = codec_context_->reordered_opaque;

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_,
                                     av_frame_.get(),
                                     &frame_decoded,
                                     &packet);

  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(INFO) << "Error decoding a video frame with timestamp: "
              << buffer->GetTimestamp().InMicroseconds() << " us"
              << " , duration: "
              << buffer->GetDuration().InMicroseconds() << " us"
              << " , packet size: "
              << buffer->GetDataSize() << " bytes";
    // TODO(jiesun): call event_handler_->OnError() instead.
    event_handler_->OnFillBufferCallback(video_frame);
    return;
  }

  // If frame_decoded == 0, then no frame was produced.
  // In this case, if we already begin to flush codec with empty
  // input packet at the end of input stream, the first time we
  // encounter frame_decoded == 0 signal output frame had been
  // drained, we mark the flag. Otherwise we read from demuxer again.
  if (frame_decoded == 0) {
    if (buffer->IsEndOfStream()) {  // We had started flushing.
      event_handler_->OnFillBufferCallback(video_frame);
      output_eos_reached_ = true;
    } else {
      ReadInput();
    }
    return;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    // TODO(jiesun): call event_handler_->OnError() instead.
    event_handler_->OnFillBufferCallback(video_frame);
    return;
  }

  // Determine timestamp and calculate the duration based on the repeat picture
  // count.  According to FFmpeg docs, the total duration can be calculated as
  // follows:
  //   duration = (1 / fps) + (repeat_pict) / (2 * fps)
  //            = (2 + repeat_pict) / (2 * fps)
  DCHECK_LE(av_frame_->repeat_pict, 2);  // Sanity check.
  // Even frame rate is fixed, for some streams and codecs, the value of
  // |codec_context_->time_base| and |av_stream_->time_base| are not the
  // inverse of the |av_stream_->r_frame_rate|. They use 1 milli-second as
  // time-base units and use increment of av_packet->pts which is not one.
  // Use the inverse of |av_stream_->r_frame_rate| instead of time_base.
  AVRational doubled_time_base;
  doubled_time_base.den = av_stream_->r_frame_rate.num;
  doubled_time_base.num = av_stream_->r_frame_rate.den;
  doubled_time_base.den *= 2;

  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque);
  base::TimeDelta duration =
      ConvertTimestamp(doubled_time_base, 2 + av_frame_->repeat_pict);

  if (!direct_rendering_) {
    // Available frame is guaranteed, because we issue as much reads as
    // available frame, except the case of |frame_decoded| == 0, which
    // implies decoder order delay, and force us to read more inputs.
    DCHECK(frame_queue_available_.size());
    video_frame = frame_queue_available_.front();
    frame_queue_available_.pop_front();

    // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
    // output, meaning the data is only valid until the next
    // avcodec_decode_video() call.
    CopyPlane(VideoFrame::kYPlane, video_frame.get(), av_frame_.get());
    CopyPlane(VideoFrame::kUPlane, video_frame.get(), av_frame_.get());
    CopyPlane(VideoFrame::kVPlane, video_frame.get(), av_frame_.get());
  } else {
    // Get the VideoFrame from allocator which associate with av_frame_.
    video_frame = allocator_->DecodeDone(codec_context_, av_frame_.get());
  }

  video_frame->SetTimestamp(timestamp);
  video_frame->SetDuration(duration);

  pending_output_buffers_--;
  event_handler_->OnFillBufferCallback(video_frame);
}

void FFmpegVideoDecodeEngine::Uninitialize() {
  if (direct_rendering_) {
    allocator_->Stop(codec_context_);
  }

  event_handler_->OnUninitializeComplete();
}

void FFmpegVideoDecodeEngine::Flush() {
  avcodec_flush_buffers(codec_context_);
  flush_pending_ = true;
  TryToFinishPendingFlush();
}

void FFmpegVideoDecodeEngine::TryToFinishPendingFlush() {
  DCHECK(flush_pending_);

  // We consider ourself flushed when there is no pending input buffers
  // and output buffers, which implies that all buffers had been returned
  // to its owner.
  if (!pending_input_buffers_ && !pending_output_buffers_) {
    // Try to finish flushing and notify pipeline.
    flush_pending_ = false;
    event_handler_->OnFlushComplete();
  }
}

void FFmpegVideoDecodeEngine::Seek() {
  // After a seek, output stream no longer considered as EOS.
  output_eos_reached_ = false;

  // The buffer provider is assumed to perform pre-roll operation.
  for (unsigned int i = 0; i < Limits::kMaxVideoFrames; ++i)
    ReadInput();

  event_handler_->OnSeekComplete();
}

void FFmpegVideoDecodeEngine::ReadInput() {
  DCHECK_EQ(output_eos_reached_, false);
  pending_input_buffers_++;
  event_handler_->OnEmptyBufferCallback(NULL);
}

VideoFrame::Format FFmpegVideoDecodeEngine::GetSurfaceFormat() const {
  // J (Motion JPEG) versions of YUV are full range 0..255.
  // Regular (MPEG) YUV is 16..240.
  // For now we will ignore the distinction and treat them the same.
  switch (codec_context_->pix_fmt) {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUVJ420P:
      return VideoFrame::YV12;
      break;
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUVJ422P:
      return VideoFrame::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      return VideoFrame::INVALID;
  }
}

}  // namespace media
