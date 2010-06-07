// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_controller.h"

// This constant governs the hardware audio buffer size, this value should be
// choosen carefully and is platform specific.
static const int kSamplesPerHardwarePacket = 8192;

static const uint32 kMegabytes = 1024 * 1024;

// The following parameters limit the request buffer and packet size from the
// renderer to avoid renderer from requesting too much memory.
static const uint32 kMaxDecodedPacketSize = 2 * kMegabytes;
static const uint32 kMaxBufferCapacity = 5 * kMegabytes;
static const int kMaxChannels = 32;
static const int kMaxBitsPerSample = 64;
static const int kMaxSampleRate = 192000;

// Return true if the parameters for creating an audio stream is valid.
// Return false otherwise.
static bool CheckParameters(int channels, int sample_rate,
                            int bits_per_sample) {
  if (channels <= 0 || channels > kMaxChannels)
    return false;
  if (sample_rate <= 0 || sample_rate > kMaxSampleRate)
    return false;
  if (bits_per_sample <= 0 || bits_per_sample > kMaxBitsPerSample)
    return false;
  return true;
}

namespace media {

AudioController::AudioController(EventHandler* handler, uint32 capacity,
                                 SyncReader* sync_reader)
    : handler_(handler),
      state_(kCreated),
      hardware_pending_bytes_(0),
      buffer_capacity_(capacity),
      sync_reader_(sync_reader),
      thread_("AudioControllerThread") {
}

AudioController::~AudioController() {
  DCHECK(kClosed == state_ || kCreated == state_);
}

// static
scoped_refptr<AudioController> AudioController::Create(
    EventHandler* event_handler,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    int bits_per_sample,
    uint32 buffer_capacity) {

  if (!CheckParameters(channels, sample_rate, bits_per_sample))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioController> source = new AudioController(
      event_handler, buffer_capacity, NULL);

  // Start the audio controller thread and post a task to create the
  // audio stream.
  source->thread_.Start();
  source->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(source.get(), &AudioController::DoCreate,
                        format, channels, sample_rate, bits_per_sample));
  return source;
}

// static
scoped_refptr<AudioController> AudioController::CreateLowLatency(
    EventHandler* event_handler,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    int bits_per_sample,
    SyncReader* sync_reader) {

  DCHECK(sync_reader);

  if (!CheckParameters(channels, sample_rate, bits_per_sample))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioController> source = new AudioController(
      event_handler, 0, sync_reader);

  // Start the audio controller thread and post a task to create the
  // audio stream.
  source->thread_.Start();
  source->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(source.get(), &AudioController::DoCreate,
                        format, channels, sample_rate, bits_per_sample));
  return source;
}

void AudioController::Play() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoPlay));
}

void AudioController::Pause() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoPause));
}

void AudioController::Flush() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoFlush));
}

void AudioController::Close() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoClose));
  thread_.Stop();
}

void AudioController::SetVolume(double volume) {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoSetVolume, volume));
}

void AudioController::EnqueueData(const uint8* data, uint32 size) {
  // Write data to the push source and ask for more data if needed.
  AutoLock auto_lock(lock_);
  push_source_.Write(data, size);
  SubmitOnMoreData_Locked();
}

void AudioController::DoCreate(AudioManager::Format format, int channels,
                               int sample_rate, int bits_per_sample) {
  // Create the stream in the first place.
  stream_ = AudioManager::GetAudioManager()->MakeAudioStream(
      format, channels, sample_rate, bits_per_sample);

  if (!stream_) {
    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  uint32 hardware_packet_size = kSamplesPerHardwarePacket * channels *
      bits_per_sample / 8;
  if (stream_ && !stream_->Open(hardware_packet_size)) {
    stream_->Close();
    stream_ = NULL;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }
  handler_->OnCreated(this);
}

void AudioController::DoPlay() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  State old_state;
  // Update the |state_| to kPlaying.
  {
    AutoLock auto_lock(lock_);
    old_state = state_;
    state_ = kPlaying;
  }

  // We start the AudioOutputStream lazily.
  if (old_state == kCreated) {
    stream_->Start(this);
  }

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioController::DoPause() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // We can pause from started state.
  if (state_ != kPlaying)
    return;

  // Sets the |state_| to kPaused so we don't draw more audio data.
  // TODO(hclam): Actually pause the audio device.
  {
    AutoLock auto_lock(lock_);
    state_ = kPaused;
  }

  handler_->OnPaused(this);
}

void AudioController::DoFlush() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (state_ != kPaused)
    return;

  // TODO(hclam): Actually flush the audio device.

  // If we are in the regular latency mode then flush the push source.
  if (!sync_reader_) {
    AutoLock auto_lock(lock_);
    push_source_.ClearAll();
  }
}

void AudioController::DoClose() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(kClosed, state_);

  // |stream_| can be null if creating the device failed in DoCreate().
  if (stream_) {
    stream_->Stop();
    stream_->Close();
    // After stream is closed it is destroyed, so don't keep a reference to it.
    stream_ = NULL;
  }

  // If we are in low latency mode then also close the SyncReader.
  // TODO(hclam): The shutdown procedure for low latency mode if not complete,
  // especially when OnModeData() is blocked on SyncReader for read and the
  // above Stop() would deadlock.
  if (sync_reader_)
    sync_reader_->Close();

  // Update the current state. Since the stream is closed at this point
  // there's no other threads reading |state_| so we don't need to lock.
  state_ = kClosed;
}

void AudioController::DoSetVolume(double volume) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (state_ == kError || state_ == kEmpty)
    return;

  stream_->SetVolume(volume);
}

void AudioController::DoReportError(int code) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  handler_->OnError(this, code);
}

uint32 AudioController::OnMoreData(AudioOutputStream* stream,
                                   void* dest,
                                   uint32 max_size,
                                   uint32 pending_bytes) {
  // If regular latency mode is used.
  if (!sync_reader_) {
    AutoLock auto_lock(lock_);

    // Record the callback time.
    last_callback_time_ = base::Time::Now();

    if (state_ != kPlaying) {
      // Don't read anything. Save the number of bytes in the hardware buffer.
      hardware_pending_bytes_ = pending_bytes;
      return 0;
    }

    // Push source doesn't need to know the stream and number of pending bytes.
    // So just pass in NULL and 0.
    uint32 size = push_source_.OnMoreData(NULL, dest, max_size, 0);
    hardware_pending_bytes_ = pending_bytes + size;
    SubmitOnMoreData_Locked();
    return size;
  }

  // Low latency mode.
  uint32 size =  sync_reader_->Read(dest, max_size);
  sync_reader_->UpdatePendingBytes(pending_bytes + size);
  return size;
}

void AudioController::OnClose(AudioOutputStream* stream) {
  // Push source doesn't need to know the stream so just pass in NULL.
  if (!sync_reader_) {
    AutoLock auto_lock(lock_);
    push_source_.OnClose(NULL);
  }
}

void AudioController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioController::DoReportError, code));
}

void AudioController::SubmitOnMoreData_Locked() {
  lock_.AssertAcquired();

  if (push_source_.UnProcessedBytes() > buffer_capacity_)
    return;

  base::Time timestamp = last_callback_time_;
  uint32 pending_bytes = hardware_pending_bytes_ +
      push_source_.UnProcessedBytes();

  // If we need more data then call the event handler to ask for more data.
  // It is okay that we don't lock in this block because the parameters are
  // correct and in the worst case we are just asking more data than needed.
  AutoUnlock auto_unlock(lock_);
  handler_->OnMoreData(this, timestamp, pending_bytes);
}

}  // namespace media
