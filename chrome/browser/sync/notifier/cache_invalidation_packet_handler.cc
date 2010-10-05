// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
//#include "base/string_util.h"
#include "chrome/browser/sync/sync_constants.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmpptask.h"

namespace sync_notifier {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";

const buzz::QName kQnData("google:notifier", "data");
const buzz::QName kQnSeq("", "seq");
const buzz::QName kQnSid("", "sid");
const buzz::QName kQnServiceUrl("", "serviceUrl");

// TODO(akalin): Move these task classes out so that they can be
// unit-tested.  This'll probably be done easier once we consolidate
// all the packet sending/receiving classes.

// A task that listens for ClientInvalidation messages and calls the
// given callback on them.
class CacheInvalidationListenTask : public buzz::XmppTask {
 public:
  // Takes ownership of callback.
  CacheInvalidationListenTask(Task* parent,
                              Callback1<const std::string&>::Type* callback)
      : XmppTask(parent, buzz::XmppEngine::HL_TYPE), callback_(callback) {}
  virtual ~CacheInvalidationListenTask() {}

  virtual int ProcessStart() {
    LOG(INFO) << "CacheInvalidationListenTask started";
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      LOG(INFO) << "CacheInvalidationListenTask blocked";
      return STATE_BLOCKED;
    }
    LOG(INFO) << "CacheInvalidationListenTask response received";
    std::string data;
    if (GetCacheInvalidationIqPacketData(stanza, &data)) {
      callback_->Run(data);
    } else {
      LOG(ERROR) << "Could not get packet data";
    }
    // Acknowledge receipt of the iq to the buzz server.
    // TODO(akalin): Send an error response for malformed packets.
    scoped_ptr<buzz::XmlElement> response_stanza(MakeIqResult(stanza));
    SendStanza(response_stanza.get());
    return STATE_RESPONSE;
  }

  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    LOG(INFO) << "Stanza received: "
              << notifier::XmlElementToString(*stanza);
    if (IsValidCacheInvalidationIqPacket(stanza)) {
      LOG(INFO) << "Queueing stanza";
      QueueStanza(stanza);
      return true;
    }
    LOG(INFO) << "Stanza skipped";
    return false;
  }

 private:
  bool IsValidCacheInvalidationIqPacket(const buzz::XmlElement* stanza) {
    // We make sure to compare jids (which are normalized) instead of
    // just strings -- server may use non-normalized jids in
    // attributes.
    //
    // TODO(akalin): Add unit tests for this.
    buzz::Jid to(stanza->Attr(buzz::QN_TO));
    return
        (MatchRequestIq(stanza, buzz::STR_SET, kQnData) &&
         (to == GetClient()->jid()));
  }

  bool GetCacheInvalidationIqPacketData(const buzz::XmlElement* stanza,
                            std::string* data) {
    DCHECK(IsValidCacheInvalidationIqPacket(stanza));
    const buzz::XmlElement* cache_invalidation_iq_packet =
        stanza->FirstNamed(kQnData);
    if (!cache_invalidation_iq_packet) {
      LOG(ERROR) << "Could not find cache invalidation IQ packet element";
      return false;
    }
    *data = cache_invalidation_iq_packet->BodyText();
    return true;
  }

  scoped_ptr<Callback1<const std::string&>::Type> callback_;
  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationListenTask);
};

// A task that sends a single outbound ClientInvalidation message.
class CacheInvalidationSendMessageTask : public buzz::XmppTask {
 public:
  CacheInvalidationSendMessageTask(Task* parent,
                                   const buzz::Jid& to_jid,
                                   const std::string& msg,
                                   int seq,
                                   const std::string& sid)
      : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
        to_jid_(to_jid), msg_(msg), seq_(seq), sid_(sid) {}
  virtual ~CacheInvalidationSendMessageTask() {}

