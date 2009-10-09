// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standalone benchmarking application based on FFmpeg.  This tool is used to
// measure decoding performance between different FFmpeg compile and run-time
// options.  We also use this tool to measure performance regressions when
// testing newer builds of FFmpeg from trunk.

#include "build/build_config.h"

// For pipe _setmode to binary
#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

#include <iomanip>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/djb2.h"
#include "media/base/media.h"
#include "media/bench/file_protocol.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_video_decoder.h"

namespace switches {
const wchar_t kStream[]                 = L"stream";
const wchar_t kVideoThreads[]           = L"video-threads";
const wchar_t kVerbose[]                = L"verbose";
const wchar_t kFast2[]                  = L"fast2";
const wchar_t kSkip[]                   = L"skip";
const wchar_t kFlush[]                  = L"flush";
const wchar_t kDjb2[]                   = L"djb2";
const wchar_t kMd5[]                    = L"md5";
const wchar_t kFrames[]                 = L"frames";
const wchar_t kLoop[]                   = L"loop";

}  // namespace switches

#if defined(OS_WIN)
// warning: disable warning about exception handler.
#pragma warning(disable:4509)

// Thread priorities to make benchmark more stable.

void EnterTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

void LeaveTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}
#else
void EnterTimingSection() {
  pthread_attr_t pta;
  struct sched_param param;

  pthread_attr_init(&pta);
  memset(&param, 0, sizeof(param));
  param.sched_priority = 78;
  pthread_attr_setschedparam(&pta, &param);
  pthread_attr_destroy(&pta);
}

void LeaveTimingSection() {
}
#endif

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::vector<std::wstring> filenames(cmd_line->GetLooseValues());
  if (filenames.empty()) {
    std::cerr << "Usage: media_bench [OPTIONS] FILE [DUMPFILE]\n"
              << "  --stream=[audio|video]          "
              << "Benchmark either the audio or video stream\n"
              << "  --video-threads=N               "
              << "Decode video using N threads\n"
              << "  --verbose=N                     "
              << "Set FFmpeg log verbosity (-8 to 48)\n"
              << "  --frames=N                      "
              << "Decode N frames\n"
              << "  --loop=N                        "
              << "Loop N times\n"
              << "  --fast2                         "
              << "Enable fast2 flag\n"
              << "  --flush                         "
              << "Flush last frame\n"
              << "  --djb2                          "
              << "Hash decoded buffers (DJB2)\n"
              << "  --md5                           "
              << "Hash decoded buffers (MD5)\n"
              << "  --skip=[1|2|3]                  "
              << "1=loop nonref, 2=loop, 3= frame nonref\n" << std::endl;
    return 1;
  }

  // Initialize our media library (try loading DLLs, etc.) before continuing.
  // We use an empty file path as the parameter to force searching of the
  // default locations for necessary DLLs and DSOs.
  if (media::InitializeMediaLibrary(FilePath()) == false) {
    std::cerr << "Unable to initialize the media library.";
    return 1;
  }

  // Retrieve command line options.
  std::string in_path(WideToUTF8(filenames[0]));
  std::string out_path;
  if (filenames.size() > 1) {
    out_path = WideToUTF8(filenames[1]);
  }
  CodecType target_codec = CODEC_TYPE_UNKNOWN;

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
  int video_threads = 0;
  std::wstring threads(cmd_line->GetSwitchValue(switches::kVideoThreads));
  if (!threads.empty() &&
      !StringToInt(WideToUTF16Hack(threads), &video_threads)) {
    video_threads = 0;
  }

  // FFmpeg verbosity.  See libavutil/log.h for values: -8 quiet..48 verbose.
  int verbose_level = AV_LOG_FATAL;
  std::wstring verbose(cmd_line->GetSwitchValue(switches::kVerbose));
  if (!verbose.empty() &&
      !StringToInt(WideToUTF16Hack(verbose), &verbose_level)) {
    verbose_level = AV_LOG_FATAL;
  }

  // Determine number of frames to decode (optional).
  int max_frames = 0;
  std::wstring frames_opt(cmd_line->GetSwitchValue(switches::kFrames));
  if (!frames_opt.empty() &&
      !StringToInt(WideToUTF16Hack(frames_opt), &max_frames)) {
    max_frames = 0;
  }

  // Determine number of times to loop (optional).
  int max_loops = 0;
  std::wstring loop_opt(cmd_line->GetSwitchValue(switches::kLoop));
  if (!loop_opt.empty() &&
      !StringToInt(WideToUTF16Hack(loop_opt), &max_loops)) {
    max_loops = 0;
  }

  bool fast2 = false;
  if (cmd_line->HasSwitch(switches::kFast2)) {
    fast2 = true;
  }

  bool flush = false;
  if (cmd_line->HasSwitch(switches::kFlush)) {
    flush = true;
  }

  unsigned int hash_value = 5381u;  // Seed for DJB2.
  bool hash_djb2 = false;
  if (cmd_line->HasSwitch(switches::kDjb2)) {
    hash_djb2 = true;
  }

  MD5Context ctx;  // Intermediate MD5 data: do not use
  MD5Init(&ctx);
  bool hash_md5 = false;
  if (cmd_line->HasSwitch(switches::kMd5)) {
    hash_md5 = true;
  }

  int skip = 0;
  if (cmd_line->HasSwitch(switches::kSkip)) {
    std::wstring skip_opt(cmd_line->GetSwitchValue(switches::kSkip));
    if (!StringToInt(WideToUTF16Hack(skip_opt), &skip)) {
      skip = 0;
    }
  }

  std::ostream* log_out = &std::cout;
