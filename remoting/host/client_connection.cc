// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_connection.h"

#include "google/protobuf/message.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/messages_decoder.h"
#include "remoting/protocol/util.h"

namespace remoting {

// Determine how many update streams we should count to find the size of
// average update stream.
static const size_t kAverageUpdateStream = 10;

ClientConnection::ClientConnection(MessageLoop* message_loop,
                                   EventHandler* handler)
    : loop_(message_loop),
      handler_(handler) {
  DCHECK(loop_);
  DCHECK(handler_);
}

ClientConnection::~ClientConnection() {
  // TODO(hclam): When we shut down the viewer we may have to close the
  // connection.
}

void ClientConnection::Init(ChromotingConnection* connection) {
  DCHECK_EQ(connection->message_loop(), MessageLoop::current());

  connection_ = connection;
  connection_->SetStateChangeCallback(
      NewCallback(this, &ClientConnection::OnConnectionStateChange));
}

void ClientConnection::SendInitClientMessage(int width, int height) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!connection_)
    return;

  ChromotingHostMessage msg;
  msg.mutable_init_client()->set_width(width);
  msg.mutable_init_client()->set_height(height);
  DCHECK(msg.IsInitialized());
  video_writer_.SendMessage(msg);
}

void ClientConnection::SendUpdateStreamPacketMessage(
    const ChromotingHostMessage& message) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!connection_)
    return;

  video_writer_.SendMessage(message);
}

int ClientConnection::GetPendingUpdateStreamMessages() {
  DCHECK_EQ(loop_, MessageLoop::current());
  return video_writer_.GetPendingMessages();
}

void ClientConnection::Disconnect() {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If there is a channel then close it and release the reference.
  if (connection_) {
    connection_->Close(NewRunnableMethod(this, &ClientConnection::OnClosed));
    connection_ = NULL;
  }
}

void ClientConnection::OnConnectionStateChange(
    ChromotingConnection::State state) {
  if (state == ChromotingConnection::CONNECTED) {
    events_reader_.Init(
        connection_->GetEventsChannel(),
        NewCallback(this, &ClientConnection::OnMessageReceived));
    video_writer_.Init(connection_->GetVideoChannel());
  }

  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::StateChangeTask, state));
}

void ClientConnection::OnMessageReceived(ChromotingClientMessage* message) {
  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::MessageReceivedTask,
                        message));
}

void ClientConnection::StateChangeTask(ChromotingConnection::State state) {
  DCHECK_EQ(loop_, MessageLoop::current());

  DCHECK(handler_);
  switch(state) {
    case ChromotingConnection::CONNECTING:
      break;
    // Don't care about this message.
    case ChromotingConnection::CONNECTED:
      handler_->OnConnectionOpened(this);
      break;
    case ChromotingConnection::CLOSED:
      handler_->OnConnectionClosed(this);
      break;
    case ChromotingConnection::FAILED:
      handler_->OnConnectionFailed(this);
      break;
    default:
      // We shouldn't receive other states.
      NOTREACHED();
  }
}

void ClientConnection::MessageReceivedTask(ChromotingClientMessage* message) {
  DCHECK_EQ(loop_, MessageLoop::current());
  DCHECK(handler_);
  handler_->HandleMessage(this, message);
}

// OnClosed() is used as a callback for ChromotingConnection::Close().
void ClientConnection::OnClosed() {
}

}  // namespace remoting
