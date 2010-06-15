// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/app/chrome_version_info.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#elif defined(OS_LINUX)
#include "base/process_util.h"
#include "chrome/installer/util/version.h"
#endif

// TODO(finnur): For the stable channel we want to check daily and notify
// the user if more than 2 weeks have passed since the upgrade happened
// (without a reboot). For the dev channel however, I want quicker feedback
// on how the feature works so I'm checking every hour and notifying the
// user immediately.

namespace {

// How often to check for an upgrade.
const int kCheckForUpgradeEveryMs = 60 * 60 * 1000;  // 1 hour.

// How long to wait before notifying the user about the upgrade.
const int kNotifyUserAfterMs = 0;

// The thread to run the upgrade detection code on. We use FILE for Linux
// because we don't want to block the UI thread while launching a background
// process and reading its output.
const ChromeThread::ID kDetectUpgradeTaskID =
#if defined(OS_LINUX)
    ChromeThread::FILE;
#else
    ChromeThread::UI;
#endif

// This task checks the currently running version of Chrome against the
// installed version. If the installed version is newer, it runs the passed
// callback task. Otherwise it just deletes the task.
class DetectUpgradeTask : public Task {
 public:
  explicit DetectUpgradeTask(Task* upgrade_detected_task)
      : upgrade_detected_task_(upgrade_detected_task) {
  }

  virtual ~DetectUpgradeTask() {
    if (upgrade_detected_task_) {
      // This has to get deleted on the same thread it was created.
      ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                             new DeleteTask<Task>(upgrade_detected_task_));
    }
  }

  virtual void Run() {
    DCHECK(ChromeThread::CurrentlyOn(kDetectUpgradeTaskID));

#if defined(OS_WIN) || defined(OS_LINUX)
    using installer::Version;

#if defined(OS_WIN)
    // Get the version of the currently *installed* instance of Chrome,
    // which might be newer than the *running* instance if we have been
    // upgraded in the background.
    scoped_ptr<Version> installed_version(InstallUtil::GetChromeVersion(false));
    if (!installed_version.get()) {
      // User level Chrome is not installed, check system level.
      installed_version.reset(InstallUtil::GetChromeVersion(true));
    }
#elif defined(OS_LINUX)
    CommandLine command_line(*CommandLine::ForCurrentProcess());
    command_line.AppendSwitch(switches::kProductVersion);
    std::string reply;
    if (!base::GetAppOutput(command_line, &reply)) {
      DLOG(ERROR) << "Failed to get current file version";
      return;
    }

    scoped_ptr<Version> installed_version(
        Version::GetVersionFromString(ASCIIToUTF16(reply)));
#endif

    // Get the version of the currently *running* instance of Chrome.
    scoped_ptr<FileVersionInfo> version(chrome_app::GetChromeVersionInfo());
    if (version.get() == NULL) {
      NOTREACHED() << "Failed to get current file version";
      return;
    }

    scoped_ptr<Version> running_version(
        Version::GetVersionFromString(WideToUTF16(version->file_version())));
    if (running_version.get() == NULL) {
      NOTREACHED() << "Failed to parse version info";
      return;
    }

    // |installed_version| may be NULL when the user downgrades on Linux (by
    // switching from dev to beta channel, for example). The user needs a
    // restart in this case as well. See http://crbug.com/46547
    if (!installed_version.get() ||
        installed_version->IsHigherThan(running_version.get())) {
      ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                             upgrade_detected_task_);
      upgrade_detected_task_ = NULL;
    }
#else  // !(defined(OS_WIN) || defined(OS_LINUX))
    DCHECK(kNotifyUserAfterMs >= 0);  // Avoid error: var defined but not used.
    NOTIMPLEMENTED();
#endif
  }

 private:
  Task* upgrade_detected_task_;
};

}  // namespace

// static
void UpgradeDetector::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
}

UpgradeDetector::UpgradeDetector()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      notify_upgrade_(false) {
  // Upgrade notifications work on Windows (only Google Chrome) and Linux
  // (chromium and Google Chrome).
#if (defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)) || defined(OS_LINUX)
  detect_upgrade_timer_.Start(
      base::TimeDelta::FromMilliseconds(kCheckForUpgradeEveryMs),
      this, &UpgradeDetector::CheckForUpgrade);
#endif
}

UpgradeDetector::~UpgradeDetector() {
}

void UpgradeDetector::CheckForUpgrade() {
  method_factory_.RevokeAll();
  Task* callback_task =
      method_factory_.NewRunnableMethod(&UpgradeDetector::UpgradeDetected);
  ChromeThread::PostTask(kDetectUpgradeTaskID, FROM_HERE,
                         new DetectUpgradeTask(callback_task));
}

void UpgradeDetector::UpgradeDetected() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Stop the recurring timer (that is checking for changes).
  detect_upgrade_timer_.Stop();

  NotificationService::current()->Notify(
      NotificationType::UPGRADE_DETECTED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());

  // Start the OneShot timer for notifying the user after a certain period.
  upgrade_notification_timer_.Start(
      base::TimeDelta::FromMilliseconds(kNotifyUserAfterMs),
      this, &UpgradeDetector::NotifyOnUpgrade);
}

void UpgradeDetector::NotifyOnUpgrade() {
  notify_upgrade_ = true;

  NotificationService::current()->Notify(
      NotificationType::UPGRADE_RECOMMENDED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());
}
