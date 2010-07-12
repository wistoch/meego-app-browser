// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//    AudioRendererHost                AudioRendererImpl
//           ^                                ^
//           |                                |
//           v                 IPC            v
//   ResourceMessageFilter <---------> AudioMessageFilter
//
// Implementation of interface with audio device is in AudioRendererHost and
// it provides services and entry points in ResourceMessageFilter, allowing
// usage of IPC calls to interact with audio device. AudioMessageFilter acts
// as a portal for IPC calls and does no more than delegation.
//
// Transportation of audio buffer is done by using shared memory, after
// OnCreateStream is executed, OnCreated would be called along with a
// SharedMemoryHandle upon successful creation of audio output stream in the
// browser process. The same piece of shared memory would be used during the
// lifetime of this unit.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. IO thread.
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 2. Pipeline thread
//    Initialization of filter and proper stopping of filters happens here.
//    Properties of this filter is also set in this thread.
// 3. Audio decoder thread (If there's one.)
//    Responsible for decoding audio data and gives raw PCM data to this object.
//
// Methods categorized according to the thread(s) they are running on.
//
// Render thread
// +-- CreateFactory()
// |     Helper method for construction this class.
// \-- IsMediaFormatSupported()
//       Helper method to identify media formats accepted by this class for
//       construction.
//
// IO thread (Main thread in render process)
// +-- OnCreateStream()
// |     Sends an IPC message to browser to create audio output stream and
// |     register this object with AudioMessageFilter.
// |-- OnSetVolume()
// |     Sends an IPC message to browser to set volume.
// |-- OnNotifyPacketReady
// |     Try to fill the shared memory with decoded audio packet and sends IPC
// |     messages to browser if packet is ready.
// |-- OnRequestPacket()
// |     Called from AudioMessageFilter when an audio packet requested is
// |     received from browser process.
// |-- OnStateChanged()
// |     Called from AudioMessageFilter upon state change of the audio output
// |     stream in the browser process. Error of the stream is reported here.
// |-- OnCreated()
// |     Called from AudioMessageFilter upon successful creation of audio output
// |     stream in the browser process, called along with a SharedMemoryHandle.
// |-- OnVolume()
// |     Called from AudioMessageFilter about the volume of the audio output
// |     stream.
// \-- OnDestroy()
//       Release resources that live inside io thread.
//
// Pipeline thread
// +-- AudioRendererImpl()
// |     Constructor method.
// |-- ~AudioRendererImpl()
// |     Destructor method.
// |-- SetPlaybackRate()
// |     Given the playback rate information.
// |-- GetMediaFormat()
// |     Obtain the current media format of this unit.
// |-- SetVolume()
// |     Given the volume information.
// |-- OnInitialize()
// |     Called from AudioRendererBase for initialization event.
// \-- OnStop()
//       Called from AudioRendererBase for stop event.
//
// Audio decoder thread (If there's one.)
// \-- OnReadComplete()
//       A raw PCM audio packet buffer is received here, this method is called
//       from pipeline thread if audio decoder thread does not exist.

#ifndef CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_

#include "base/scoped_ptr.h"
#include "base/lock.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/renderer/audio_message_filter.h"
#include "media/audio/audio_io.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class AudioMessageFilter;

class AudioRendererImpl : public media::AudioRendererBase,
                          public AudioMessageFilter::Delegate,
                          public MessageLoop::DestructionObserver {
 public:
  // Methods called on render thread ------------------------------------------
  // Methods called during construction.
  static media::FilterFactory* CreateFactory(AudioMessageFilter* filter) {
    return new media::FilterFactoryImpl1<AudioRendererImpl,
                                         AudioMessageFilter*>(filter);
  }
  static bool IsMediaFormatSupported(const media::MediaFormat& format);

  // Methods called on IO thread ----------------------------------------------
  // AudioMessageFilter::Delegate methods, called by AudioMessageFilter.
  void OnRequestPacket(uint32 bytes_in_buffer,
                       const base::Time& message_timestamp);
  void OnStateChanged(const ViewMsg_AudioStreamState_Params& state);
  void OnCreated(base::SharedMemoryHandle handle, uint32 length);
  void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                           base::SyncSocket::Handle socket_handle,
                           uint32 length);
  void OnVolume(double volume);

  // Methods called on pipeline thread ----------------------------------------
  // media::MediaFilter implementation.
  virtual void SetPlaybackRate(float rate);
  virtual void Seek(base::TimeDelta time, media::FilterCallback* callback);

  // media::AudioRenderer implementation.
  virtual void SetVolume(float volume);

 protected:
  // Methods called on audio renderer thread ----------------------------------
  // These methods are called from AudioRendererBase.
  virtual bool OnInitialize(const media::MediaFormat& media_format);
  virtual void OnStop();

  // Called when the decoder completes a Read().
  virtual void OnReadComplete(media::Buffer* buffer_in);

 private:
  friend class media::FilterFactoryImpl1<AudioRendererImpl,
                                         AudioMessageFilter*>;

  // For access to constructor and IO thread methods.
  friend class AudioRendererImplTest;
  FRIEND_TEST(AudioRendererImplTest, Stop);
  FRIEND_TEST(AudioRendererImplTest, DestroyedMessageLoop_OnReadComplete);

  explicit AudioRendererImpl(AudioMessageFilter* filter);
  virtual ~AudioRendererImpl();

  // Helper methods.
  // Convert number of bytes to duration of time using information about the
  // number of channels, sample rate and sample bits.
  base::TimeDelta ConvertToDuration(int bytes);

  // Methods call on IO thread ------------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void CreateStreamTask(AudioManager::Format format, int channels,
                        int sample_rate, int bits_per_sample);
  void PlayTask();
  void PauseTask();
  void SeekTask();
  void SetVolumeTask(double volume);
  void NotifyPacketReadyTask();
  void DestroyTask();

  // Called on IO thread when message loop is dying.
  virtual void WillDestroyCurrentMessageLoop();

  // Information about the audio stream.
  int channels_;
  int sample_rate_;
  int sample_bits_;
  uint32 bytes_per_second_;

  scoped_refptr<AudioMessageFilter> filter_;

  // ID of the stream created in the browser process.
  int32 stream_id_;

  // Memory shared by the browser process for audio buffer.
  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32 shared_memory_size_;

  // Message loop for the IO thread.
  MessageLoop* io_loop_;

  // Protects:
  // - |stopped_|
  // - |pending_request_|
  // - |request_timestamp_|
  // - |request_delay_|
  Lock lock_;

  // A flag that indicates this filter is called to stop.
  bool stopped_;

  // A flag that indicates an outstanding packet request.
  bool pending_request_;

  // The time when a request is made.
  base::Time request_timestamp_;

  // The delay for the requested packet to be played.
  base::TimeDelta request_delay_;

  // State variables for prerolling.
  bool prerolling_;

  // Remaining bytes for prerolling to complete.
  uint32 preroll_bytes_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
