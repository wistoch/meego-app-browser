// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_

#include <vector>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"

class Profile;

namespace chromeos {

// An interface for objects that will authenticate a Chromium OS user.
// When authentication successfully completes, will call
// consumer_->OnLoginSuccess(|username|) on the UI thread.
// On failure, will call consumer_->OnLoginFailure() on the UI thread.
class Authenticator : public base::RefCountedThreadSafe<Authenticator> {
 public:
  explicit Authenticator(LoginStatusConsumer* consumer)
      : consumer_(consumer) {
  }
  virtual ~Authenticator() {}

  // Given a |username| and |password|, this method attempts to authenticate
  // Returns true if we kick off the attempt successfully and false if we can't.
  // Must be called on the FILE thread.
  virtual bool Authenticate(Profile* profile,
                            const std::string& username,
                            const std::string& password) = 0;

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  virtual void OnLoginSuccess(const std::string& credentials) = 0;
  virtual void OnLoginFailure(const std::string& data) = 0;

 protected:
  LoginStatusConsumer* consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

class StubAuthenticator : public Authenticator {
 public:
  explicit StubAuthenticator(LoginStatusConsumer* consumer)
      : Authenticator(consumer) {
  }
  virtual ~StubAuthenticator() {}

  // Returns true after calling OnLoginSuccess().
  virtual bool Authenticate(Profile* profile,
                            const std::string& username,
                            const std::string& password) {
    username_ = username;
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &StubAuthenticator::OnLoginSuccess,
                          std::string()));
    return true;
  }

  void OnLoginSuccess(const std::string& credentials) {
    consumer_->OnLoginSuccess(username_, credentials);
  }

  void OnLoginFailure(const std::string& data) {}

 private:
  std::string username_;
  DISALLOW_COPY_AND_ASSIGN(StubAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
