// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects (i.e., copying data to the
// buffer passed into MockDataSource::Read()).
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <string>

#include "media/base/factory.h"
#include "media/base/filters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Use this template to test for object destruction by setting expectations on
// the method OnDestroy().
//
// TODO(scherkus): not sure about the naming...  perhaps contribute this back
// to gmock itself!
template<class MockClass>
class Destroyable : public MockClass {
 public:
  Destroyable() {}

  MOCK_METHOD0(OnDestroy, void());

 protected:
  virtual ~Destroyable() {
    OnDestroy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Destroyable);
};

// Helper class used to test that callbacks are executed.  It is recommend you
// combine this class with StrictMock<> to verify that the callback is executed.
// You can reuse the same instance of a MockFilterCallback many times since
// gmock will track the number of times the methods are executed.
class MockFilterCallback {
 public:
  MockFilterCallback() {}
  virtual ~MockFilterCallback() {}

  MOCK_METHOD0(OnCallbackDestroyed, void());
  MOCK_METHOD0(OnFilterCallback, void());

  // Helper method to create a new callback for this mock.  The callback will
  // call OnFilterCallback() when executed and OnCallbackDestroyed() when
  // destroyed.  Clients should use NiceMock<> or StrictMock<> depending on the
  // test.
  FilterCallback* NewCallback() {
    return new CallbackImpl(this);
  }

 private:
  // Private implementation of CallbackRunner used to trigger expectations on
  // MockFilterCallback.
  class CallbackImpl : public CallbackRunner<Tuple0> {
   public:
    explicit CallbackImpl(MockFilterCallback* mock_callback)
        : mock_callback_(mock_callback) {
    }

    virtual ~CallbackImpl() {
      mock_callback_->OnCallbackDestroyed();
    }

    virtual void RunWithParams(const Tuple0& params) {
      mock_callback_->OnFilterCallback();
    }

   private:
    MockFilterCallback* mock_callback_;

    DISALLOW_COPY_AND_ASSIGN(CallbackImpl);
  };

  DISALLOW_COPY_AND_ASSIGN(MockFilterCallback);
};

class MockDataSource : public DataSource {
 public:
  MockDataSource() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // DataSource implementation.
  MOCK_METHOD2(Initialize, void(const std::string& url,
                                FilterCallback* callback));
  const MediaFormat& media_format() { return media_format_; }
  MOCK_METHOD4(Read, void(int64 position, size_t size, uint8* data,
                          DataSource::ReadCallback* callback));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD0(IsStreaming, bool());

 protected:
  virtual ~MockDataSource() {}

 private:
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // Demuxer implementation.
  MOCK_METHOD2(Initialize, void(DataSource* data_source,
                                FilterCallback* callback));
  MOCK_METHOD0(GetNumberOfStreams, size_t());
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(int stream_id));

 protected:
  virtual ~MockDemuxer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream() {}

  // Sets the mime type of this object's media format, which is usually checked
  // to determine the type of decoder to create.
  explicit MockDemuxerStream(const std::string& mime_type) {
    media_format_.SetAsString(MediaFormat::kMimeType, mime_type);
  }

  // DemuxerStream implementation.
  const MediaFormat& media_format() { return media_format_; }
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));
  MOCK_METHOD1(QueryInterface, void*(const char* interface_id));

 protected:
  virtual ~MockDemuxerStream() {}

 private:
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder() {}

  // Sets the essential media format keys for this decoder.
  MockVideoDecoder(const std::string& mime_type, int width, int height) {
    media_format_.SetAsString(MediaFormat::kMimeType, mime_type);
    media_format_.SetAsInteger(MediaFormat::kWidth, width);
    media_format_.SetAsInteger(MediaFormat::kHeight, height);
  }

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // VideoDecoder implementation.
  MOCK_METHOD2(Initialize, void(DemuxerStream* stream,
                                FilterCallback* callback));
  const MediaFormat& media_format() { return media_format_; }
  MOCK_METHOD1(Read, void(Callback1<VideoFrame*>::Type* read_callback));

 protected:
  virtual ~MockVideoDecoder() {}

 private:
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder() {}

  // Sets the essential media format keys for this decoder.
  MockAudioDecoder(int channels, int sample_rate, int sample_bits) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedAudio);
    media_format_.SetAsInteger(MediaFormat::kChannels, channels);
    media_format_.SetAsInteger(MediaFormat::kSampleRate, sample_rate);
    media_format_.SetAsInteger(MediaFormat::kSampleBits, sample_bits);
  }

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // AudioDecoder implementation.
  MOCK_METHOD2(Initialize, void(DemuxerStream* stream,
                                FilterCallback* callback));
  const MediaFormat& media_format() { return media_format_; }
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));

 protected:
  virtual ~MockAudioDecoder() {}

 private:
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // VideoRenderer implementation.
  MOCK_METHOD2(Initialize, void(VideoDecoder* decoder,
                                FilterCallback* callback));
  MOCK_METHOD0(HasEnded, bool());

 protected:
  virtual ~MockVideoRenderer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD1(OnReceivedMessage, void(FilterMessage message));

  // AudioRenderer implementation.
  MOCK_METHOD2(Initialize, void(AudioDecoder* decoder,
                                FilterCallback* callback));
  MOCK_METHOD0(HasEnded, bool());
  MOCK_METHOD1(SetVolume, void(float volume));

 protected:
  virtual ~MockAudioRenderer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

