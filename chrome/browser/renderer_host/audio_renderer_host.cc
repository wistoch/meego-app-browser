// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(hclam): Several changes need to be made to this code:
// 1. We should host AudioRendererHost on a dedicated audio thread. Doing
//    so we don't have to worry about blocking method calls such as
//    play / stop an audio stream.
// 2. Move locked data structures into a separate structure that sanity
//    checks access by different threads that use it.
//
// SEMANTICS OF |state_|
// Note that |state_| of IPCAudioSource is accessed on two thread. Namely
// the IO thread and the audio thread. IO thread is the thread on which
// IPAudioSource::Play(), IPCAudioSource::Pause() are called. Audio thread
// is a thread operated by the audio hardware for requesting data.
// It is important that |state_| is only written on the IO thread because
// reading of such state in Play() and Pause() is not protected. However,
// since OnMoreData() is called on the audio thread and reads |state_|
// variable. Writing to this variable needs to be protected in Play()
// and Pause().

#include "chrome/browser/renderer_host/audio_renderer_host.h"

#include "base/histogram.h"
#include "base/lock.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_logging.h"

namespace {

// This constant governs the hardware audio buffer size, this value should be
// choosen carefully and is platform specific.
const int kSamplesPerHardwarePacket = 8192;

// If the size of the buffer is less than this number, then the low latency
// mode is to be used.
const uint32 kLowLatencyPacketThreshold = 1025;

const uint32 kMegabytes = 1024 * 1024;

// The following parameters limit the request buffer and packet size from the
// renderer to avoid renderer from requesting too much memory.
const uint32 kMaxDecodedPacketSize = 2 * kMegabytes;
const uint32 kMaxBufferCapacity = 5 * kMegabytes;
static const int kMaxChannels = 32;
static const int kMaxBitsPerSample = 64;
static const int kMaxSampleRate = 192000;

}  // namespace

//-----------------------------------------------------------------------------
// AudioRendererHost::IPCAudioSource implementations.

AudioRendererHost::IPCAudioSource::IPCAudioSource(
    AudioRendererHost* host,
    int process_id,
    int route_id,
    int stream_id,
    AudioOutputStream* stream,
    uint32 hardware_packet_size,
    uint32 decoded_packet_size,
    uint32 buffer_capacity)
    : host_(host),
      process_id_(process_id),
      route_id_(route_id),
      stream_id_(stream_id),
      stream_(stream),
      hardware_packet_size_(hardware_packet_size),
      decoded_packet_size_(decoded_packet_size),
      buffer_capacity_(buffer_capacity),
      state_(kCreated),
      outstanding_request_(false),
      pending_bytes_(0) {
}

AudioRendererHost::IPCAudioSource::~IPCAudioSource() {
  DCHECK(kClosed == state_ || kCreated == state_);
}

