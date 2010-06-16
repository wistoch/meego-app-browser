// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env_var.h"
#include "base/basictypes.h"
#include "base/waitable_event.h"
#include "media/audio/audio_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

static const int kSampleRate = AudioManager::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const int kChannels = 2;
static const int kHardwareBufferSize = kSampleRate * kBitsPerSample *
                                       kChannels / 8;
static const int kBufferCapacity = 3 * kHardwareBufferSize;

namespace media {

class MockAudioControllerEventHandler : public AudioController::EventHandler {
 public:
  MockAudioControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioController* controller));
  MOCK_METHOD1(OnPlaying, void(AudioController* controller));
  MOCK_METHOD1(OnPaused, void(AudioController* controller));
  MOCK_METHOD2(OnError, void(AudioController* controller, int error_code));
  MOCK_METHOD3(OnMoreData,
               void(AudioController* controller,
                    base::Time timestamp, uint32 pending_bytes));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioControllerEventHandler);
};

class MockAudioControllerSyncReader : public AudioController::SyncReader {
 public:
  MockAudioControllerSyncReader() {}

  MOCK_METHOD1(UpdatePendingBytes, void(uint32 bytes));
  MOCK_METHOD2(Read, uint32(void* data, uint32 size));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioControllerSyncReader);
};

static bool HasAudioDevices() {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  CHECK(audio_man);
  return audio_man->HasAudioDevices();
}

static bool IsRunningHeadless() {
  scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
  if (env->HasEnv("CHROME_HEADLESS"))
    return true;
  return false;
}

ACTION_P3(SignalEvent, event, count, limit) {
  if (++*count >= limit) {
    event->Signal();
  }
}

TEST(AudioControllerTest, PlayAndClose) {
  if (!HasAudioDevices() || IsRunningHeadless())
    return;

  MockAudioControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  int count = 0;

  // If OnCreated is called then signal the event.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  // OnPlaying() will be called only once.
  EXPECT_CALL(event_handler, OnPlaying(NotNull()))
      .Times(Exactly(1));

  // If OnMoreData is called enough then signal the event.
  EXPECT_CALL(event_handler, OnMoreData(NotNull(), _, 0))
      .Times(AtLeast(10))
      .WillRepeatedly(SignalEvent(&event, &count, 10));

  scoped_refptr<AudioController> controller = AudioController::Create(
      &event_handler, AudioManager::AUDIO_PCM_LINEAR, kChannels,
      kSampleRate, kBitsPerSample, kHardwareBufferSize, kBufferCapacity);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();
  event.Reset();

  // Play and then wait for the event to be signaled.
  controller->Play();
  event.Wait();

  // Now stop the controller. This should shutdown the internal
  // thread and we hold the only reference to it.
  controller->Close();

  // TODO(hclam): Make sure releasing the reference to this
  // object actually destruct it.
  controller = NULL;
}

TEST(AudioControllerTest, PlayPauseClose) {
  if (!HasAudioDevices() || IsRunningHeadless())
    return;

  MockAudioControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  int count = 0;

  // If OnCreated is called then signal the event.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  // OnPlaying() will be called only once.
  EXPECT_CALL(event_handler, OnPlaying(NotNull()))
      .Times(Exactly(1));

  // If OnMoreData is called enough then signal the event.
  EXPECT_CALL(event_handler, OnMoreData(NotNull(), _, 0))
      .Times(AtLeast(10))
      .WillRepeatedly(SignalEvent(&event, &count, 10));

  // And then OnPaused() will be called.
  EXPECT_CALL(event_handler, OnPaused(NotNull()))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  scoped_refptr<AudioController> controller = AudioController::Create(
      &event_handler, AudioManager::AUDIO_PCM_LINEAR, kChannels,
      kSampleRate, kBitsPerSample, kHardwareBufferSize, kBufferCapacity);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();
  event.Reset();

  // Play and then wait for the event to be signaled.
  controller->Play();
  event.Wait();
  event.Reset();

  // And then wait for pause to complete.
  controller->Pause();
  event.Wait();

  // Now stop the controller. This should shutdown the internal
  // thread and we hold the only reference to it.
  controller->Close();

  // TODO(hclam): Make sure releasing the reference to this
  // object actually destruct it.
  controller = NULL;
}

TEST(AudioControllerTest, HardwareBufferTooLarge) {
  if (!HasAudioDevices() || IsRunningHeadless())
    return;

  // Create an audio device with a very large hardware buffer size.
  MockAudioControllerEventHandler event_handler;
  scoped_refptr<AudioController> controller = AudioController::Create(
      &event_handler, AudioManager::AUDIO_PCM_LINEAR, kChannels,
      kSampleRate, kBitsPerSample, kHardwareBufferSize * 1000,
      kBufferCapacity);

  // Use assert because we don't stop the device and assume we can't
  // create one.
  ASSERT_FALSE(controller);
}

}  // namespace media