// FilterFactory that returns canned instances of mock filters.  You can set
// expectations on the filters and then pass the factory into a pipeline.
class MockFilterFactory : public FilterFactory {
 public:
  MockFilterFactory()
      : creation_successful_(true),
        data_source_(new MockDataSource()),
        demuxer_(new MockDemuxer()),
        video_decoder_(new MockVideoDecoder()),
        audio_decoder_(new MockAudioDecoder()),
        video_renderer_(new MockVideoRenderer()),
        audio_renderer_(new MockAudioRenderer()) {
  }

  virtual ~MockFilterFactory() {}

  // Controls whether the Create() method is successful or not.
  void set_creation_successful(bool creation_successful) {
    creation_successful_ = creation_successful;
  }

  // Mock accessors.
  MockDataSource* data_source() const { return data_source_; }
  MockDemuxer* demuxer() const { return demuxer_; }
  MockVideoDecoder* video_decoder() const { return video_decoder_; }
  MockAudioDecoder* audio_decoder() const { return audio_decoder_; }
  MockVideoRenderer* video_renderer() const { return video_renderer_; }
  MockAudioRenderer* audio_renderer() const { return audio_renderer_; }

 protected:
  MediaFilter* Create(FilterType filter_type, const MediaFormat& media_format) {
    if (!creation_successful_) {
      return NULL;
    }

    switch (filter_type) {
      case FILTER_DATA_SOURCE:
        return data_source_;
      case FILTER_DEMUXER:
        return demuxer_;
      case FILTER_VIDEO_DECODER:
        return video_decoder_;
      case FILTER_AUDIO_DECODER:
        return audio_decoder_;
      case FILTER_VIDEO_RENDERER:
        return video_renderer_;
      case FILTER_AUDIO_RENDERER:
        return audio_renderer_;
      default:
        NOTREACHED() << "Unknown filter type: " << filter_type;
    }
    return NULL;
  }

 private:
  bool creation_successful_;
  scoped_refptr<MockDataSource> data_source_;
  scoped_refptr<MockDemuxer> demuxer_;
  scoped_refptr<MockVideoDecoder> video_decoder_;
  scoped_refptr<MockAudioDecoder> audio_decoder_;
  scoped_refptr<MockVideoRenderer> video_renderer_;
  scoped_refptr<MockAudioRenderer> audio_renderer_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterFactory);
};

// Helper gmock function that immediately executes and destroys the
// FilterCallback on behalf of the provided filter.  Can be used when mocking
// the Initialize() and Seek() methods.
void RunFilterCallback(::testing::Unused, FilterCallback* callback);

// Helper gmock function that immediately destroys the FilterCallback on behalf
// of the provided filter.  Can be used when mocking the Initialize() and Seek()
// methods.
void DestroyFilterCallback(::testing::Unused, FilterCallback* callback);

// Helper gmock action that calls SetError() on behalf of the provided filter.
ACTION_P2(SetError, filter, error) {
  filter->host()->SetError(error);
}

// Helper gmock action that calls SetDuration() on behalf of the provided
// filter.
ACTION_P2(SetDuration, filter, duration) {
  filter->host()->SetDuration(duration);
}

// Helper gmock action that calls SetTotalBytes() on behalf of the provided
// filter.
ACTION_P2(SetTotalBytes, filter, bytes) {
  filter->host()->SetTotalBytes(bytes);
}

// Helper gmock action that calls SetBufferedBytes() on behalf of the provided
// filter.
ACTION_P2(SetBufferedBytes, filter, bytes) {
  filter->host()->SetBufferedBytes(bytes);
}

// Helper gmock action that calls BroadcastMessage() on behalf of the provided
// filter.
ACTION_P2(BroadcastMessage, filter, message) {
  filter->host()->BroadcastMessage(message);
}

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
