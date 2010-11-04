// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"

namespace remoting {

struct ClientConfig;

namespace protocol {

class VideoStub;

class ConnectionToHost {
 public:
  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Handles an event received by the protocol::ConnectionToHost. Ownership of
    // the message is passed to the callee.
    virtual void HandleMessage(ConnectionToHost* conn,
                               ChromotingHostMessage* message) = 0;

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(ConnectionToHost* conn) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(ConnectionToHost* conn) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(ConnectionToHost* conn) = 0;
  };

  virtual ~ConnectionToHost() {}

  // TODO(ajwong): We need to generalize this API.
  virtual void Connect(const ClientConfig& config,
                       HostEventCallback* event_callback,
                       VideoStub* video_stub) = 0;
  virtual void Disconnect() = 0;

  // Send an input event to the host.
  virtual void SendEvent(const ChromotingClientMessage& msg) = 0;

 protected:
  ConnectionToHost() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
