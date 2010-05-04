// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software adjust volume of samples, allows each audio stream its own
// volume without impacting master volume for chrome and other applications.

// Implemented as templates to allow 8, 16 and 32 bit implementations.
// 8 bit is unsigned and biased by 128.

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"

namespace media {

namespace {

// TODO(fbarchard): Convert to intrinsics for better efficiency.

template<class Fixed>
static int ScaleChannel(int channel, int volume) {
  return static_cast<int>((static_cast<Fixed>(channel) * volume) >> 16);
}

template<class Format, class Fixed, int bias>
static void AdjustVolume(Format* buf_out, int sample_count, int fixed_volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = static_cast<Format>(ScaleChannel<Fixed>(buf_out[i] - bias,
                                                         fixed_volume) + bias);
  }
}

static void AdjustVolumeFloat(float* buf_out, int sample_count, float volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = buf_out[i] * volume;
  }
}


// Channel order for AAC
// From http://www.hydrogenaudio.org/forums/lofiversion/index.php/t40046.html

static const int kChannel_C = 0;
static const int kChannel_L = 1;
static const int kChannel_R = 2;

template<class Fixed, int min_value, int max_value>
static int AddChannel(int val, int adder) {
  Fixed sum = static_cast<Fixed>(val) + static_cast<Fixed>(adder);
  if (sum > max_value)
    return max_value;
  if (sum < min_value)
    return min_value;
  return static_cast<int>(sum);
}

// FoldChannels() downmixes multichannel (ie 5.1 Surround Sound) to Stereo.
// Left and Right channels are preserved asis, and Center channel is
// distributed equally to both sides.  To be perceptually 1/2 volume on
// both channels, 1/sqrt(2) is used instead of 1/2.
// Fixed point math is used for efficiency.  16 bits of fraction and 8,16 or 32
// bits of integer are used.
// 8 bit samples are unsigned and 128 represents 0, so a bias is removed before
// doing calculations, then readded for the final output.

template<class Format, class Fixed, int min_value, int max_value, int bias>
static void FoldChannels(Format* buf_out,
                         int sample_count,
                         const float volume,
                         int channels) {
  Format* buf_in = buf_out;
  const int center_volume = static_cast<int>(volume * 0.707f * 65536);
  const int fixed_volume = static_cast<int>(volume * 65536);

  for (int i = 0; i < sample_count; ++i) {
    int center = static_cast<int>(buf_in[kChannel_C] - bias);
    int left = static_cast<int>(buf_in[kChannel_L] - bias);
    int right = static_cast<int>(buf_in[kChannel_R] - bias);

    center = ScaleChannel<Fixed>(center, center_volume);
    left = ScaleChannel<Fixed>(left, fixed_volume);
    right = ScaleChannel<Fixed>(right, fixed_volume);

    buf_out[0] = static_cast<Format>(
        AddChannel<Fixed, min_value, max_value>(left, center) + bias);
    buf_out[1] = static_cast<Format>(
        AddChannel<Fixed, min_value, max_value>(right, center) + bias);

    buf_out += 2;
    buf_in += channels;
  }
}

static void FoldChannelsFloat(float* buf_out,
                              int sample_count,
                              const float volume,
                              int channels) {
  float* buf_in = buf_out;
  const float center_volume = volume * 0.707f;

  for (int i = 0; i < sample_count; ++i) {
    float center = buf_in[kChannel_C];
    float left = buf_in[kChannel_L];
    float right = buf_in[kChannel_R];

    center = center * center_volume;
    left = left * volume;
    right = right * volume;

    buf_out[0] = left + center;
    buf_out[1] = right + center;

    buf_out += 2;
    buf_in += channels;
  }
}

}  // namespace

// AdjustVolume() does an in place audio sample change.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  DCHECK(volume >= 0.0f && volume <= 1.0f);
  if (volume == 1.0f) {
    return true;
  } else if (volume == 0.0f) {
    memset(buf, 0, buflen);
    return true;
  }
  if (channels > 0 && channels <= 6 && bytes_per_sample > 0) {
    int sample_count = buflen / bytes_per_sample;
    const int fixed_volume = static_cast<int>(volume * 65536);
    if (bytes_per_sample == 1) {
      AdjustVolume<uint8, int32, 128>(reinterpret_cast<uint8*>(buf),
                                      sample_count,
                                      fixed_volume);
      return true;
    } else if (bytes_per_sample == 2) {
      AdjustVolume<int16, int32, 0>(reinterpret_cast<int16*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    } else if (bytes_per_sample == 4) {
      // 4 byte per sample is float.
      AdjustVolumeFloat(reinterpret_cast<float*>(buf),
                        sample_count,
                        volume);
      return true;
    }
  }
  return false;
}

bool FoldChannels(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  DCHECK(volume >= 0.0f && volume <= 1.0f);
  if (channels >= 5 && channels <= 6 && bytes_per_sample > 0) {
    int sample_count = buflen / (channels * bytes_per_sample);
    if (bytes_per_sample == 1) {
      FoldChannels<uint8, int32, -128, 127, 128>(
          reinterpret_cast<uint8*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 2) {
      FoldChannels<int16, int32, -32768, 32767, 0>(
          reinterpret_cast<int16*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 4) {
      // 4 byte per sample is float.
      FoldChannelsFloat(
          reinterpret_cast<float*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    }
  }
  return false;
}

}  // namespace media
