// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_CONTROLLER_H_

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/time.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"

// An AudioController controls an AudioOutputStream and provides data
// to this output stream. It has an important function that it executes
// audio operations like play, pause, stop, etc. on a separate thread,
// namely the audio controller thread.
//
// All the public methods of AudioController are non-blocking except close,
// the actual operations are performed on the audio controller thread.
//
// Here is a state diagram for the AudioController:
//
//                    .---->  [ Closed / Error ]  <------.
//                    |                ^                 |
//                    |                |                 |
//               [ Created ]  -->  [ Playing ]  -->  [ Paused ]
//                    ^                ^                 |
//                    |                |                 |
//              *[  Empty  ]           `-----------------'
//
// * Initial state
//
// There are two modes of buffering operations supported by this class.
//
// Regular latency mode:
//   In this mode we receive signals from AudioController and then we
//   enqueue data into it.
//
// Low latency mode:
//   In this mode a DataSource object is given to the AudioController
//   and AudioController reads from it synchronously.
//
namespace media {

class AudioController : public base::RefCountedThreadSafe<AudioController>,
                        public AudioOutputStream::AudioSourceCallback {
 public:
  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kPlaying,
    kPaused,
    kClosed,
    kError,
  };

  // An event handler that receives events from the AudioController. The
  // following methods are called on the audio controller thread.
  class EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnCreated(AudioController* controller) = 0;
    virtual void OnPlaying(AudioController* controller) = 0;
    virtual void OnPaused(AudioController* controller) = 0;
    virtual void OnError(AudioController* controller, int error_code) = 0;

    // Audio controller asks for more data.
    // |pending_bytes| is the number of bytes still on the controller.
    // |timestamp| is then time when |pending_bytes| is recorded.
    virtual void OnMoreData(AudioController* controller,
                            base::Time timestamp,
                            uint32 pending_bytes) = 0;
  };

  // A synchronous reader interface used by AudioController for synchronous
  // reading.
  class SyncReader {
   public:
    virtual ~SyncReader() {}

    // Notify the synchronous reader the number of bytes in the AudioController
    // not yet played. This is used by SyncReader to prepare more data and
    // perform synchronization.
    virtual void UpdatePendingBytes(uint32 bytes) = 0;

    // Read certain amount of data into |data|. This method returns if some
    // data is available.
    virtual uint32 Read(void* data, uint32 size) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;
  };

  virtual ~AudioController();

  // Factory method for creating an AudioController, returns NULL if failed.
  // If successful, an audio controller thread is created. The audio device
  // will be created on the audio controller thread and when that is done
  // event handler will receive a OnCreated() call.
  static scoped_refptr<AudioController> Create(
      EventHandler* event_handler,
      AudioManager::Format format,    // Format of the stream.
      int channels,                   // Number of channels.
      int sample_rate,                // Sampling frequency/rate.
      int bits_per_sample,            // Number of bits per sample.
      uint32 hardware_buffer_size,    // Size of the hardware buffer.

      // Soft limit for buffer capacity in this controller. This parameter
      // is used only in regular latency mode.
      uint32 buffer_capacity);

  // Factory method for creating a low latency audio stream.
  static scoped_refptr<AudioController> CreateLowLatency(
      EventHandler* event_handler,
      AudioManager::Format format,    // Format of the stream.
      int channels,                   // Number of channels.
      int sample_rate,                // Sampling frequency/rate.
      int bits_per_sample,            // Number of bits per sample.
      uint32 hardware_buffer_size,    // Size of the hardware buffer.

      // External synchronous reader for audio controller.
      SyncReader* sync_reader);

  // Methods to control playback of the stream.

  // Starts the playback of this audio output stream.
  void Play();

  // Pause this audio output stream.
  void Pause();

  // Discard all audio data buffered in this output stream. This method only
  // has effect when the stream is paused.
  void Flush();

  // Closes the audio output stream and shutdown the audio controller thread.
  // This method returns only after all operations are completed. This
  // controller cannot be used after this method is called.
  //
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  void Close();

  // Sets the volume of the audio output stream.
  void SetVolume(double volume);

  // Enqueue audio |data| into the controller. This method is used only in
  // the regular latency mode and it is illegal to call this method when
  // SyncReader is present.
  void EnqueueData(const uint8* data, uint32 size);

  bool LowLatencyMode() const { return sync_reader_ != NULL; }

  ///////////////////////////////////////////////////////////////////////////
  // AudioSourceCallback methods.
  virtual uint32 OnMoreData(AudioOutputStream* stream, void* dest,
                            uint32 max_size, uint32 pending_bytes);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 private:
  AudioController(EventHandler* handler,
                  uint32 capacity, SyncReader* sync_reader);

  // The following methods are executed on the audio controller thread.
  void DoCreate(AudioManager::Format format, int channels,
                int sample_rate, int bits_per_sample,
                uint32 hardware_buffer_size);
  void DoPlay();
  void DoPause();
  void DoFlush();
  void DoClose();
  void DoSetVolume(double volume);
  void DoReportError(int code);

  // Helper method to submit a OnMoreData() call to the event handler.
  void SubmitOnMoreData_Locked();

  EventHandler* handler_;
  AudioOutputStream* stream_;

  // The current volume of the audio stream.
  double volume_;

  // |state_| is written on the audio controller thread and is read on the
  // hardware audio thread. These operations need to be locked. But lock
  // is not required for reading on the audio controller thread.
  State state_;

  uint32 hardware_pending_bytes_;
  base::Time last_callback_time_;
  Lock lock_;

  // PushSource role is to buffer and it's only used in regular latency mode.
  PushSource push_source_;
  uint32 buffer_capacity_;

  // SyncReader is used only in low latency mode for synchronous reading.
  SyncReader* sync_reader_;

  // The audio controller thread that this object runs on.
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioController);
};

}  // namespace media

#endif  //  MEDIA_AUDIO_AUDIO_CONTROLLER_H_
