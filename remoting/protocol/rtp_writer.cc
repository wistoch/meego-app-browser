// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_writer.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/rtp_utils.h"

namespace remoting {

namespace {
const int kMtu = 1200;
const uint8 kRtpPayloadTypePrivate = 96;
}

RtpWriter::RtpWriter()
    : rtp_socket_(NULL),
      rtcp_socket_(NULL),
      last_packet_number_(0) {
}

RtpWriter::~RtpWriter() { }

// Initializes the writer. Must be called on the thread the sockets belong
// to.
void RtpWriter::Init(net::Socket* rtp_socket, net::Socket* rtcp_socket) {
  buffered_rtp_writer_ = new BufferedDatagramWriter();
  buffered_rtp_writer_->Init(rtp_socket, NULL);
  rtp_socket_ = rtp_socket;
  rtcp_socket_ = rtcp_socket;
}

void RtpWriter::SendPacket(const char* data, int packet_size,
                           uint32 timestamp) {
  RtpHeader header;
  header.padding = false;
  header.extension = false;
  header.sources = 0;
  header.payload_type = kRtpPayloadTypePrivate;
  header.timestamp = timestamp;

  // TODO(sergeyu): RTP requires that SSRC is chosen randomly by each
  // participant. There are only two participants in chromoting session,
  // so SSRC isn't useful. Implement it in future if neccessary.
  header.sync_source_id = 0;

  // TODO(sergeyu): Add VP8 payload header.

  int position = 0;
  while (position < packet_size) {
    // Allocate buffer.
    int size = std::max(kMtu, packet_size - position);
    int header_size = GetRtpHeaderSize(header.sources) + size;
    int total_size = size + header_size;
    net::IOBufferWithSize* buffer = new net::IOBufferWithSize(total_size);

    // Set marker if this is the last frame.
    header.marker = (position + size) == packet_size;

    // TODO(sergeyu): Handle sequence number wrapping.
    header.sequence_number = last_packet_number_;
    ++last_packet_number_;

    // Generate header.
    PackRtpHeader(reinterpret_cast<uint8*>(buffer->data()), buffer->size(),
                  header);

    // Copy data to the buffer.
    memcpy(buffer->data() + header_size, data + position, size);

    // Send it.
    buffered_rtp_writer_->Write(buffer);

    position += size;
  }

  DCHECK_EQ(position, packet_size);
}

// Stop writing and drop pending data. Must be called from the same thread as
// Init().
void RtpWriter::Close() {
  buffered_rtp_writer_->Close();
}

}  // namespace remoting
