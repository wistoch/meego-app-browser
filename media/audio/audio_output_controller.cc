// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include "base/message_loop.h"

// The following parameters limit the request buffer and packet size from the
// renderer to avoid renderer from requesting too much memory.
static const uint32 kMegabytes = 1024 * 1024;
static const uint32 kMaxHardwareBufferSize = 2 * kMegabytes;
// Signal a pause in low-latency mode.
static const int kPauseMark = -1;

namespace {
// Return true if the parameters for creating an audio stream is valid.
// Return false otherwise.
static bool CheckParameters(AudioParameters params,
                            uint32 hardware_buffer_size) {
  if (!params.IsValid())
    return false;
  if (hardware_buffer_size <= 0 ||
      hardware_buffer_size > kMaxHardwareBufferSize)
    return false;
  return true;
}

}  // namespace

namespace media {

AudioOutputController::AudioOutputController(EventHandler* handler,
                                             uint32 capacity,
                                             SyncReader* sync_reader)
    : handler_(handler),
      stream_(NULL),
      volume_(1.0),
      state_(kEmpty),
      hardware_pending_bytes_(0),
      buffer_capacity_(capacity),
      sync_reader_(sync_reader) {
}

AudioOutputController::~AudioOutputController() {
  DCHECK(kClosed == state_);
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    EventHandler* event_handler,
    AudioParameters params,
    uint32 hardware_buffer_size,
    uint32 buffer_capacity) {

  if (!CheckParameters(params, hardware_buffer_size))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller = new AudioOutputController(
      event_handler, buffer_capacity, NULL);

  controller->message_loop_ =
      AudioManager::GetAudioManager()->GetMessageLoop();
  controller->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        params, hardware_buffer_size));
  return controller;
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::CreateLowLatency(
    EventHandler* event_handler,
    AudioParameters params,
    uint32 hardware_buffer_size,
    SyncReader* sync_reader) {

  DCHECK(sync_reader);

  if (!CheckParameters(params, hardware_buffer_size))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller = new AudioOutputController(
      event_handler, 0, sync_reader);

  controller->message_loop_ =
      AudioManager::GetAudioManager()->GetMessageLoop();
  controller->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        params, hardware_buffer_size));
  return controller;
}

void AudioOutputController::Play() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPlay));
}

void AudioOutputController::Pause() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPause));
}

void AudioOutputController::Flush() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoFlush));
}

void AudioOutputController::Close(Task* closed_task) {
  DCHECK(closed_task);
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoClose, closed_task));
}

void AudioOutputController::SetVolume(double volume) {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoSetVolume, volume));
}

void AudioOutputController::EnqueueData(const uint8* data, uint32 size) {
  // Write data to the push source and ask for more data if needed.
  AutoLock auto_lock(lock_);
  push_source_.Write(data, size);
  SubmitOnMoreData_Locked();
}

void AudioOutputController::DoCreate(AudioParameters params,
                                     uint32 hardware_buffer_size) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Close() can be called before DoCreate() is executed.
  if (state_ == kClosed)
    return;
  DCHECK(state_ == kEmpty);

  stream_ = AudioManager::GetAudioManager()->MakeAudioOutputStream(params);
  if (!stream_) {
    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (!stream_->Open(hardware_buffer_size)) {
    stream_->Close();
    stream_ = NULL;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }
  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  // And then report we have been created.
  handler_->OnCreated(this);

  // If in normal latency mode then start buffering.
  if (!LowLatencyMode()) {
    AutoLock auto_lock(lock_);
    SubmitOnMoreData_Locked();
  }
}

void AudioOutputController::DoPlay() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;
  state_ = kPlaying;

  // We start the AudioOutputStream lazily.
  stream_->Start(this);

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioOutputController::DoPause() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // We can pause from started state.
  if (state_ != kPlaying)
    return;
  state_ = kPaused;

  // Then we stop the audio device. This is not the perfect solution because
  // it discards all the internal buffer in the audio device.
  // TODO(hclam): Actually pause the audio device.
  stream_->Stop();

  if (LowLatencyMode()) {
    // Send a special pause mark to the low-latency audio thread.
    sync_reader_->UpdatePendingBytes(kPauseMark);
  }

  handler_->OnPaused(this);
}

void AudioOutputController::DoFlush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // TODO(hclam): Actually flush the audio device.

  // If we are in the regular latency mode then flush the push source.
  if (!sync_reader_) {
    if (state_ != kPaused)
      return;
    push_source_.ClearAll();
  }
}

void AudioOutputController::DoClose(Task* closed_task) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (state_ != kClosed) {
    // |stream_| can be null if creating the device failed in DoCreate().
    if (stream_) {
      stream_->Stop();
      stream_->Close();
      // After stream is closed it is destroyed, so don't keep a reference to
      // it.
      stream_ = NULL;
    }

    state_ = kClosed;
  }

  closed_task->Run();
  delete closed_task;
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  if (state_ != kPlaying && state_ != kPaused && state_ != kCreated)
    return;

  stream_->SetVolume(volume_);
}

void AudioOutputController::DoReportError(int code) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (state_ != kClosed)
    handler_->OnError(this, code);
}

uint32 AudioOutputController::OnMoreData(AudioOutputStream* stream,
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

    // Push source doesn't need to know the stream and number of pending
    // bytes. So just pass in NULL and 0.
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

void AudioOutputController::OnClose(AudioOutputStream* stream) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Push source doesn't need to know the stream so just pass in NULL.
  if (LowLatencyMode()) {
    sync_reader_->Close();
  } else {
    AutoLock auto_lock(lock_);
    push_source_.OnClose(NULL);
  }
}

void AudioOutputController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoReportError, code));
}

void AudioOutputController::SubmitOnMoreData_Locked() {
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
