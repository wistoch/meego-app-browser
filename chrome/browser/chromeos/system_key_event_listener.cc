// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"

namespace chromeos {

namespace {

const double kStepPercentage = 4.0;

}  // namespace

// static
SystemKeyEventListener* SystemKeyEventListener::instance() {
  return Singleton<SystemKeyEventListener>::get();
}

SystemKeyEventListener::SystemKeyEventListener()
    : audio_handler_(AudioHandler::instance()) {
  WmMessageListener::instance()->AddObserver(this);
}

SystemKeyEventListener::~SystemKeyEventListener() {
  WmMessageListener::instance()->RemoveObserver(this);
}

void SystemKeyEventListener::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED)
    return;

  switch (message.param(0)) {
    case WM_IPC_SYSTEM_KEY_VOLUME_MUTE:
      audio_handler_->SetMute(true);
      VolumeBubble::instance()->ShowVolumeBubble(0);
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_DOWN:
      audio_handler_->AdjustVolumeByPercent(-kStepPercentage);
      audio_handler_->SetMute(false);
      VolumeBubble::instance()->ShowVolumeBubble(
          audio_handler_->GetVolumePercent());
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_UP:
      audio_handler_->AdjustVolumeByPercent(kStepPercentage);
      audio_handler_->SetMute(false);
      VolumeBubble::instance()->ShowVolumeBubble(
          audio_handler_->GetVolumePercent());
      break;
    default:
      DLOG(ERROR) << "SystemKeyEventListener: Unexpected message "
                  << message.param(0)
                  << " received";
  }
}

}  // namespace chromeos
