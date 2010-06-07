// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioRendererHost serves audio related requests from AudioRenderer which
// lives inside the render process and provide access to audio hardware. It maps
// an internal ID to AudioRendererHost::IPCAudioSource in a map, which is the
// actual object providing audio packets through IPC. It creates the actual
// AudioOutputStream object when requested by the renderer provided with
// render view id and stream id.
//
// This class is owned by BrowserRenderProcessHost, and instantiated on UI
// thread, but all other operations and method calls (except Destroy()) happens
// in IO thread, so we need to be extra careful about the lifetime of this
// object. AudioManager is a singleton and created in IO thread, audio output
// streams are also created in the IO thread, so we need to destroy them also
// in IO thread. After this class is created, a task of OnInitialized() is
// posted on IO thread in which singleton of AudioManager is created and
// AddRef() is called to increase one ref count of this object. Owner of this
// class should call Destroy() before decrementing the ref count to this object,
// which essentially post a task of OnDestroyed() on IO thread. Inside
// OnDestroyed(), audio output streams are destroyed and Release() is called
// which may result in self-destruction.
//
// AudioRendererHost::IPCAudioSource is a container of AudioOutputStream and
// provide audio packets to the associated AudioOutputStream through IPC. It
// performs the logic for buffering and controlling the AudioOutputStream.
//
// Here is a state diagram for the IPCAudioSource:
//
//          .--------->  [ Stopped ]  <--------.
//          |                ^                 |
//          |                |                 |
//    *[ Created ]  -->  [ Playing ]  -->  [ Paused ]
//                           ^                 |
//                           |                 |
//                           `-----------------'
//
// Here's an example of a typical IPC dialog for audio:
//
//   Renderer                     AudioRendererHost
//      |                               |
//      |         CreateStream >        |
//      |          < Created            |
//      |                               |
//      |             Play >            |
//      |           < Playing           |  time
//      |                               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |             ...               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |                               |
//      |             ...               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |             ...               |
//      |           Pause >             |
//      |          < Paused             |
//      |            ...                |
//      |           Start >             |
//      |          < Started            |
//      |             ...               |
//      |            Close >            |
//      v                               v

// The above mode of operation uses relatively big buffers and has latencies
// of 50 ms or more. There is a second mode of operation which is low latency.
// For low latency audio, the picture above is modified by not having the
// RequestAudioPacket and the AudioPacketReady messages, instead a SyncSocket
// pair is used to signal buffer readiness without having to route messages
// using the IO thread.

#ifndef CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/lock.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"

class AudioManager;
struct ViewHostMsg_Audio_CreateStream_Params;

