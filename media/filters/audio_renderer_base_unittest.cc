// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util-inl.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/filters/audio_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

// Mocked subclass of AudioRendererBase for testing purposes.
class MockAudioRendererBase : public AudioRendererBase {
 public:
  MockAudioRendererBase(size_t max_queue_size)
      : AudioRendererBase(max_queue_size) {}
  virtual ~MockAudioRendererBase() {}

  // AudioRenderer implementation.
  MOCK_METHOD1(SetVolume, void(float volume));

  // AudioRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(const MediaFormat& media_format));
  MOCK_METHOD0(OnStop, void());

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererBase);
};

class AudioRendererBaseTest : public ::testing::Test {
 public:
  AudioRendererBaseTest()
      : renderer_(new MockAudioRendererBase(kMaxQueueSize)),
        decoder_(new MockAudioDecoder()) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, Read(NotNull()))
        .WillRepeatedly(Invoke(this, &AudioRendererBaseTest::EnqueueCallback));
  }

  virtual ~AudioRendererBaseTest() {
    STLDeleteElements(&read_queue_);

    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop());
    renderer_->Stop();
  }

 protected:
  static const size_t kMaxQueueSize;

  // Fixture members.
  scoped_refptr<MockAudioRendererBase> renderer_;
  scoped_refptr<MockAudioDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  StrictMock<MockFilterCallback> callback_;

  // Receives asynchronous read requests sent to |decoder_|.
  std::deque<Callback1<Buffer*>::Type*> read_queue_;

 private:
  void EnqueueCallback(Callback1<Buffer*>::Type* callback) {
    read_queue_.push_back(callback);
  }

  DISALLOW_COPY_AND_ASSIGN(AudioRendererBaseTest);
};

const size_t AudioRendererBaseTest::kMaxQueueSize = 16u;

TEST_F(AudioRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // We expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Initialize, we expect to get a bunch of read requests.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(kMaxQueueSize, read_queue_.size());
}

TEST_F(AudioRendererBaseTest, Initialize_Successful) {
  InSequence s;

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // Set up a check point to verify that the callback hasn't been executed yet.
  EXPECT_CALL(*renderer_, CheckPoint(0));

  // After finishing preroll, we expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Initialize, we expect to get a bunch of read requests.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(kMaxQueueSize, read_queue_.size());

  // Verify our callback hasn't been executed yet.
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  while (!read_queue_.empty()) {
    scoped_refptr<DataBuffer> buffer = new DataBuffer(1);
    read_queue_.front()->Run(buffer);
    delete read_queue_.front();
    read_queue_.pop_front();
  }
}

}  // namespace media