#if defined(OS_WIN)
  // Catch exceptions so this tool can be used in automated testing.
  __try {
#endif

  // Register FFmpeg and attempt to open file.
  avcodec_init();
  av_log_set_level(verbose_level);
  av_register_all();
  av_register_protocol(&kFFmpegFileProtocol);
  AVFormatContext* format_context = NULL;
  if (av_open_input_file(&format_context, in_path.c_str(), NULL, 0, NULL) < 0) {
    std::cerr << "Error: Could not open input for "
              << in_path << std::endl;
    return 1;
  }

  // Open output file.
  FILE *output = NULL;
  if (!out_path.empty()) {
    // TODO(fbarchard): Add pipe:1 for piping to stderr.
    if (!strncmp(out_path.c_str(), "pipe:", 5) ||
        !strcmp(out_path.c_str(), "-")) {
      output = stdout;
      log_out = &std::cerr;
#if defined(OS_WIN)
      _setmode(_fileno(stdout), _O_BINARY);
#endif
    } else {
      output = file_util::OpenFile(out_path.c_str(), "wb");
    }
    if (!output) {
      std::cerr << "Error: Could not open output "
                << out_path << std::endl;
      return 1;
    }
  }

  // Parse a little bit of the stream to fill out the format context.
  if (av_find_stream_info(format_context) < 0) {
    std::cerr << "Error: Could not find stream info for "
              << in_path << std::endl;
    return 1;
  }

  // Find our target stream.
  int target_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    // See if we found our target codec.
    if (codec_context->codec_type == target_codec && target_stream < 0) {
      *log_out << "* ";
      target_stream = i;
    } else {
      *log_out << "  ";
    }

    if (!codec || (codec_context->codec_type == CODEC_TYPE_UNKNOWN)) {
      *log_out << "Stream #" << i << ": Unknown" << std::endl;
    } else {
      // Print out stream information
      *log_out << "Stream #" << i << ": " << codec->name << " ("
               << codec->long_name << ")" << std::endl;
    }
  }

  // Only continue if we found our target stream.
  if (target_stream < 0) {
    std::cerr << "Error: Could not find target stream "
              << target_stream << " for " << in_path << std::endl;
    return 1;
  }

  // Prepare FFmpeg structures.
  AVPacket packet;
  AVCodecContext* codec_context = format_context->streams[target_stream]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  // Only continue if we found our codec.
  if (!codec) {
    std::cerr << "Error: Could not find codec for "
              << in_path << std::endl;
    return 1;
  }

  if (skip == 1) {
    codec_context->skip_loop_filter = AVDISCARD_NONREF;
  } else if (skip == 2) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
  } else if (skip == 3) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
    codec_context->skip_frame = AVDISCARD_NONREF;
  }
  if (fast2) {
    codec_context->flags2 |= CODEC_FLAG2_FAST;
  }

  // Initialize threaded decode.
  if (target_codec == CODEC_TYPE_VIDEO && video_threads > 0) {
    if (avcodec_thread_init(codec_context, video_threads) < 0) {
      std::cerr << "Warning: Could not initialize threading!\n"
                << "Did you build with pthread/w32thread support?" << std::endl;
    }
  }

  // Initialize our codec.
  if (avcodec_open(codec_context, codec) < 0) {
    std::cerr << "Error: Could not open codec "
              << codec_context->codec->name << " for "
              << in_path << std::endl;
    return 1;
  }

  // Buffer used for audio decoding.
  int16* samples =
      reinterpret_cast<int16*>(av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE));

  // Buffer used for video decoding.
  AVFrame* frame = avcodec_alloc_frame();
  if (!frame) {
    std::cerr << "Error: avcodec_alloc_frame for "
              << in_path << std::endl;
    return 1;
  }

  // Stats collector.
  EnterTimingSection();
  std::vector<double> decode_times;
  decode_times.reserve(4096);
  // Parse through the entire stream until we hit EOF.
  base::TimeTicks start = base::TimeTicks::HighResNow();
  int frames = 0;
  int read_result = 0;
  do {
    read_result = av_read_frame(format_context, &packet);

    if (read_result < 0) {
      if (max_loops) {
        --max_loops;
      }
      if (max_loops > 0) {
        av_seek_frame(format_context, -1, 0, AVSEEK_FLAG_BACKWARD);
        read_result = 0;
        continue;
      }
      if (flush) {
        packet.stream_index = target_stream;
        packet.size = 0;
      } else {
        break;
      }
    }

    // Only decode packets from our target stream.
    if (packet.stream_index == target_stream) {
      int result = -1;
      if (target_codec == CODEC_TYPE_AUDIO) {
        int size_out = AVCODEC_MAX_AUDIO_FRAME_SIZE;

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_audio3(codec_context, samples, &size_out,
                                       &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (size_out) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          if (output) {
            if (fwrite(samples, 1, size_out, output) !=
                static_cast<size_t>(size_out)) {
              std::cerr << "Error: Could not write "
                        << size_out << " bytes for " << in_path << std::endl;
              return 1;
            }
          }
          if (hash_djb2) {
            hash_value = DJB2Hash(reinterpret_cast<const uint8*>(samples),
                                  size_out, hash_value);
          }
          if (hash_md5) {
            MD5Update(&ctx, reinterpret_cast<const uint8*>(samples),
                      size_out);
          }
        }
      } else if (target_codec == CODEC_TYPE_VIDEO) {
        int got_picture = 0;

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_video2(codec_context, frame, &got_picture,
                                       &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (got_picture) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          for (int plane = 0; plane < 3; ++plane) {
            const uint8* source = frame->data[plane];
            const size_t source_stride = frame->linesize[plane];
            size_t bytes_per_line = codec_context->width;
            size_t copy_lines = codec_context->height;
            if (plane != 0) {
              switch (codec_context->pix_fmt) {
                case PIX_FMT_YUV420P:
                case PIX_FMT_YUVJ420P:
                  bytes_per_line /= 2;
                  copy_lines = (copy_lines + 1) / 2;
                  break;
                case PIX_FMT_YUV422P:
                case PIX_FMT_YUVJ422P:
                  bytes_per_line /= 2;
                  break;
                case PIX_FMT_YUV444P:
                case PIX_FMT_YUVJ444P:
                  break;
                default:
                  std::cerr << "Error: Unknown video format "
                            << codec_context->pix_fmt;
                  return 1;
              }
            }
            if (output) {
              for (size_t i = 0; i < copy_lines; ++i) {
                if (fwrite(source, 1, bytes_per_line, output) !=
                           bytes_per_line) {
                  std::cerr << "Error: Could not write data after "
                            << copy_lines << " lines for "
                            << in_path << std::endl;
                  return 1;
                }
                source += source_stride;
              }
            }
            if (hash_djb2) {
              for (size_t i = 0; i < copy_lines; ++i) {
                hash_value = DJB2Hash(source, bytes_per_line, hash_value);
                source += source_stride;
              }
            }
            if (hash_md5) {
              for (size_t i = 0; i < copy_lines; ++i) {
                MD5Update(&ctx, reinterpret_cast<const uint8*>(source),
                          bytes_per_line);
                source += source_stride;
              }
            }
          }
        }
      } else {
        NOTREACHED();
      }

      // Make sure our decoding went OK.
      if (result < 0) {
        std::cerr << "Error: avcodec_decode returned "
                  << result << " for " << in_path << std::endl;
        return 1;
      }
    }
    // Free our packet.
    av_free_packet(&packet);

    if (max_frames && (frames >= max_frames))
      break;
  } while (read_result >= 0);
  base::TimeDelta total = base::TimeTicks::HighResNow() - start;
  LeaveTimingSection();

  if (output)
    file_util::CloseFile(output);

  // Calculate the sum of times.  Note that some of these may be zero.
  double sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    sum += decode_times[i];
  }

  // Print our results.
  log_out->setf(std::ios::fixed);
  log_out->precision(2);
  *log_out << std::endl;
  *log_out << "     Frames:" << std::setw(11) << frames
           << std::endl;
  *log_out << "      Total:" << std::setw(11) << total.InMillisecondsF()
           << " ms" << std::endl;
  *log_out << "  Summation:" << std::setw(11) << sum
           << " ms" << std::endl;

  if (frames > 0) {
    // Calculate the average time per frame.
    double average = sum / frames;

    // Calculate the sum of the squared differences.
    // Standard deviation will only be accurate if no threads are used.
    // TODO(fbarchard): Rethink standard deviation calculation.
    double squared_sum = 0;
    for (int i = 0; i < frames; ++i) {
      double difference = decode_times[i] - average;
      squared_sum += difference * difference;
    }

    // Calculate the standard deviation (jitter).
    double stddev = sqrt(squared_sum / frames);

    *log_out << "    Average:" << std::setw(11) << average
             << " ms" << std::endl;
    *log_out << "     StdDev:" << std::setw(11) << stddev
             << " ms" << std::endl;
  }
  if (hash_djb2) {
    *log_out << "       DJB2:" << std::setw(11) << hash_value
             << "  " << in_path << std::endl;
  }
  if (hash_md5) {
    MD5Digest digest;  // The result of the computation.
    MD5Final(&digest, &ctx);
    *log_out << "        MD5: " << MD5DigestToBase16(digest)
             << " " << in_path << std::endl;
  }
#if defined(OS_WIN)
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    *log_out << "  Exception:" << std::setw(11) << GetExceptionCode()
             << " " << in_path << std::endl;
    return 1;
  }
#endif
  return 0;
}