// static
AudioRendererHost::IPCAudioSource*
AudioRendererHost::IPCAudioSource::CreateIPCAudioSource(
    AudioRendererHost* host,
    int process_id,
    int route_id,
    int stream_id,
    base::ProcessHandle process_handle,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    char bits_per_sample,
    uint32 decoded_packet_size,
    uint32 buffer_capacity,
    bool low_latency) {
  // Perform come preliminary checks on the parameters.
  // Make sure the renderer didn't ask for too much memory.
  if (buffer_capacity > kMaxBufferCapacity ||
      decoded_packet_size > kMaxDecodedPacketSize)
    return NULL;

  // Make sure the packet size and buffer capacity parameters are valid.
  if (buffer_capacity < decoded_packet_size)
    return NULL;

  if (channels <= 0 || channels > kMaxChannels)
    return NULL;

  if (sample_rate <= 0 || sample_rate > kMaxSampleRate)
    return NULL;

  if (bits_per_sample <= 0 || bits_per_sample > kMaxBitsPerSample)
    return NULL;

  // Create the stream in the first place.
  AudioOutputStream* stream =
      AudioManager::GetAudioManager()->MakeAudioStream(
          format, channels, sample_rate, bits_per_sample);

  uint32 hardware_packet_size = kSamplesPerHardwarePacket * channels *
                                bits_per_sample / 8;
  if (stream && !stream->Open(hardware_packet_size)) {
    stream->Close();
    stream = NULL;
  }

  if (stream) {
    IPCAudioSource* source = new IPCAudioSource(
        host,
        process_id,
        route_id,
        stream_id,
        stream,
        hardware_packet_size,
        decoded_packet_size,
        buffer_capacity);
    // If we can open the stream, proceed with sharing the shared memory.
    base::SharedMemoryHandle foreign_memory_handle;

    // Time to create the PCM transport. Either low latency or regular latency
    // If things go well we send a message back to the renderer with the
    // transport information.
    // Note that the low latency mode is not yet ready and the if part of this
    // method is never executed. TODO(cpu): Enable this mode.

    if (source->shared_memory_.Create(L"",
                                      false,
                                      false,
                                      decoded_packet_size) &&
        source->shared_memory_.Map(decoded_packet_size) &&
        source->shared_memory_.ShareToProcess(process_handle,
                                              &foreign_memory_handle)) {
      if (low_latency) {
        // Low latency mode. We use SyncSocket to signal.
        base::SyncSocket* sockets[2] = {0};
        if (base::SyncSocket::CreatePair(sockets)) {
          source->shared_socket_.reset(sockets[0]);
#if defined(OS_WIN)
          HANDLE foreign_socket_handle = 0;
          ::DuplicateHandle(GetCurrentProcess(), sockets[1]->handle(),
                            process_handle, &foreign_socket_handle,
                            0, FALSE, DUPLICATE_SAME_ACCESS);
          bool valid = foreign_socket_handle != 0;
#else
          base::FileDescriptor foreign_socket_handle(sockets[1]->handle(),
                                                     false);
          bool valid = foreign_socket_handle.fd != -1;
#endif

          if (valid) {
            host->Send(new ViewMsg_NotifyLowLatencyAudioStreamCreated(
                route_id, stream_id, foreign_memory_handle,
                foreign_socket_handle, decoded_packet_size));
            return source;
          }
        }
      } else {
        // Regular latency mode.
        host->Send(new ViewMsg_NotifyAudioStreamCreated(
            route_id, stream_id, foreign_memory_handle, decoded_packet_size));

        // Also request the first packet to kick start the pre-rolling.
        source->StartBuffering();
        return source;
      }
    }
    // Failure. Close and free acquired resources.
    source->Close();
    delete source;
  }

  host->SendErrorMessage(route_id, stream_id);
  return NULL;
}

void AudioRendererHost::IPCAudioSource::Play() {
  // We can start from created or paused state.
  if (!stream_ || (state_ != kCreated && state_ != kPaused))
    return;

  ViewMsg_AudioStreamState_Params state;
  state.state = ViewMsg_AudioStreamState_Params::kPlaying;
  host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
      route_id_, stream_id_, state));

  State old_state;
  // Update the state and notify renderer.
  {
    AutoLock auto_lock(lock_);
    old_state = state_;
    state_ = kPlaying;
  }

  if (old_state == kCreated)
    stream_->Start(this);
}

void AudioRendererHost::IPCAudioSource::Pause() {
  // We can pause from started state.
  if (state_ != kPlaying)
    return;

  // Update the state and notify renderer.
  {
    AutoLock auto_lock(lock_);
    state_ = kPaused;
  }

  ViewMsg_AudioStreamState_Params state;
  state.state = ViewMsg_AudioStreamState_Params::kPaused;
  host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
      route_id_, stream_id_, state));
}

void AudioRendererHost::IPCAudioSource::Flush() {
  if (state_ != kPaused)
    return;

  // The following operation is atomic in PushSource so we don't need to lock.
  push_source_.ClearAll();
}

void AudioRendererHost::IPCAudioSource::Close() {
  if (!stream_)
    return;

  stream_->Stop();
  stream_->Close();
  // After stream is closed it is destroyed, so don't keep a reference to it.
  stream_ = NULL;

  // Update the current state.
  state_ = kClosed;
}

void AudioRendererHost::IPCAudioSource::SetVolume(double volume) {
  // TODO(hclam): maybe send an error message back to renderer if this object
  // is in a wrong state.
  if (!stream_)
    return;
  stream_->SetVolume(volume);
}

void AudioRendererHost::IPCAudioSource::GetVolume() {
  // TODO(hclam): maybe send an error message back to renderer if this object
  // is in a wrong state.
  if (!stream_)
    return;
  double volume;
  stream_->GetVolume(&volume);
  host_->Send(new ViewMsg_NotifyAudioStreamVolume(route_id_, stream_id_,
                                                  volume));
}

