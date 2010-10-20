// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_WRITER_H_
#define REMOTING_PROTOCOL_RTP_WRITER_H_

#include "net/socket/socket.h"
#include "remoting/protocol/buffered_socket_writer.h"

namespace remoting {

class RtpWriter {
 public:
  RtpWriter();
  virtual ~RtpWriter();

  // Initializes the writer. Must be called on the thread the sockets belong
  // to.
  void Init(net::Socket* rtp_socket, net::Socket* rtcp_socket);

  void SendPacket(const char* buffer, int packet_size,
                  uint32 timestamp);

  // Stop writing and drop pending data. Must be called from the same thread as
  // Init().
  void Close();

 private:
  net::Socket* rtp_socket_;
  net::Socket* rtcp_socket_;

  uint32 last_packet_number_;

  scoped_refptr<BufferedDatagramWriter> buffered_rtp_writer_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_WRITER_H_
