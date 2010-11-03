// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_writer.h"

#include "remoting/protocol/session.h"
#include "remoting/protocol/rtp_writer.h"

namespace remoting {

RtpVideoWriter::RtpVideoWriter() { }

RtpVideoWriter::~RtpVideoWriter() { }

void RtpVideoWriter::Init(protocol::Session* session) {
  rtp_writer_.Init(session->video_rtp_channel(),
                   session->video_rtcp_channel());
}

void RtpVideoWriter::SendPacket(const VideoPacket& packet) {
  rtp_writer_.SendPacket(packet.data().data(), packet.data().length(),
                         packet.timestamp());
}

int RtpVideoWriter::GetPendingPackets() {
  return rtp_writer_.GetPendingPackets();
}


void RtpVideoWriter::Close() {
  rtp_writer_.Close();
}

}  // namespace remoting
