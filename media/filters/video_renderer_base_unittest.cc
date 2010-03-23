// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {

// Mocked subclass of VideoRendererBase for testing purposes.
class MockVideoRendererBase : public VideoRendererBase {
 public:
  MockVideoRendererBase() {}
  virtual ~MockVideoRendererBase() {}

  // VideoRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(VideoDecoder* decoder));
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD0(OnFrameAvailable, void());

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRendererBase);
};

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : renderer_(new MockVideoRendererBase()),
        decoder_(new MockVideoDecoder()) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, Read(NotNull()))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::EnqueueCallback));

    // Sets the essential media format keys for this decoder.
    decoder_media_format_.SetAsString(MediaFormat::kMimeType,
                                      mime_type::kUncompressedVideo);
    decoder_media_format_.SetAsInteger(MediaFormat::kWidth, kWidth);
    decoder_media_format_.SetAsInteger(MediaFormat::kHeight, kHeight);
    EXPECT_CALL(*decoder_, media_format())
        .WillRepeatedly(ReturnRef(decoder_media_format_));
  }

  virtual ~VideoRendererBaseTest() {
    STLDeleteElements(&read_queue_);

    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop());
    renderer_->Stop();
  }

 protected:
  static const size_t kWidth;
  static const size_t kHeight;

  // Fixture members.
  scoped_refptr<MockVideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  StrictMock<MockFilterCallback> callback_;
  MediaFormat decoder_media_format_;

  // Receives asynchronous read requests sent to |decoder_|.
  std::deque<Callback1<VideoFrame*>::Type*> read_queue_;

 private:
  void EnqueueCallback(Callback1<VideoFrame*>::Type* callback) {
    read_queue_.push_back(callback);
  }

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

const size_t VideoRendererBaseTest::kWidth = 16u;
const size_t VideoRendererBaseTest::kHeight = 16u;

// Test initialization where the decoder's media format is malformed.
TEST_F(VideoRendererBaseTest, Initialize_BadMediaFormat) {
  InSequence s;

  // Don't set a media format.
  MediaFormat media_format;
  scoped_refptr<MockVideoDecoder> bad_decoder = new MockVideoDecoder();
  EXPECT_CALL(*bad_decoder, media_format())
      .WillRepeatedly(ReturnRef(media_format));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // We expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Initialize, we expect to have no reads.
  renderer_->Initialize(bad_decoder, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test initialization where the subclass failed for some reason.
TEST_F(VideoRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // We expect the video size to be set.
  EXPECT_CALL(host_, SetVideoSize(kWidth, kHeight));

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // We expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test successful initialization and preroll.
TEST_F(VideoRendererBaseTest, Initialize_Successful) {
  // Who knows how many times ThreadMain() will execute!
  //
  // TODO(scherkus): really, really, really need to inject a thread into
  // VideoRendererBase... it makes mocking much harder.
  EXPECT_CALL(host_, GetTime()).WillRepeatedly(Return(base::TimeDelta()));

  InSequence s;

  // We expect the video size to be set.
  EXPECT_CALL(host_, SetVideoSize(kWidth, kHeight));

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // After finishing initialization, we expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Verify the following expectations haven't run until we complete the reads.
  EXPECT_CALL(*renderer_, CheckPoint(0));

  // We'll expect to get notified once due preroll completing.
  EXPECT_CALL(*renderer_, OnFrameAvailable());

  MockFilterCallback seek_callback;
  EXPECT_CALL(seek_callback, OnFilterCallback());
  EXPECT_CALL(seek_callback, OnCallbackDestroyed());

  // Initialize, we shouldn't have any reads.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());

  // Now seek to trigger prerolling.
  renderer_->Seek(base::TimeDelta(), seek_callback.NewCallback());
  EXPECT_LT(0u, read_queue_.size());

  // Verify our seek callback hasn't been executed yet.
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  while (!read_queue_.empty()) {
    const base::TimeDelta kZero;
    scoped_refptr<VideoFrame> frame;
    VideoFrame::CreateFrame(VideoFrame::RGB32, kWidth, kHeight, kZero,
                            kZero, &frame);
    read_queue_.front()->Run(frame);
    delete read_queue_.front();
    read_queue_.pop_front();
  }
}

}  // namespace media
