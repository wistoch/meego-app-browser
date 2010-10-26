// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_chromoting_connection.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/channel_socket_adapter.h"
#include "remoting/jingle_glue/stream_socket_adapter.h"
#include "remoting/protocol/jingle_chromoting_server.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/session/tunnel/pseudotcpchannel.h"

using cricket::BaseSession;
using cricket::PseudoTcpChannel;
using cricket::Session;

namespace remoting {

namespace {
const char kControlChannelName[] = "control";
const char kEventChannelName[] = "event";
const char kVideoChannelName[] = "video";
const char kVideoRtpChannelName[] = "videortp";
const char kVideoRtcpChannelName[] = "videortcp";
}  // namespace

const char JingleChromotingConnection::kChromotingContentName[] = "chromoting";

JingleChromotingConnection::JingleChromotingConnection(
    JingleChromotingServer* server)
    : server_(server),
      state_(INITIALIZING),
      closed_(false),
      session_(NULL),
      event_channel_(NULL),
      video_channel_(NULL) {
}

JingleChromotingConnection::~JingleChromotingConnection() {
  DCHECK(closed_);
}

void JingleChromotingConnection::Init(Session* session) {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());

  session_ = session;
  jid_ = session_->remote_name();
  session_->SignalState.connect(
      this, &JingleChromotingConnection::OnSessionState);
}

bool JingleChromotingConnection::HasSession(cricket::Session* session) {
  return session_ == session;
}

Session* JingleChromotingConnection::ReleaseSession() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());

  SetState(CLOSED);
  Session* session = session_;
  if (session_)
    session_->SignalState.disconnect(this);
  session_ = NULL;
  closed_ = true;
  return session;
}

void JingleChromotingConnection::SetStateChangeCallback(
    StateChangeCallback* callback) {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  DCHECK(callback);
  state_change_callback_.reset(callback);
}

net::Socket* JingleChromotingConnection::GetControlChannel() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  return control_channel_adapter_.get();
}

net::Socket* JingleChromotingConnection::GetEventChannel() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  return event_channel_adapter_.get();
}

// TODO(sergeyu): Remove this method after we switch to RTP.
net::Socket* JingleChromotingConnection::GetVideoChannel() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  return video_channel_adapter_.get();
}

net::Socket* JingleChromotingConnection::GetVideoRtpChannel() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  return video_rtp_channel_.get();
}

net::Socket* JingleChromotingConnection::GetVideoRtcpChannel() {
  DCHECK_EQ(server_->message_loop(), MessageLoop::current());
  return video_rtcp_channel_.get();
}

const std::string& JingleChromotingConnection::jid() {
  // No synchronization is needed because jid_ is not changed
  // after new connection is passed to JingleChromotingServer callback.
  return jid_;
}

MessageLoop* JingleChromotingConnection::message_loop() {
  return server_->message_loop();
}

const CandidateChromotocolConfig*
JingleChromotingConnection::candidate_config() {
  DCHECK(candidate_config_.get());
  return candidate_config_.get();
}

void JingleChromotingConnection::set_candidate_config(
    const CandidateChromotocolConfig* candidate_config) {
  DCHECK(!candidate_config_.get());
  DCHECK(candidate_config);
  candidate_config_.reset(candidate_config);
}

const ChromotocolConfig* JingleChromotingConnection::config() {
  DCHECK(config_.get());
  return config_.get();
}

void JingleChromotingConnection::set_config(const ChromotocolConfig* config) {
  DCHECK(!config_.get());
  DCHECK(config);
  config_.reset(config);
}

