// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (main thread of render process), it intercepts audio messages
// and process them on IO thread since these messages are time critical.

#ifndef CHROME_RENDERER_AUDIO_MESSAGE_FILTER_H_
#define CHROME_RENDERER_AUDIO_MESSAGE_FILTER_H_

#include "base/id_map.h"
#include "base/shared_memory.h"
#include "ipc/ipc_channel_proxy.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class AudioMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  class Delegate {
   public:
    // Called when an audio packet is requested from the browser process.
    virtual void OnRequestPacket(size_t bytes_in_buffer,
                                 const base::Time& message_timestamp) = 0;

    // Called when state of an audio stream has changed in the browser process.
    virtual void OnStateChanged(ViewMsg_AudioStreamState state) = 0;

    // Called when an audio stream has been created in the browser process.
    virtual void OnCreated(base::SharedMemoryHandle handle, size_t length) = 0;

    // Called when notification of stream volume is received from the browser
    // process.
    virtual void OnVolume(double volume) = 0;
  };

  explicit AudioMessageFilter(int32 route_id);
  ~AudioMessageFilter();

  // Add a delegate to the map and return id of the entry.
  int32 AddDelegate(Delegate* delegate);

  // Remove a delegate referenced by |id| from the map.
  void RemoveDelegate(int32 id);

  // Sends an IPC message using |channel_|.
  bool Send(IPC::Message* message);

  MessageLoop* message_loop() { return message_loop_; }

 private:
  // For access to |message_loop_|.
  friend class AudioRendererImplTest;

  FRIEND_TEST(AudioMessageFilterTest, Basic);
  FRIEND_TEST(AudioMessageFilterTest, Delegates);

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();
  virtual void OnChannelClosing();

  // Received when browser process wants more audio packet.
  void OnRequestPacket(const IPC::Message& msg, int stream_id,
                       size_t bytes_in_buffer, int64 message_timestamp);

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
                       int length);

  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id, ViewMsg_AudioStreamState state);

  // Notification of volume property of an audio output stream.
  void OnStreamVolume(int stream_id, double volume);

  // A map of stream ids to delegates.
  IDMap<Delegate> delegates_;

  IPC::Channel* channel_;

  int32 route_id_;

  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

#endif  // CHROME_RENDERER_AUDIO_MESSAGE_FILTER_H_

