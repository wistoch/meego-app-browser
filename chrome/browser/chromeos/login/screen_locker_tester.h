// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_

#include "base/basictypes.h"

namespace views {
class Button;
class Textfield;
}  // namespace views

namespace chromeos {

class ScreenLocker;

namespace test {

// ScreenLockerTester provides access to the private state/function
// of ScreenLocker class. Used to implement unit tests.
class ScreenLockerTester {
 public:
  // Returns true if the screen lock is open.
  bool IsOpen();

  // Injects MockAuthenticate that uses given password .
  void InjectMockAuthenticator(const char* password);

  // Emulates entring a password.
  void EnterPassword(const char* password);

 private:
  friend class chromeos::ScreenLocker;

  ScreenLockerTester() {}

  views::Textfield* GetPasswordField();

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTester);
};

}  // namespace test

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_
