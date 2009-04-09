// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standalone benchmarking application based on FFmpeg.  This tool is used to
// measure decoding performance between different FFmpeg compile and run-time
// options.  We also use this tool to measure performance regressions when
// testing newer builds of FFmpeg from trunk.

#include <iostream>
#include <windows.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/filters/ffmpeg_common.h"

namespace switches {
const wchar_t kStream[]                 = L"stream";
const wchar_t kVideoThreads[]           = L"video-threads";
}  // namespace switches

int main(int argc, const char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::vector<std::wstring> filenames(cmd_line->GetLooseValues());
  if (filenames.empty()) {
    std::cerr << "Usage: media_bench [OPTIONS] FILE\n"
              << "  --stream=[audio|video]          "
              << "Benchmark either the audio or video stream\n"
              << "  --video-threads=N               "
              << "Decode video using N threads" << std::endl;
    return 1;
  }

  // Retrieve command line options.
  std::string path(WideToUTF8(filenames[0]));
  CodecType target_codec = CODEC_TYPE_UNKNOWN;
  int video_threads = 0;

  // Determine whether to benchmark audio or video decoding.
  std::wstring stream(cmd_line->GetSwitchValue(switches::kStream));
  if (!stream.empty()) {
    if (stream.compare(L"audio") == 0) {
      target_codec = CODEC_TYPE_AUDIO;
    } else if (stream.compare(L"video") == 0) {
      target_codec = CODEC_TYPE_VIDEO;
    } else {
      std::cerr << "Unknown --stream option " << stream << std::endl;
      return 1;
    }
  }

  // Determine number of threads to use for video decoding (optional).
  std::wstring threads(cmd_line->GetSwitchValue(switches::kVideoThreads));
  if (!threads.empty() && !StringToInt(threads, &video_threads)) {
    video_threads = 0;
  }

  // Register FFmpeg and attempt to open file.
  avcodec_init();
  av_register_all();
  AVFormatContext* format_context = NULL;
  if (av_open_input_file(&format_context, path.c_str(), NULL, 0, NULL) < 0) {
    std::cerr << "Could not open " << path << std::endl;
    return 1;
  }

  // Parse a little bit of the stream to fill out the format context.
  if (av_find_stream_info(format_context) < 0) {
    std::cerr << "Could not find stream info for " << path << std::endl;
    return 1;
  }

  // Find our target stream.
  int target_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    // See if we found our target codec.
    if (codec_context->codec_type == target_codec && target_stream < 0) {
      std::cout << "* ";
      target_stream = i;
    } else {
      std::cout << "  ";
    }

    // Print out stream information
    std::cout << "Stream #" << i << ": " << codec->name << " ("
              << codec->long_name << ")" << std::endl;
  }

  // Only continue if we found our target stream.
  if (target_stream < 0) {
    return 1;
  }

  // Prepare FFmpeg structures.
  AVPacket packet;
  AVCodecContext* codec_context = format_context->streams[target_stream]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  // Initialize threaded decode.
  if (target_codec == CODEC_TYPE_VIDEO && video_threads > 0) {
    if (avcodec_thread_init(codec_context, video_threads) < 0) {
      std::cerr << "WARNING: Could not initialize threading!\n"
                << "Did you build with pthread/w32thread support?" << std::endl;
    }
  }

  // Initialize our codec.
  if (avcodec_open(codec_context, codec) < 0) {
    std::cerr << "Could not open codec " << codec_context->codec->name
              << std::endl;
    return 1;
  }

  // Buffer used for audio decoding.
  int16* samples =
      reinterpret_cast<int16*>(av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE));

  // Buffer used for video decoding.
  AVFrame* frame = avcodec_alloc_frame();
  if (!frame) {
    std::cerr << "Could not allocate an AVFrame" << std::endl;
    return 1;
  }

  // Stats collector.
  std::vector<double> decode_times;
  decode_times.reserve(4096);

  // Parse through the entire stream until we hit EOF.
  base::TimeTicks start = base::TimeTicks::HighResNow();
  while (av_read_frame(format_context, &packet) >= 0) {
    // Only decode packets from our target stream.
    if (packet.stream_index == target_stream) {
      int result = -1;
      base::TimeTicks decode_start = base::TimeTicks::HighResNow();
      if (target_codec == CODEC_TYPE_AUDIO) {
        int size_out = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        result = avcodec_decode_audio2(codec_context, samples, &size_out,
                                       packet.data, packet.size);
      } else if (target_codec == CODEC_TYPE_VIDEO) {
        int got_picture = 0;
        result = avcodec_decode_video(codec_context, frame, &got_picture,
                                      packet.data, packet.size);
      } else {
        NOTREACHED();
      }
      base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;
      decode_times.push_back(delta.InMillisecondsF());

      // Make sure our decoding went OK.
      if (result < 0) {
        std::cerr << "Error while decoding" << std::endl;
        return 1;
      }
    }

    // Free our packet.
    av_free_packet(&packet);
  }
  base::TimeDelta total = base::TimeTicks::HighResNow() - start;

  // Calculate the sum.  The numbers are very consistent and the we're not too
  // worried about floating point error here.
  double sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    sum += decode_times[i];
  }

  // Calculate the average.
  double average = sum / decode_times.size();

  // Calculate the sum of the squared differences.
  double squared_sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    double difference = decode_times[i] - average;
    squared_sum += difference * difference;
  }

  // Calculate the standard deviation (jitter).
  double stddev = sqrt(squared_sum / decode_times.size());

  // Print our results.
  std::cout << std::endl;
  std::cout << "     Frames: " << decode_times.size() << std::endl;
  std::cout << "      Total: " << total.InMillisecondsF() << "ms" << std::endl;
  std::cout << "  Summation: " << sum << "ms" << std::endl;
  std::cout << "    Average: " << average << "ms" << std::endl;
  std::cout << "     StdDev: " << stddev << "ms" << std::endl;
  return 0;
}