uint32 AudioRendererHost::IPCAudioSource::OnMoreData(AudioOutputStream* stream,
                                                     void* dest,
                                                     uint32 max_size,
                                                     uint32 pending_bytes) {
  AutoLock auto_lock(lock_);

  // Record the callback time.
  last_callback_time_ = base::Time::Now();

  if (state_ != kPlaying) {
    // Don't read anything. Save the number of bytes in the hardware buffer.
    pending_bytes_  = pending_bytes;
    return 0;
  }

  uint32 size;
  if (!shared_socket_.get()) {
    // Push source doesn't need to know the stream and number of pending bytes.
    // So just pass in NULL and 0 for them.
    size = push_source_.OnMoreData(NULL, dest, max_size, 0);
    pending_bytes_ = pending_bytes + size;
    SubmitPacketRequest(&auto_lock);
  } else {
    // Low latency mode.
    size = std::min(shared_memory_.max_size(), max_size);
    memcpy(dest, shared_memory_.memory(), size);
    memset(shared_memory_.memory(), 0, shared_memory_.max_size());
    shared_socket_->Send(&pending_bytes, sizeof(pending_bytes));
  }

  return size;
}

void AudioRendererHost::IPCAudioSource::OnClose(AudioOutputStream* stream) {
  // Push source doesn't need to know the stream so just pass in NULL.
  if (!shared_socket_.get())
    push_source_.OnClose(NULL);
  else
    shared_socket_->Close();
}

void AudioRendererHost::IPCAudioSource::OnError(AudioOutputStream* stream,
                                                int code) {
  host_->SendErrorMessage(route_id_, stream_id_);
  // The following method call would cause this object to be destroyed on IO
  // thread.
  host_->DestroySource(this);
}

void AudioRendererHost::IPCAudioSource::NotifyPacketReady(
    uint32 decoded_packet_size) {
  // Packet ready notifications do not happen in low latency mode. If they
  // do something is horribly wrong.
  DCHECK(!shared_socket_.get());

  AutoLock auto_lock(lock_);
  outstanding_request_ = false;
  // If reported size is greater than capacity of the shared memory, we have
  // an error.
  if (decoded_packet_size <= decoded_packet_size_) {
    bool ok = push_source_.Write(
        static_cast<char*>(shared_memory_.memory()), decoded_packet_size);

    // Submit packet request if we have written something.
    if (ok)
      SubmitPacketRequest(&auto_lock);
  }
}

void AudioRendererHost::IPCAudioSource::SubmitPacketRequest_Locked() {
  lock_.AssertAcquired();
  // Submit a new request when these two conditions are fulfilled:
  // 1. No outstanding request
  // 2. There's space for data of the new request.
  if (!outstanding_request_ &&
      (push_source_.UnProcessedBytes() + decoded_packet_size_ <=
       buffer_capacity_)) {
    outstanding_request_ = true;

    // This variable keeps track of the total amount of bytes buffered for
    // the associated AudioOutputStream. This value should consist of bytes
    // buffered in AudioOutputStream and those kept inside |push_source_|.
    uint32 buffered_bytes = pending_bytes_ + push_source_.UnProcessedBytes();
    host_->Send(
        new ViewMsg_RequestAudioPacket(
            route_id_,
            stream_id_,
            buffered_bytes,
            last_callback_time_.ToInternalValue()));
  }
}

void AudioRendererHost::IPCAudioSource::SubmitPacketRequest(AutoLock* alock) {
  if (alock) {
    SubmitPacketRequest_Locked();
  } else {
    AutoLock auto_lock(lock_);
    SubmitPacketRequest_Locked();
  }
}

void AudioRendererHost::IPCAudioSource::StartBuffering() {
  SubmitPacketRequest(NULL);
}

//-----------------------------------------------------------------------------
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost()
    : process_id_(0),
      process_handle_(0),
      ipc_sender_(NULL) {
  // Increase the ref count of this object so it is active until we do
  // Release().
  AddRef();
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(sources_.empty());
}

void AudioRendererHost::Destroy() {
  // Post a message to the thread where this object should live and do the
  // actual operations there.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &AudioRendererHost::OnDestroyed));
}

// Event received when IPC channel is connected to the renderer process.
void AudioRendererHost::IPCChannelConnected(int process_id,
                                            base::ProcessHandle process_handle,
                                            IPC::Message::Sender* ipc_sender) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  process_id_ = process_id;
  process_handle_ = process_handle;
  ipc_sender_ = ipc_sender;
}

// Event received when IPC channel is closing.
void AudioRendererHost::IPCChannelClosing() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  ipc_sender_ = NULL;
  process_handle_ = 0;
  process_id_ = 0;
  DestroyAllSources();
}

