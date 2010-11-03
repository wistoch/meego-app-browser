// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_reader.h"

#include "base/task.h"
#include "remoting/protocol/chromotocol_connection.h"

namespace remoting {

ProtobufVideoReader::ProtobufVideoReader() { }
ProtobufVideoReader::~ProtobufVideoReader() { }

void ProtobufVideoReader::Init(ChromotocolConnection* connection,
                               VideoStub* video_stub) {
  reader_.Init<VideoPacket>(connection->video_channel(),
                            NewCallback(this, &ProtobufVideoReader::OnNewData));
  video_stub_ = video_stub;
}

void ProtobufVideoReader::Close() {
  reader_.Close();
}

void ProtobufVideoReader::OnNewData(VideoPacket* packet) {
  video_stub_->ProcessVideoPacket(packet, new DeleteTask<VideoPacket>(packet));
}

}  // namespace remoting
