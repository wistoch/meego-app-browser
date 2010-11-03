// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
// TODO(hclam): Remove this header once MessageDispatcher is used.
#include "remoting/base/multiple_array_input_stream.h"
#include "remoting/client/client_config.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {

JingleHostConnection::JingleHostConnection(ClientContext* context)
    : context_(context),
      event_callback_(NULL) {
}

JingleHostConnection::~JingleHostConnection() {
}

void JingleHostConnection::Connect(const ClientConfig& config,
                                   HostEventCallback* event_callback,
                                   VideoStub* video_stub) {
  event_callback_ = event_callback;
  video_stub_ = video_stub;

  // Initialize |jingle_client_|.
  jingle_client_ = new JingleClient(context_->jingle_thread());
  jingle_client_->Init(config.username, config.auth_token,
                       kChromotingTokenServiceName, this);

  // Save jid of the host. The actual connection is created later after
  // |jingle_client_| is connected.
  host_jid_ = config.host_jid;
}

void JingleHostConnection::Disconnect() {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this,
                                     &JingleHostConnection::Disconnect));
    return;
  }

  control_reader_.Close();
  event_writer_.Close();
  video_reader_->Close();

  if (session_) {
    session_->Close(
        NewRunnableMethod(this, &JingleHostConnection::OnDisconnected));
  } else {
    OnDisconnected();
  }
}

void JingleHostConnection::OnControlMessage(ChromotingHostMessage* msg) {
  event_callback_->HandleMessage(this, msg);
}

void JingleHostConnection::InitSession() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Initialize chromotocol |session_manager_|.
  protocol::JingleSessionManager* session_manager =
      new protocol::JingleSessionManager(context_->jingle_thread());
  // TODO(ajwong): Make this a command switch when we're more stable.
  session_manager->set_allow_local_ips(true);
  session_manager->Init(
      jingle_client_->GetFullJid(),
      jingle_client_->session_manager(),
      NewCallback(this, &JingleHostConnection::OnNewSession));
  session_manager_ = session_manager;

  CandidateChromotocolConfig* candidate_config =
      CandidateChromotocolConfig::CreateDefault();
  // TODO(sergeyu): Set resolution in the |candidate_config| to the desired
  // resolution.

  // Initialize |session_|.
  session_ = session_manager_->Connect(
      host_jid_, candidate_config,
      NewCallback(this, &JingleHostConnection::OnSessionStateChange));
}

void JingleHostConnection::OnDisconnected() {
  session_ = NULL;

  if (session_manager_) {
    session_manager_->Close(
        NewRunnableMethod(this, &JingleHostConnection::OnServerClosed));
  } else {
    OnServerClosed();
  }
}

void JingleHostConnection::OnServerClosed() {
  session_manager_ = NULL;
  if (jingle_client_) {
    jingle_client_->Close();
    jingle_client_ = NULL;
  }
}

void JingleHostConnection::SendEvent(const ChromotingClientMessage& msg) {
  // This drops the message if we are not connected yet.
  event_writer_.SendMessage(msg);
}

// JingleClient::Callback interface.
void JingleHostConnection::OnStateChange(JingleClient* client,
                                         JingleClient::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(client);
  DCHECK(event_callback_);

  if (state == JingleClient::CONNECTED) {
    VLOG(1) << "Connected as: " << client->GetFullJid();
    InitSession();
  } else if (state == JingleClient::CLOSED) {
    VLOG(1) << "Connection closed.";
    event_callback_->OnConnectionClosed(this);
  }
}

void JingleHostConnection::OnNewSession(protocol::Session* session,
    protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  // Client always rejects incoming sessions.
  *response = protocol::SessionManager::DECLINE;
}

void JingleHostConnection::OnSessionStateChange(
    protocol::Session::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(event_callback_);

  switch (state) {
    case protocol::Session::FAILED:
      event_callback_->OnConnectionFailed(this);
      break;

    case protocol::Session::CLOSED:
      event_callback_->OnConnectionClosed(this);
      break;

    case protocol::Session::CONNECTED:
      // Initialize reader and writer.
      control_reader_.Init<ChromotingHostMessage>(
          session_->control_channel(),
          NewCallback(this, &JingleHostConnection::OnControlMessage));
      event_writer_.Init(session_->event_channel());
      video_reader_.reset(VideoReader::Create(session_->config()));
      video_reader_->Init(session_, video_stub_);
      event_callback_->OnConnectionOpened(this);
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

MessageLoop* JingleHostConnection::message_loop() {
  return context_->jingle_thread()->message_loop();
}

}  // namespace remoting
