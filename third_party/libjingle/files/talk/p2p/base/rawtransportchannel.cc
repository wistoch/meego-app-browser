/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/p2p/base/rawtransportchannel.h"
#include "talk/base/common.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/rawtransport.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/stunport.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"

namespace {

const int MSG_DESTROY_UNUSED_PORTS = 1;

}  // namespace

namespace cricket {

RawTransportChannel::RawTransportChannel(
     const std::string &name, const std::string &session_type, RawTransport* transport,
    PortAllocator *allocator)
  : TransportChannelImpl(name, session_type), raw_transport_(transport),
    allocator_(allocator), allocator_session_(NULL), stun_port_(NULL),
    relay_port_(NULL), port_(NULL), use_relay_(false) {
}

RawTransportChannel::~RawTransportChannel() {
  delete allocator_session_;
}

int RawTransportChannel::SendPacket(const char *data, size_t size) {
  if (port_ == NULL)
    return -1;
  if (remote_address_.IsAny())
    return -1;
  return port_->SendTo(data, size, remote_address_, true);
}

int RawTransportChannel::SetOption(talk_base::Socket::Option opt, int value) {
  // TODO: allow these to be set before we have a port
  if (port_ == NULL)
    return -1;
  return port_->SetOption(opt, value);
}

int RawTransportChannel::GetError() {
  return (port_ != NULL) ? port_->GetError() : 0;
}

void RawTransportChannel::Connect() {
  // Create an allocator that only returns stun and relay ports.
  allocator_session_ = allocator_->CreateSession(name(), session_type());

  uint32 flags = PORTALLOCATOR_DISABLE_UDP | PORTALLOCATOR_DISABLE_TCP;

#if !defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
  flags |= PORTALLOCATOR_DISABLE_RELAY;
#endif
  allocator_session_->set_flags(flags);
  allocator_session_->SignalPortReady.connect(
      this, &RawTransportChannel::OnPortReady);
  allocator_session_->SignalCandidatesReady.connect(
      this, &RawTransportChannel::OnCandidatesReady);

  // The initial ports will include stun.
  allocator_session_->GetInitialPorts();
}

void RawTransportChannel::Reset() {
  set_readable(false);
  set_writable(false);

  delete allocator_session_;

  allocator_session_ = NULL;
  stun_port_ = NULL;
  relay_port_ = NULL;
  port_ = NULL;
  remote_address_ = talk_base::SocketAddress();
}

void RawTransportChannel::OnChannelMessage(const buzz::XmlElement* msg) {
  bool valid = raw_transport_->ParseAddress(NULL, msg, &remote_address_);
  ASSERT(valid);
  ASSERT(!remote_address_.IsAny());
  set_readable(true);

  // We can write once we have a port and a remote address.
  if (port_ != NULL)
    SetWritable();
}

// Note about stun classification
// Code to classify our NAT type and use the relay port if we are behind an
// asymmetric NAT is under a FEATURE_ENABLE_STUN_CLASSIFICATION #define.
// To turn this one we will have to enable a second stun address and make sure
// that the relay server works for raw UDP.  
//
// Another option is to classify the NAT type early and not offer the raw
// transport type at all if we can't support it.

void RawTransportChannel::OnPortReady(
    PortAllocatorSession* session, Port* port) {
  ASSERT(session == allocator_session_);

  if (port->type() == STUN_PORT_TYPE) {
    stun_port_ = static_cast<StunPort*>(port);

#if defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
    // We need a secondary address to determine the NAT type.
    stun_port_->PrepareSecondaryAddress();
#endif
  } else if (port->type() == RELAY_PORT_TYPE) {
    relay_port_ = static_cast<RelayPort*>(port);
  } else {
    ASSERT(false);
  }
}

void RawTransportChannel::OnCandidatesReady(
    PortAllocatorSession *session, const std::vector<Candidate>& candidates) {
  ASSERT(session == allocator_session_);
  ASSERT(candidates.size() >= 1);

  // The most recent candidate is the one we haven't seen yet.
  Candidate c = candidates[candidates.size() - 1];

  if (c.type() == STUN_PORT_TYPE) {
    ASSERT(stun_port_ != NULL);

#if defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
    // We need to wait until we have two addresses.
    if (stun_port_->candidates().size() < 2)
      return;

    // This is the second address.  If these addresses are the same, then we
    // are not behind a symmetric NAT.  Hence, a stun port should be sufficient.
    if (stun_port_->candidates()[0].address() ==
        stun_port_->candidates()[1].address()) {
      SetPort(stun_port_);
      return;
    }

    // We will need to use relay.
    use_relay_ = true;

    // If we weren't given a relay port, we'll need to request it.
    if (relay_port_ == NULL) {
      allocator_session_->StartGetAllPorts();
      return;
    }

    // If we already have a relay address, we're good.  Otherwise, we will need
    // to wait until one arrives.
    if (relay_port_->candidates().size() > 0)
      SetPort(relay_port_);
#else // defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
    // Always use the stun port.  We don't classify right now so just assume it
    // will work fine.
    SetPort(stun_port_);
#endif
  } else if (c.type() == RELAY_PORT_TYPE) {
    if (use_relay_)
      SetPort(relay_port_);
  } else {
    ASSERT(false);
  }
}

void RawTransportChannel::SetPort(Port* port) {
  ASSERT(port_ == NULL);
  port_ = port;

  // We don't need any ports other than the one we picked.
  allocator_session_->StopGetAllPorts();
  raw_transport_->session_manager()->worker_thread()->Post(
      this, MSG_DESTROY_UNUSED_PORTS, NULL);

  // Send a message to the other client containing our address.

  ASSERT(port_->candidates().size() >= 1);
  ASSERT(port_->candidates()[0].protocol() == "udp");
  talk_base::SocketAddress addr = port_->candidates()[0].address();

  buzz::XmlElement* msg = new buzz::XmlElement(kQnRawChannel);
  msg->SetAttr(buzz::QN_NAME, name());
  msg->SetAttr(QN_ADDRESS, addr.IPAsString());
  msg->SetAttr(QN_PORT, addr.PortAsString());
  SignalChannelMessage(this, msg);

  // Read all packets from this port.
  port_->EnablePortPackets();
  port_->SignalReadPacket.connect(this, &RawTransportChannel::OnReadPacket);

  // We can write once we have a port and a remote address.
  if (!remote_address_.IsAny())
    SetWritable();
}

void RawTransportChannel::SetWritable() {
  ASSERT(port_ != NULL);
  ASSERT(!remote_address_.IsAny());

  set_writable(true);

  SignalRouteChange(this, remote_address_);
}

void RawTransportChannel::OnReadPacket(
    Port* port, const char* data, size_t size, 
    const talk_base::SocketAddress& addr) {
  ASSERT(port_ == port);
  SignalReadPacket(this, data, size);
}

void RawTransportChannel::OnMessage(talk_base::Message* msg) {
  ASSERT(msg->message_id == MSG_DESTROY_UNUSED_PORTS);
  ASSERT(port_ != NULL);
  if (port_ != stun_port_) {
    stun_port_->Destroy();
    stun_port_ = NULL;
  }
  if (port_ != relay_port_ && relay_port_ != NULL) {
    relay_port_->Destroy();
    relay_port_ = NULL;
  }
}

}  // namespace cricket
