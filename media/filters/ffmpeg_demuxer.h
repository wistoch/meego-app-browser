// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface using FFmpeg's libavformat.  At this time
// will support demuxing any audio/video format thrown at it.  The streams
// output mime types audio/x-ffmpeg and video/x-ffmpeg and include an integer
// key FFmpegCodecID which contains the CodecID enumeration value.  The CodecIDs
// can be used to create and initialize the corresponding FFmpeg decoder.
//
// FFmpegDemuxer sets the duration of pipeline during initialization by using
// the duration of the longest audio/video stream.
//
// NOTE: since FFmpegDemuxer reads packets sequentially without seeking, media
// files with very large drift between audio/video streams may result in
// excessive memory consumption.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <deque>
#include <vector>

#include "base/lock.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"

// FFmpeg forward declarations.
struct AVCodecContext;
struct AVBitStreamFilterContext;
struct AVFormatContext;
struct AVPacket;
struct AVStream;

namespace media {

class FFmpegDemuxer;

// Forward declaration for scoped_ptr_malloc.
class ScopedPtrAVFree;

class FFmpegDemuxerStream : public DemuxerStream {
 public:
  // Maintains a reference to |demuxer| and initializes itself using information
  // inside |stream|.
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, AVStream* stream);

  virtual ~FFmpegDemuxerStream();

  // Returns true is this stream has pending reads, false otherwise.
  //
  // Safe to call on any thread.
  bool HasPendingReads();

  // Enqueues and takes ownership over the given AVPacket, returns the timestamp
  // of the enqueued packet.
  base::TimeDelta EnqueuePacket(AVPacket* packet);

  // Signals to empty queue and mark next packet as discontinuous.
  void FlushBuffers();

  // Returns the duration of this stream.
  base::TimeDelta duration() { return duration_; }

  // DemuxerStream implementation.
  virtual const MediaFormat& media_format();
  virtual void Read(Callback1<Buffer*>::Type* read_callback);

  AVStream* av_stream() const { return stream_; }

  static const char* interface_id();

 protected:
  virtual void* QueryInterface(const char* interface_id);

 private:
  // Returns true if there are still pending reads.
  bool FulfillPendingReads();

  // Converts an FFmpeg stream timestamp into a base::TimeDelta.
  base::TimeDelta ConvertTimestamp(int64 timestamp);

  FFmpegDemuxer* demuxer_;
  AVStream* stream_;
  MediaFormat media_format_;
  base::TimeDelta duration_;
  bool discontinuous_;

  Lock lock_;

  typedef std::deque< scoped_refptr<Buffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<Callback1<Buffer*>::Type*> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class FFmpegDemuxer : public Demuxer {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<FFmpegDemuxer>();
  }

  // Called by FFmpegDemuxerStreams to post a demuxing task.
  void PostDemuxTask();

  // MediaFilter implementation.
  virtual void Stop();
  virtual void Seek(base::TimeDelta time);

  // Demuxer implementation.
  virtual bool Initialize(DataSource* data_source);
  virtual size_t GetNumberOfStreams();
  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id);

 private:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<FFmpegDemuxer>;
  FFmpegDemuxer();
  virtual ~FFmpegDemuxer();

  // Carries out initialization on the demuxer thread.
  void InititalizeTask(DataSource* data_source);

  // Carries out a seek on the demuxer thread.
  void SeekTask(base::TimeDelta time);

  // Carries out demuxing and satisfying stream reads on the demuxer thread.
  void DemuxTask();

  // Returns true if any of the streams have pending reads.  Since we lazily
  // post a DemuxTask() for every read, we use this method to quickly terminate
  // the tasks if there is no work to do.
  //
  // Safe to call on any thread.
  bool StreamsHavePendingReads();

  // Helper function to deep copy an AVPacket's data, size and timestamps.
  // Returns NULL if a packet could not be cloned (i.e., out of memory).
  AVPacket* ClonePacket(AVPacket* packet);

  // FFmpeg context handle.
  scoped_ptr_malloc<AVFormatContext, ScopedPtrAVFree> format_context_;

  // Latest timestamp read on the demuxer thread.
  base::TimeDelta current_timestamp_;

  // Two vector of streams:
  //   - |streams_| is indexed for the Demuxer interface GetStream(), which only
  //     contains supported streams and no NULL entries.
  //   - |packet_streams_| is indexed to mirror AVFormatContext when dealing
  //     with AVPackets returned from av_read_frame() and contain NULL entries
  //     representing unsupported streams where we throw away the data.
  //
  // Ownership is handled via reference counting.
  typedef std::vector< scoped_refptr<FFmpegDemuxerStream> > StreamVector;
  StreamVector streams_;
  StreamVector packet_streams_;

  // Thread handle.
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