class AudioRendererHost
    : public base::RefCountedThreadSafe<
          AudioRendererHost, ChromeThread::DeleteOnIOThread> {
 private:
  class IPCAudioSource;
 public:
  // Called from UI thread from the owner of this object.
  AudioRendererHost();

  // Called from UI thread from the owner of this object to kick start
  // destruction of streams in IO thread.
  void Destroy();

  //---------------------------------------------------------------------------
  // The following public methods are called from ResourceMessageFilter in the
  // IO thread.

  // Event received when IPC channel is connected with the renderer process.
  void IPCChannelConnected(int process_id, base::ProcessHandle process_handle,
                           IPC::Message::Sender* ipc_sender);

  // Event received when IPC channel is closing.
  void IPCChannelClosing();

  // Returns true if the message is a audio related message and was processed.
  // If it was, message_was_ok will be false iff the message was corrupt.
  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

 protected:
  friend class ChromeThread;
  friend class DeleteTask<AudioRendererHost>;

  // Destruction always happens on the IO thread (see DeleteOnIOThread above).
  virtual ~AudioRendererHost();

  //---------------------------------------------------------------------------
  // Helper methods called from IPCAudioSource or from this class, since
  // methods in IPCAudioSource maybe called from hardware audio threads, these
  // methods make sure the actual tasks happen on IO thread.
  // These methods are virtual protected so we can mock these methods to test
  // IPCAudioSource.

  // A helper method to send an IPC message to renderer process on IO thread.
  virtual void Send(IPC::Message* message);

  // A helper method for sending error IPC messages.
  virtual void SendErrorMessage(int32 render_view_id, int32 stream_id);

  // A helper method for calling OnDestroySource on IO thread.
  virtual void DestroySource(IPCAudioSource* source);

 private:
  friend class AudioRendererHost::IPCAudioSource;
  friend class AudioRendererHostTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  // The container for AudioOutputStream and serves the audio packet received
  // via IPC.
  class IPCAudioSource : public AudioOutputStream::AudioSourceCallback {
   public:
    // Internal state of the source.
    enum State {
      kCreated,
      kPlaying,
      kPaused,
      kClosed,
      kError,
    };

    // Factory method for creating an IPCAudioSource, returns NULL if failed.
    // The IPCAudioSource object will have an internal state of
    // AudioOutputStream::STATE_CREATED after creation.
    // If an IPCAudioSource is created successfully, a
    // ViewMsg_NotifyAudioStreamCreated message is sent to the renderer.
    // This factory method also starts requesting audio packet from the renderer
    // after creation. The renderer will thus receive
    // ViewMsg_RequestAudioPacket message.
    static IPCAudioSource* CreateIPCAudioSource(
        AudioRendererHost* host,             // Host of this source.
        int process_id,                      // Process ID of renderer.
        int route_id,                        // Routing ID to RenderView.
        int stream_id,                       // ID of this source.
        base::ProcessHandle process_handle,  // Process handle of renderer.
        AudioManager::Format format,         // Format of the stream.
        int channels,                        // Number of channels.
        int sample_rate,                     // Sampling frequency/rate.
        char bits_per_sample,                // Number of bits per sample.
        uint32 packet_size,                  // Size of hardware packet.
        bool low_latency                     // Use low-latency (socket) code
    );
    ~IPCAudioSource();

    // Methods to control playback of the stream.
    // Starts the playback of this audio output stream. The internal state will
    // be updated to AudioOutputStream::STATE_STARTED and the state update is
    // sent to the renderer.
    void Play();

    // Pause this audio output stream. The audio output stream will stop
    // reading from the |push_source_|. The internal state will be updated
    // to AudioOutputStream::STATE_PAUSED and the state update is sent to
    // the renderer.
    void Pause();

    // Discard all audio data buffered in this output stream. This method only
    // has effect when the stream is paused.
    void Flush();

    // Closes the audio output stream. After calling this method all activities
    // of the audio output stream are stopped.
    void Close();

    // Sets the volume of the audio output stream. There's no IPC messages
    // sent back to the renderer upon success and failure.
    void SetVolume(double volume);

    // Gets the volume of the audio output stream.
    // ViewMsg_NotifyAudioStreamVolume is sent back to renderer with volume
    // information if succeeded.
    void GetVolume();

    // Notify this source that buffer has been filled and is ready to be
    // consumed.
    void NotifyPacketReady(uint32 packet_size);

    // AudioSourceCallback methods.
    virtual uint32 OnMoreData(AudioOutputStream* stream, void* dest,
                              uint32 max_size, uint32 pending_bytes);
    virtual void OnClose(AudioOutputStream* stream);
    virtual void OnError(AudioOutputStream* stream, int code);

    int process_id() { return process_id_; }
    int route_id() { return route_id_; }
    int stream_id() { return stream_id_; }

   private:
    IPCAudioSource(AudioRendererHost* host,     // Host of this source.
                   int process_id,              // Process ID of renderer.
                   int route_id,                // Routing ID to RenderView.
                   int stream_id,               // ID of this source.
                   AudioOutputStream* stream,   // Stream associated.
                   uint32 hardware_packet_size,
                   uint32 buffer_capacity);     // Capacity of transportation
                                                // buffer.

    // Check the condition of |outstanding_request_| and |push_source_| to
    // determine if we should submit a new packet request.
    void SubmitPacketRequest_Locked();

    void SubmitPacketRequest(AutoLock* alock);

    // A helper method to start buffering. This method is used by
    // CreateIPCAudioSource to submit a packet request.
    void StartBuffering();

    AudioRendererHost* host_;
    int process_id_;
    int route_id_;
    int stream_id_;
    AudioOutputStream* stream_;
    uint32 hardware_packet_size_;
    uint32 buffer_capacity_;

    State state_;

    base::SharedMemory shared_memory_;
    scoped_ptr<base::SyncSocket> shared_socket_;

    // PushSource role is to buffer and it's only used in regular latency mode.
    PushSource push_source_;

    // Flag that indicates there is an outstanding request.
    bool outstanding_request_;

    // Number of bytes copied in the last OnMoreData call.
    uint32 pending_bytes_;
    base::Time last_callback_time_;

    // Protects:
    // - |outstanding_requests_|
    // - |last_copied_bytes_|
    // - |push_source_|
    Lock lock_;
  };

  //---------------------------------------------------------------------------
  // Methods called on IO thread.
  // Returns true if the message is an audio related message and should be
  // handled by this class.
  bool IsAudioRendererHostMessage(const IPC::Message& message);

  // Audio related IPC message handlers.
  // Creates an audio output stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties. See IPCAudioSource::CreateIPCAudioSource() for more
  // details.
  void OnCreateStream(const IPC::Message& msg, int stream_id,
                      const ViewHostMsg_Audio_CreateStream_Params& params,
                      bool low_latency);

  // Starts buffering for the audio output stream. Delegates the start method
  // call to the corresponding IPCAudioSource::Play().
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnPlayStream(const IPC::Message& msg, int stream_id);

  // Pauses the audio output stream. Delegates the pause method call to the
  // corresponding IPCAudioSource::Pause(),
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnPauseStream(const IPC::Message& msg, int stream_id);

  // Discard all audio data buffered.
  void OnFlushStream(const IPC::Message& msg, int stream_id);

  // Closes the audio output stream, delegates the close method call to the
  // corresponding IPCAudioSource::Close(), no returning IPC message to renderer
  // upon success and failure.
  void OnCloseStream(const IPC::Message& msg, int stream_id);

  // Set the volume for the stream specified. Delegates the SetVolume() method
  // call to IPCAudioSource. No returning IPC message to renderer upon success.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnSetVolume(const IPC::Message& msg, int stream_id, double volume);

  // Gets the volume of the stream specified, delegates to corresponding
  // IPCAudioSource::GetVolume(), see the method for more details.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnGetVolume(const IPC::Message& msg, int stream_id);

  // Notify packet has been prepared for stream, delegates to corresponding
  // IPCAudioSource::NotifyPacketReady(), see the method for more details.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnNotifyPacketReady(const IPC::Message& msg, int stream_id,
                           uint32 packet_size);

  // Called on IO thread when this object needs to be destroyed and after
  // Destroy() is called from owner of this class in UI thread.
  void OnDestroyed();

  // Sends IPC messages using ipc_sender_.
  void OnSend(IPC::Message* message);

  // Closes the source, deletes it and removes it from the internal map.
  // Destruction of source and associated stream should always be done by this
  // method. *DO NOT* call this method from other than IPCAudioSource and from
  // this class.
  void OnDestroySource(IPCAudioSource* source);

  // A helper method that destroy all IPCAudioSource and associated audio
  // output streams.
  void DestroyAllSources();

  // A helper method to look up a IPCAudioSource with a tuple of render view id
  // and stream id. Returns NULL if not found.
  IPCAudioSource* Lookup(int render_view_id, int stream_id);

  int process_id_;
  base::ProcessHandle process_handle_;
  IPC::Message::Sender* ipc_sender_;

  // A map of id to audio sources.
  typedef std::pair<int32, int32> SourceID;
  typedef std::map<SourceID, IPCAudioSource*> SourceMap;
  SourceMap sources_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
