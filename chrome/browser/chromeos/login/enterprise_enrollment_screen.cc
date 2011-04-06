// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"

#include "chrome/browser/chromeos/login/screen_observer.h"

namespace chromeos {

EnterpriseEnrollmentScreen::EnterpriseEnrollmentScreen(
    WizardScreenDelegate* delegate)
    : ViewScreen<EnterpriseEnrollmentView>(delegate) {}

EnterpriseEnrollmentScreen::~EnterpriseEnrollmentScreen() {}

void EnterpriseEnrollmentScreen::Authenticate(const std::string& user,
                                              const std::string& password,
                                              const std::string& captcha,
                                              const std::string& access_code) {
  // TODO(mnissler): Implement actual authentication stuff.

  if (view())
    view()->ShowConfirmationScreen();
}

void EnterpriseEnrollmentScreen::CancelEnrollment() {
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_CANCELLED);
}

void EnterpriseEnrollmentScreen::CloseConfirmation() {
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);
}

EnterpriseEnrollmentView* EnterpriseEnrollmentScreen::AllocateView() {
  return new EnterpriseEnrollmentView(this);
}

}  // namespace chromeos
