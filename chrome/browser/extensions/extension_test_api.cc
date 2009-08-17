// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/common/notification_service.h"

namespace extension_test_api_functions {
const char kPassFunction[] = "test.pass";
const char kFailFunction[] = "test.fail";
};  // namespace extension_test_api_functions

bool ExtensionTestPassFunction::RunImpl() {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_PASSED,
      Source<Profile>(dispatcher()->profile()),
      NotificationService::NoDetails());
  return true;
}

bool ExtensionTestFailFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&message));
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_FAILED,
      Source<Profile>(dispatcher()->profile()),
      Details<std::string>(&message));
  return true;
}
