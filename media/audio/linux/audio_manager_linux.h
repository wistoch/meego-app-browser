// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_

#include <map>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "media/audio/audio_output.h"

class AlsaPcmOutputStream;
class AlsaWrapper;

class AudioManagerLinux : public AudioManager {
 public:
  AudioManagerLinux();

  // Call before using a newly created AudioManagerLinux instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioDevices();
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample);
  virtual void MuteAll();
  virtual void UnMuteAll();

  virtual void ReleaseStream(AlsaPcmOutputStream* stream);

 protected:
  // Friend function for invoking the destructor at exit.
  friend void DestroyAudioManagerLinux(void*);
  virtual ~AudioManagerLinux();

 private:
  // Thread used to interact with AudioOutputStreams created by this
  // audio manger.
  base::Thread audio_thread_;
  scoped_ptr<AlsaWrapper> wrapper_;

  Lock lock_;
  std::map<AlsaPcmOutputStream*, scoped_refptr<AlsaPcmOutputStream> >
      active_streams_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerLinux);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
