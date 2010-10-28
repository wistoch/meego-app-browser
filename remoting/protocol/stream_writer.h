// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_WRITER_H_
#define REMOTING_PROTOCOL_STREAM_WRITER_H_

#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"

namespace remoting {

class ChromotingConnection;

class StreamWriterBase {
 public:
  StreamWriterBase();
  virtual ~StreamWriterBase();

  // Initializes the writer. Must be called on the thread the |socket| belongs
  // to.
  void Init(net::Socket* socket);

  // Return current buffer state. Can be called from any thread.
  int GetBufferSize();
  int GetPendingMessages();

  // Stop writing and drop pending data. Must be called from the same thread as
  // Init().
  void Close();

 protected:
  net::Socket* socket_;
  scoped_refptr<BufferedSocketWriter> buffered_writer_;
};

class EventStreamWriter : public StreamWriterBase {
 public:
  // Sends the |message| or returns false if called before Init().
  // Can be called on any thread.
  bool SendMessage(const ChromotingClientMessage& message);
};

class VideoStreamWriter : public StreamWriterBase {
 public:
  // Sends the |message| or returns false if called before Init().
  // Can be called on any thread.
  bool SendMessage(const ChromotingHostMessage& message);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_WRITER_H_
