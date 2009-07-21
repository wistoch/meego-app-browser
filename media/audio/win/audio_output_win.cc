// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output.h"

#include <windows.h>
#include <mmsystem.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/waveout_output_win.h"

namespace {

// The next 3 constants are some sensible limits to prevent integer overflow
// at this layer.
// Up to 6 channels can be passed to the driver.
// This should work, given the right drivers, but graceful error handling is
// needed.
// In theory 7.1 could also be supported, but it has not been tested.
// The 192 Khz constant is the frequency of quicktime lossless audio codec.
// MP4 is limited to 96 Khz, and mp3 is limited to 48 Khz.
// OGG vorbis was initially limited to 96 Khz, but recent tools are unlimited.
// 192 Khz is also the limit on most PC audio hardware.  The minimum is 100 Hz.
// Humans range is 20 to 20000 Hz.  Below 20 can be felt (woofer).

const int kMaxChannels = 6;
const int kMaxSampleRate = 192000;
const int kMaxBitsPerSample = 64;

AudioManagerWin* g_audio_manager = NULL;

}  // namespace.

bool AudioManagerWin::HasAudioDevices() {
  return (::waveOutGetNumDevs() != 0);
}

// Factory for the implementations of AudioOutputStream. Two implementations
// should suffice most windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOutWrite API (in progress)
// - PCMDXSoundAudioOutputStream: Based on DirectSound or XAudio (future work).
AudioOutputStream* AudioManagerWin::MakeAudioStream(Format format, int channels,
                                                    int sample_rate,
                                                    char bits_per_sample) {
  if ((channels > kMaxChannels) || (channels <= 0) ||
      (sample_rate > kMaxSampleRate) || (sample_rate <= 0) ||
      (bits_per_sample > kMaxBitsPerSample) || (bits_per_sample <= 0))
    return NULL;

  if (format == AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMWaveOutAudioOutputStream(this, channels, sample_rate,
                                           bits_per_sample, WAVE_MAPPER);
  }
  return NULL;
}

void AudioManagerWin::ReleaseStream(PCMWaveOutAudioOutputStream* stream) {
  if (stream)
    delete stream;
}

void AudioManagerWin::MuteAll() {
}

void AudioManagerWin::UnMuteAll() {
}

AudioManagerWin::~AudioManagerWin() {
}

void DestroyAudioManagerWin(void* param) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerWin();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerWin, NULL);
  }
  return g_audio_manager;
}