bool AudioRendererHost::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  if (!IsAudioRendererHostMessage(message))
    return false;
  *message_was_ok = true;

  IPC_BEGIN_MESSAGE_MAP_EX(AudioRendererHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateAudioStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PlayAudioStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PauseAudioStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FlushAudioStream, OnFlushStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseAudioStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_NotifyAudioPacketReady, OnNotifyPacketReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioVolume, OnGetVolume)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetAudioVolume, OnSetVolume)
  IPC_END_MESSAGE_MAP_EX()

  return true;
}

bool AudioRendererHost::IsAudioRendererHostMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_CreateAudioStream::ID:
    case ViewHostMsg_PlayAudioStream::ID:
    case ViewHostMsg_PauseAudioStream::ID:
    case ViewHostMsg_FlushAudioStream::ID:
    case ViewHostMsg_CloseAudioStream::ID:
    case ViewHostMsg_NotifyAudioPacketReady::ID:
    case ViewHostMsg_GetAudioVolume::ID:
    case ViewHostMsg_SetAudioVolume::ID:
      return true;
    default:
      break;
    }
    return false;
}

void AudioRendererHost::OnCreateStream(
    const IPC::Message& msg, int stream_id,
    const ViewHostMsg_Audio_CreateStream_Params& params, bool low_latency) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(Lookup(msg.routing_id(), stream_id) == NULL);

  IPCAudioSource* source = IPCAudioSource::CreateIPCAudioSource(
      this,
      process_id_,
      msg.routing_id(),
      stream_id,
      process_handle_,
      params.format,
      params.channels,
      params.sample_rate,
      params.bits_per_sample,
      params.packet_size,
      params.buffer_capacity,
      low_latency);

  // If we have created the source successfully, adds it to the map.
  if (source) {
    sources_.insert(
        std::make_pair(
            SourceID(source->route_id(), source->stream_id()), source));
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnPlayStream(const IPC::Message& msg, int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->Play();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnPauseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->Pause();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnFlushStream(const IPC::Message& msg, int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->Flush();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnCloseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    DestroySource(source);
  }
}

void AudioRendererHost::OnSetVolume(const IPC::Message& msg, int stream_id,
                                    double volume) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->SetVolume(volume);
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnGetVolume(const IPC::Message& msg, int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->GetVolume();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnNotifyPacketReady(const IPC::Message& msg,
                                            int stream_id, uint32 packet_size) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->NotifyPacketReady(packet_size);
  } else {
    SendErrorMessage(msg.routing_id(), stream_id);
  }
}

void AudioRendererHost::OnDestroyed() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  ipc_sender_ = NULL;
  process_handle_ = 0;
  process_id_ = 0;
  DestroyAllSources();
  // Decrease the reference to this object, which may lead to self-destruction.
  Release();
}

void AudioRendererHost::OnSend(IPC::Message* message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (ipc_sender_) {
    ipc_sender_->Send(message);
  }
}

void AudioRendererHost::OnDestroySource(IPCAudioSource* source) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (source) {
    sources_.erase(SourceID(source->route_id(), source->stream_id()));
    source->Close();
    delete source;
  }
}

void AudioRendererHost::DestroyAllSources() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  std::vector<IPCAudioSource*> sources;
  for (SourceMap::iterator i = sources_.begin(); i != sources_.end(); ++i) {
    sources.push_back(i->second);
  }
  for (size_t i = 0; i < sources.size(); ++i) {
    DestroySource(sources[i]);
  }
  DCHECK(sources_.empty());
}

AudioRendererHost::IPCAudioSource* AudioRendererHost::Lookup(int route_id,
                                                             int stream_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  SourceMap::iterator i = sources_.find(SourceID(route_id, stream_id));
  if (i != sources_.end())
    return i->second;
  return NULL;
}

void AudioRendererHost::Send(IPC::Message* message) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    OnSend(message);
  } else {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &AudioRendererHost::OnSend, message));
  }
}

void AudioRendererHost::SendErrorMessage(int32 render_view_id,
                                         int32 stream_id) {
  ViewMsg_AudioStreamState_Params state;
  state.state = ViewMsg_AudioStreamState_Params::kError;
  Send(new ViewMsg_NotifyAudioStreamStateChanged(
      render_view_id, stream_id, state));
}

void AudioRendererHost::DestroySource(IPCAudioSource* source) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    OnDestroySource(source);
  } else {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &AudioRendererHost::OnDestroySource, source));
  }
}