void JingleChromotingConnection::Close(Task* closed_task) {
  if (MessageLoop::current() != server_->message_loop()) {
    server_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleChromotingConnection::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    if (control_channel_adapter_.get())
      control_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (control_channel_) {
      control_channel_->OnSessionTerminate(session_);
      control_channel_ = NULL;
    }

    if (event_channel_adapter_.get())
      event_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (event_channel_) {
      event_channel_->OnSessionTerminate(session_);
      event_channel_ = NULL;
    }

    if (video_channel_adapter_.get())
      video_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (video_channel_) {
      video_channel_->OnSessionTerminate(session_);
      video_channel_ = NULL;
    }

    if (video_rtp_channel_.get())
      video_rtp_channel_->Close(net::ERR_CONNECTION_CLOSED);
    if (video_rtcp_channel_.get())
      video_rtcp_channel_->Close(net::ERR_CONNECTION_CLOSED);

    if (session_)
      session_->Terminate();

    SetState(CLOSED);

    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleChromotingConnection::OnSessionState(
    BaseSession* session, BaseSession::State state) {
  DCHECK_EQ(session_, session);

  switch (state) {
    case Session::STATE_SENTINITIATE:
    case Session::STATE_RECEIVEDINITIATE:
      OnInitiate();
      break;

    case Session::STATE_SENTACCEPT:
    case Session::STATE_RECEIVEDACCEPT:
      OnAccept();
      break;

    case Session::STATE_SENTTERMINATE:
    case Session::STATE_RECEIVEDTERMINATE:
    case Session::STATE_SENTREJECT:
    case Session::STATE_RECEIVEDREJECT:
      OnTerminate();
      break;

    case Session::STATE_DEINIT:
      // Close() must have been called before this.
      NOTREACHED();
      break;

    default:
      // We don't care about other steates.
      break;
  }
}

void JingleChromotingConnection::OnInitiate() {
  jid_ = session_->remote_name();

  std::string content_name;
  // If we initiate the connection, we get to specify the content name. When
  // accepting one, the remote end specifies it.
  if (session_->initiator()) {
    content_name = kChromotingContentName;
  } else {
    const cricket::ContentInfo* content;
    content = session_->remote_description()->FirstContentByType(
        kChromotingXmlNamespace);
    CHECK(content);
    content_name = content->name;
  }

  // Create video RTP channels.
  video_rtp_channel_.reset(new TransportChannelSocketAdapter(
      session_->CreateChannel(content_name, kVideoRtpChannelName)));
  video_rtcp_channel_.reset(new TransportChannelSocketAdapter(
      session_->CreateChannel(content_name, kVideoRtcpChannelName)));

  // Create control channel.
  // TODO(sergeyu): Don't use talk_base::Thread::Current() here.
  control_channel_ =
      new PseudoTcpChannel(talk_base::Thread::Current(), session_);
  control_channel_->Connect(content_name, kControlChannelName);
  control_channel_adapter_.reset(new StreamSocketAdapter(
      control_channel_->GetStream()));

  // Create event channel.
  event_channel_ =
      new PseudoTcpChannel(talk_base::Thread::Current(), session_);
  event_channel_->Connect(content_name, kEventChannelName);
  event_channel_adapter_.reset(new StreamSocketAdapter(
      event_channel_->GetStream()));

  // Create video channel.
  // TODO(sergeyu): Remove video channel when we are ready to switch to RTP.
  video_channel_ =
      new PseudoTcpChannel(talk_base::Thread::Current(), session_);
  video_channel_->Connect(content_name, kVideoChannelName);
  video_channel_adapter_.reset(new StreamSocketAdapter(
      video_channel_->GetStream()));

  if (!session_->initiator())
    server_->AcceptConnection(this, session_);

  SetState(CONNECTING);
}

void JingleChromotingConnection::OnAccept() {
  // Set config for outgoing connections.
  if (session_->initiator()) {
    const cricket::ContentInfo* content =
        session_->remote_description()->FirstContentByType(
            kChromotingXmlNamespace);
    CHECK(content);

    const ChromotingContentDescription* content_description =
        static_cast<const ChromotingContentDescription*>(content->description);
    ChromotocolConfig* config = content_description->config()->GetFinalConfig();

    // Terminate the session if the config we received is invalid.
    if (!config || !candidate_config()->IsSupported(config)) {
      // TODO(sergeyu): Inform the user that the host is misbehaving?
      LOG(ERROR) << "Terminating outgoing session after an "
          "invalid session description has been received.";
      session_->Terminate();
      return;
    }

    set_config(config);
  }

  SetState(CONNECTED);
}

void JingleChromotingConnection::OnTerminate() {
  if (control_channel_adapter_.get())
    control_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (control_channel_) {
    control_channel_->OnSessionTerminate(session_);
    control_channel_ = NULL;
  }

  if (event_channel_adapter_.get())
    event_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (event_channel_) {
    event_channel_->OnSessionTerminate(session_);
    event_channel_ = NULL;
  }

  if (video_channel_adapter_.get())
    video_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_channel_) {
    video_channel_->OnSessionTerminate(session_);
    video_channel_ = NULL;
  }

  if (video_rtp_channel_.get())
    video_rtp_channel_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_rtcp_channel_.get())
    video_rtcp_channel_->Close(net::ERR_CONNECTION_ABORTED);

  SetState(CLOSED);

  closed_ = true;
}

void JingleChromotingConnection::SetState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    if (!closed_ && state_change_callback_.get())
      state_change_callback_->Run(new_state);
  }
}

}  // namespace remoting
