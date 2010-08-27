// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"

namespace {
bool HasAudioHardware(AudioObjectPropertySelector selector) {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
    selector,
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  size_t output_device_id_size = sizeof(output_device_id);
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
  return err == kAudioHardwareNoError &&
      output_device_id != kAudioObjectUnknown;
}
}  // namespace

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

AudioInputStream* AudioManagerMac::MakeAudioInputStream(
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample,
    uint32 samples_per_packet) {
  if (format == AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(channels, bits_per_sample,
                                                sample_rate,
                                                samples_per_packet);
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMQueueInAudioInputStream(this, channels, sample_rate,
                                          bits_per_sample, samples_per_packet);
  }
  return NULL;
}

AudioOutputStream* AudioManagerMac::MakeAudioOutputStream(
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample) {
  if (format == AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMQueueOutAudioOutputStream(this, channels, sample_rate,
                                            bits_per_sample);
  }
  return NULL;
}

void AudioManagerMac::MuteAll() {
  // TODO(cpu): implement.
}

void AudioManagerMac::UnMuteAll() {
  // TODO(cpu): implement.
}

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseOutputStream(
    PCMQueueOutAudioOutputStream* stream) {
  delete stream;
}

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseInputStream(PCMQueueInAudioInputStream* stream) {
  delete stream;
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerMac();
}
