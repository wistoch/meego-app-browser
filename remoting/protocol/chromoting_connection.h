// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_
#define REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_

#include <string>

#include "base/callback.h"
#include "remoting/protocol/chromotocol_config.h"

class MessageLoop;
class Task;

namespace net {
class Socket;
}  // namespace net

namespace remoting {

// Generic interface for Chromoting connection used by both client and host.
// Provides access to the connection channels, but doesn't depend on the
// protocol used for each channel.
// TODO(sergeyu): Remove refcounting?
// TODO(sergeyu): Rename to ChromotocolConnection.
class ChromotingConnection
    : public base::RefCountedThreadSafe<ChromotingConnection> {
 public:
  enum State {
    INITIALIZING,
    CONNECTING,
    CONNECTED,
    CLOSED,
    FAILED,
  };

  typedef Callback1<State>::Type StateChangeCallback;

  // Set callback that is called when state of the connection is changed.
  // Must be called on the jingle thread only.
  virtual void SetStateChangeCallback(StateChangeCallback* callback) = 0;

  // Reliable PseudoTCP channels for this connection.
  // TODO(sergeyu): Remove VideoChannel, and use RTP channels instead.
  // TODO(sergeyu): Make it possible to create/destroy new channels on-fly?
  virtual net::Socket* GetEventChannel() = 0;
  virtual net::Socket* GetVideoChannel() = 0;

  // Unreliable channels for this connection.
  virtual net::Socket* GetVideoRtpChannel() = 0;
  virtual net::Socket* GetVideoRtcpChannel() = 0;

  // JID of the other side.
  virtual const std::string& jid() = 0;

  // Message loop that must be used for to access the channels of this
  // connection.
  virtual MessageLoop* message_loop() = 0;

  // Configuration of the protocol requested by the client.
  // Returned pointer is valid until connection is closed.
  virtual const CandidateChromotocolConfig* candidate_config() = 0;

  // Protocol configuration. Can be called only after session has been accepted.
  // Returned pointer is valid until connection is closed.
  virtual const ChromotocolConfig* config() = 0;

  // Set protocol configuration for an incoming session. Must be called
  // on the host before the connection is accepted, from
  // ChromotingServer::NewConnectionCallback. Ownership of |config| is given
  // to the connection.
  virtual void set_config(const ChromotocolConfig* config) = 0;

  // Closes connection. Callbacks are guaranteed not to be called after
  // |closed_task| is executed.
  virtual void Close(Task* closed_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ChromotingConnection>;
  virtual ~ChromotingConnection() { }
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_
