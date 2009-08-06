// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_base.h"

#include <algorithm>

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_algorithm_ola.h"

namespace media {

AudioRendererBase::AudioRendererBase()
    : state_(kUninitialized),
      pending_reads_(0) {
}

AudioRendererBase::~AudioRendererBase() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererBase::Play(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void AudioRendererBase::Pause(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPlaying, state_);
  pause_callback_.reset(callback);
  state_ = kPaused;

  // We'll only pause when we've finished all pending reads.
  if (pending_reads_ == 0) {
    pause_callback_->Run();
    pause_callback_.reset();
  } else {
    state_ = kPaused;
  }
}

void AudioRendererBase::Stop() {
  OnStop();
  AutoLock auto_lock(lock_);
  state_ = kStopped;
  algorithm_.reset(NULL);
}

void AudioRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  state_ = kSeeking;
  seek_callback_.reset(callback);

  // Throw away everything and schedule our reads.
  last_fill_buffer_time_ = base::TimeDelta();

  // |algorithm_| will request more reads.
  algorithm_->FlushBuffers();
}

void AudioRendererBase::Initialize(AudioDecoder* decoder,
                                   FilterCallback* callback) {
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK_EQ(kUninitialized, state_);
  scoped_ptr<FilterCallback> c(callback);
  decoder_ = decoder;

  // Defer initialization until all scheduled reads have completed.
  if (!OnInitialize(decoder_->media_format())) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

  // Get the media properties to initialize our algorithms.
  int channels = 0;
  int sample_rate = 0;
  int sample_bits = 0;
  bool ret = ParseMediaFormat(decoder_->media_format(),
                              &channels,
                              &sample_rate,
                              &sample_bits);

  // We should have successfully parsed the media format, or we would not have
  // been created.
  DCHECK(ret);

  // Create a callback so our algorithm can request more reads.
  AudioRendererAlgorithmBase::RequestReadCallback* cb =
      NewCallback(this, &AudioRendererBase::ScheduleRead_Locked);

  // Construct the algorithm.
  algorithm_.reset(new AudioRendererAlgorithmOLA());

  // Initialize our algorithm with media properties, initial playback rate
  // (may be 0), and a callback to request more reads from the data source.
  algorithm_->Initialize(channels,
                         sample_rate,
                         sample_bits,
                         GetPlaybackRate(),
                         cb);

  // Finally, execute the start callback.
  state_ = kPaused;
  callback->Run();
}

void AudioRendererBase::OnReadComplete(Buffer* buffer_in) {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying);
  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  // Don't enqueue an end-of-stream buffer because it has no data.
  if (!buffer_in->IsEndOfStream()) {
    // Note: Calling this may schedule more reads.
    algorithm_->EnqueueBuffer(buffer_in);
  }

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(seek_callback_.get());
    if (algorithm_->IsQueueFull() || buffer_in->IsEndOfStream()) {
      // Transition into paused whether we have data in |algorithm_| or not.
      // FillBuffer() will play silence if there's nothing to fill.
      state_ = kPaused;
      seek_callback_->Run();
      seek_callback_.reset();
    }
  } else if (state_ == kPaused && pending_reads_ == 0) {
    // No more pending reads!  We're now officially "paused".
    if (pause_callback_.get()) {
      pause_callback_->Run();
      pause_callback_.reset();
    }
  }
}

size_t AudioRendererBase::FillBuffer(uint8* dest,
                                     size_t dest_len,
                                     const base::TimeDelta& playback_delay) {
  // The timestamp of the last buffer written during the last call to
  // FillBuffer().
  base::TimeDelta last_fill_buffer_time;
  size_t dest_written = 0;
  {
    AutoLock auto_lock(lock_);

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      // TODO(scherkus): To keep the audio hardware busy we write at most 8k of
      // zeros.  This gets around the tricky situation of pausing and resuming
      // the audio IPC layer in Chrome.  Ideally, we should return zero and then
      // the subclass can restart the conversation.
      const size_t kZeroLength = 8192;
      dest_written = std::min(kZeroLength, dest_len);
      memset(dest, 0, dest_written);
      return dest_written;
    }

    // Save a local copy of last fill buffer time and reset the member.
    last_fill_buffer_time = last_fill_buffer_time_;
    last_fill_buffer_time_ = base::TimeDelta();

    // Do the fill.
    dest_written = algorithm_->FillBuffer(dest, dest_len);

    // Get the current time.
    last_fill_buffer_time_ = algorithm_->GetTime();
  }

  // Update the pipeline's time if it was set last time.
  if (last_fill_buffer_time.InMicroseconds() > 0 &&
      last_fill_buffer_time != last_fill_buffer_time_) {
    // Adjust the |last_fill_buffer_time| with the playback delay.
    // TODO(hclam): If there is a playback delay, the pipeline would not be
    // updated with a correct timestamp when the stream is played at the very
    // end since we use decoded packets to trigger time updates. A better
    // solution is to start a timer when an audio packet is decoded to allow
    // finer time update events.
    if (playback_delay < last_fill_buffer_time)
      last_fill_buffer_time -= playback_delay;
    host()->SetTime(last_fill_buffer_time);
  }

  return dest_written;
}

void AudioRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  ++pending_reads_;
  decoder_->Read(NewCallback(this, &AudioRendererBase::OnReadComplete));
}

// static
bool AudioRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* channels_out,
                                         int* sample_rate_out,
                                         int* sample_bits_out) {
  // TODO(scherkus): might be handy to support NULL parameters.
  std::string mime_type;
  return media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      media_format.GetAsInteger(MediaFormat::kChannels, channels_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleRate, sample_rate_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleBits, sample_bits_out) &&
      mime_type.compare(mime_type::kUncompressedAudio) == 0;
}

void AudioRendererBase::SetPlaybackRate(float playback_rate) {
  algorithm_->set_playback_rate(playback_rate);
}

float AudioRendererBase::GetPlaybackRate() {
  return algorithm_->playback_rate();
}

}  // namespace media
