// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "talk/xmpp/constants.h"
#include "talk/xmllite/xmlelement.h"

namespace remoting {

namespace {
const char * const kChromotingNamespace = "google:remoting";
const buzz::QName kHeartbeatQuery(true, kChromotingNamespace, "heartbeat");
const buzz::QName kHostIdAttr(true, kChromotingNamespace, "hostid");

// TODO(sergeyu): Make this configurable by the cloud.
const int64 kHeartbeatPeriodMs = 5 * 60 * 1000;  // 5 minutes.
}

HeartbeatSender::HeartbeatSender()
    : started_(false) {
}

void HeartbeatSender::Start(HostConfig* config, JingleClient* jingle_client) {
  DCHECK(jingle_client);
  DCHECK(!started_);

  started_ = true;

  jingle_client_ = jingle_client;
  config_ = config;

  jingle_client_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoStart));
}

void HeartbeatSender::DoStart() {
  DCHECK(MessageLoop::current() == jingle_client_->message_loop());

  request_.reset(new IqRequest(jingle_client_));
  request_->set_callback(NewCallback(this, &HeartbeatSender::ProcessResponse));

  jingle_client_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza));
}

void HeartbeatSender::DoSendStanza() {
  DCHECK(MessageLoop::current() == jingle_client_->message_loop());

  LOG(INFO) << "Sending heartbeat stanza to " << kChromotingBotJid;

  buzz::XmlElement* stanza = new buzz::XmlElement(kHeartbeatQuery);
  stanza->AddAttr(kHostIdAttr, config_->host_id());
  request_->SendIq(buzz::STR_SET, kChromotingBotJid, stanza);

  // Schedule next heartbeat.
  jingle_client_->message_loop()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza),
      kHeartbeatPeriodMs);
}

void HeartbeatSender::ProcessResponse(const buzz::XmlElement* response) {
  if (response->Attr(buzz::QN_TYPE) == buzz::STR_ERROR) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
  }
}

}  // namespace remoting
