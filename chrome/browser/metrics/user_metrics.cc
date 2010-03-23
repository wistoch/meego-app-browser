// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

void UserMetrics::RecordAction(const UserMetricsAction& action,
                               Profile* profile) {
  Record(action.str_, profile);
}

void UserMetrics::RecordComputedAction(const std::string& action,
                                       Profile* profile) {
  Record(action.c_str(), profile);
}

void UserMetrics::Record(const char *action, Profile *profile) {
  NotificationService::current()->Notify(NotificationType::USER_ACTION,
                                         Source<Profile>(profile),
                                         Details<const char*>(&action));
}



