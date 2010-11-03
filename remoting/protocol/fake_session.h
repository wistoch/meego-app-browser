// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_SESSION_H_
#define REMOTING_PROTOCOL_FAKE_SESSION_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "net/socket/socket.h"
#include "remoting/protocol/session.h"

namespace remoting {

namespace protocol {

extern const char kTestJid[];

// FakeSocket implement net::Socket interface for FakeConnection. All data
// written to FakeSocket is stored in a buffer returned by written_data().
// Read() reads data from another buffer that can be set with AppendInputData().
// Pending reads are supported, so if there is a pending read AppendInputData()
// calls the read callback.
class FakeSocket : public net::Socket {
 public:
  FakeSocket();
  virtual ~FakeSocket();

  const std::vector<char>& written_data() { return written_data_; }

  void AppendInputData(char* data, int data_size);
  int input_pos() { return input_pos_; }

  // net::Socket interface.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback* read_callback_;

  std::vector<char> written_data_;
  std::vector<char> input_data_;
  int input_pos_;
};

// FakeSession is a dummy protocol::Session that uses FakeSocket for all
// channels.
class FakeSession : public Session {
 public:
  FakeSession();
  virtual ~FakeSession();

  StateChangeCallback* state_change_callback() { return callback_.get(); }

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  bool is_closed() { return closed_; }

  virtual void SetStateChangeCallback(StateChangeCallback* callback);

  virtual FakeSocket* control_channel();
  virtual FakeSocket* event_channel();
  virtual FakeSocket* video_channel();

  virtual FakeSocket* video_rtp_channel();
  virtual FakeSocket* video_rtcp_channel();

  virtual const std::string& jid();

  virtual MessageLoop* message_loop();
  virtual const CandidateChromotocolConfig* candidate_config();
  virtual const ChromotocolConfig* config();
  virtual void set_config(const ChromotocolConfig* config);

  virtual void Close(Task* closed_task);

 public:
  scoped_ptr<StateChangeCallback> callback_;
  scoped_ptr<const CandidateChromotocolConfig> candidate_config_;
  scoped_ptr<const ChromotocolConfig> config_;
  MessageLoop* message_loop_;
  FakeSocket control_channel_;
  FakeSocket event_channel_;
  FakeSocket video_channel_;
  FakeSocket video_rtp_channel_;
  FakeSocket video_rtcp_channel_;
  std::string jid_;
  bool closed_;
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_SESSION_H_