  virtual int ProcessStart() {
    scoped_ptr<buzz::XmlElement> stanza(
        MakeCacheInvalidationIqPacket(to_jid_, task_id(), msg_,
                                      seq_, sid_));
    LOG(INFO) << "Sending message: "
              << notifier::XmlElementToString(*stanza.get());
    if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
      LOG(INFO) << "Error when sending message";
      return STATE_ERROR;
    }
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      LOG(INFO) << "CacheInvalidationSendMessageTask blocked...";
      return STATE_BLOCKED;
    }
    LOG(INFO) << "CacheInvalidationSendMessageTask response received: "
              << notifier::XmlElementToString(*stanza);
    // TODO(akalin): Handle errors here.
    return STATE_DONE;
  }

  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    LOG(INFO) << "Stanza received: "
              << notifier::XmlElementToString(*stanza);
    if (!MatchResponseIq(stanza, to_jid_, task_id())) {
      LOG(INFO) << "Stanza skipped";
      return false;
    }
    LOG(INFO) << "Queueing stanza";
    QueueStanza(stanza);
    return true;
  }

 private:
  static buzz::XmlElement* MakeCacheInvalidationIqPacket(
      const buzz::Jid& to_jid,
      const std::string& task_id,
      const std::string& msg,
      int seq, const std::string& sid) {
    buzz::XmlElement* iq = MakeIq(buzz::STR_SET, to_jid, task_id);
    buzz::XmlElement* cache_invalidation_iq_packet =
        new buzz::XmlElement(kQnData, true);
    iq->AddElement(cache_invalidation_iq_packet);
    cache_invalidation_iq_packet->SetAttr(kQnSeq, base::IntToString(seq));
    cache_invalidation_iq_packet->SetAttr(kQnSid, sid);
    cache_invalidation_iq_packet->SetAttr(kQnServiceUrl,
                                          browser_sync::kSyncServiceUrl);
    cache_invalidation_iq_packet->SetBodyText(msg);
    return iq;
  }

  const buzz::Jid to_jid_;
  std::string msg_;
  int seq_;
  std::string sid_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationSendMessageTask);
};

std::string MakeSid() {
  uint64 sid = base::RandUint64();
  return std::string("chrome-sync-") + base::Uint64ToString(sid);
}

}  // namespace

CacheInvalidationPacketHandler::CacheInvalidationPacketHandler(
    buzz::XmppClient* xmpp_client,
    invalidation::InvalidationClient* invalidation_client)
    : xmpp_client_(xmpp_client),
      invalidation_client_(invalidation_client),
      seq_(0),
      sid_(MakeSid()) {
  CHECK(xmpp_client_);
  CHECK(invalidation_client_);
  if (xmpp_client_->GetState() != buzz::XmppEngine::STATE_OPEN) {
    LOG(DFATAL) << "non-open xmpp_client_ passed to "
                << "CacheInvalidationPacketHandler";
    return;
  }
  xmpp_client_->SignalStateChange.connect(
      this, &CacheInvalidationPacketHandler::OnClientStateChange);
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  CHECK(network_endpoint);
  network_endpoint->RegisterOutboundListener(
      invalidation::NewPermanentCallback(
          this,
          &CacheInvalidationPacketHandler::HandleOutboundPacket));
  // Owned by xmpp_client.
  CacheInvalidationListenTask* listen_task =
      new CacheInvalidationListenTask(
          xmpp_client, NewCallback(
              this, &CacheInvalidationPacketHandler::HandleInboundPacket));
  listen_task->Start();
}

CacheInvalidationPacketHandler::~CacheInvalidationPacketHandler() {
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  CHECK(network_endpoint);
  network_endpoint->RegisterOutboundListener(NULL);
}

void CacheInvalidationPacketHandler::HandleOutboundPacket(
    invalidation::NetworkEndpoint* const& network_endpoint) {
  CHECK_EQ(network_endpoint, invalidation_client_->network_endpoint());
  if (!xmpp_client_) {
    LOG(DFATAL) << "HandleOutboundPacket() called with NULL xmpp_client_";
    return;
  }
  invalidation::string message;
  network_endpoint->TakeOutboundMessage(&message);
  std::string encoded_message;
  if (!base::Base64Encode(message, &encoded_message)) {
    LOG(ERROR) << "Could not base64-encode message to send: "
               << message;
    return;
  }
  // Owned by xmpp_client.
  CacheInvalidationSendMessageTask* send_message_task =
      new CacheInvalidationSendMessageTask(xmpp_client_,
                                           buzz::Jid(kBotJid),
                                           encoded_message,
                                           seq_, sid_);
  send_message_task->Start();
  ++seq_;
}

void CacheInvalidationPacketHandler::HandleInboundPacket(
    const std::string& packet) {
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  std::string decoded_message;
  if (!base::Base64Decode(packet, &decoded_message)) {
    LOG(ERROR) << "Could not base64-decode received message: "
               << packet;
    return;
  }
  network_endpoint->HandleInboundMessage(decoded_message);
}

void CacheInvalidationPacketHandler::OnClientStateChange(
    buzz::XmppEngine::State state) {
  switch (state) {
    case buzz::XmppEngine::STATE_OPEN:
      LOG(INFO) << "redundant STATE_OPEN message received";
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      LOG(INFO) << "xmpp_client_ closed -- setting to NULL";
      xmpp_client_->SignalStateChange.disconnect(this);
      xmpp_client_ = NULL;
      break;
    default:
      LOG(INFO) << "xmpp_client_ state changed to " << state;
      break;
  }
}

}  // namespace sync_notifier
