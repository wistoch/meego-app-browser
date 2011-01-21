// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Use this class to authenticate users with Gaia and access cookies sent
// by the Gaia servers. This class cannot be used on its own becaue it relies
// on a subclass to provide the virtual Post and GetBackoffDelaySeconds methods.
//
// Sample usage:
// class ActualGaiaAuthenticator : public gaia::GaiaAuthenticator {
//   Provides actual implementation of Post and GetBackoffDelaySeconds.
// };
// ActualGaiaAuthenticator gaia_auth("User-Agent", SERVICE_NAME, kGaiaUrl);
// if (gaia_auth.Authenticate("email", "passwd", SAVE_IN_MEMORY_ONLY,
//                            true)) { // Synchronous
//   // Do something with: gaia_auth.auth_token(), or gaia_auth.sid(),
//   // or gaia_auth.lsid()
// }
//
// Credentials can also be preserved for subsequent requests, though these are
// saved in plain-text in memory, and not very secure on client systems. The
// email address associated with the Gaia account can be read; the password is
// write-only.

// TODO(sanjeevr): This class has been moved here from the bookmarks sync code.
// While it is a generic class that handles GAIA authentication, there are some
// artifacts of the sync code which needs to be cleaned up.
#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "chrome/common/deprecated/event_sys.h"
#include "googleurl/src/gurl.h"

namespace gaia {

static const char kGaiaUrl[] =
    "https://www.google.com:443/accounts/ClientLogin";

// Error codes from Gaia. These will be set correctly for both Gaia V1
// (/ClientAuth) and V2 (/ClientLogin)
enum AuthenticationError {
  None                      = 0,
  BadAuthentication         = 1,
  NotVerified               = 2,
  TermsNotAgreed            = 3,
  Unknown                   = 4,
  AccountDeleted            = 5,
  AccountDisabled           = 6,
  CaptchaRequired           = 7,
  ServiceUnavailable        = 8,
  // Errors generated by this class not Gaia.
  CredentialsNotSet         = 9,
  ConnectionUnavailable     = 10
};

class GaiaAuthenticator;

struct GaiaAuthEvent {
  enum {
    GAIA_AUTH_FAILED,
    GAIA_AUTH_SUCCEEDED,
    GAIA_AUTHENTICATOR_DESTROYED
  }
  what_happened;
  AuthenticationError error;
  const GaiaAuthenticator* authenticator;

  // Lets us use GaiaAuthEvent as its own traits type in hookups.
  typedef GaiaAuthEvent EventType;
  static inline bool IsChannelShutdownEvent(const GaiaAuthEvent& event) {
    return event.what_happened == GAIA_AUTHENTICATOR_DESTROYED;
  }
};

// GaiaAuthenticator can be used to pass user credentials to Gaia and obtain
// cookies set by the Gaia servers.
class GaiaAuthenticator {
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticatorTest,
                           TestNewlineAtEndOfAuthTokenRemoved);
 public:

  // Since GaiaAuthenticator can be used for any service, or by any client, you
  // must include a user-agent and a service-id when creating one. The
  // user_agent is a short string used for simple log analysis. gaia_url is used
  // to choose the server to authenticate with (e.g.
  // http://www.google.com/accounts/ClientLogin).
  GaiaAuthenticator(const std::string& user_agent,
                    const std::string& service_id,
                    const std::string& gaia_url);

  virtual ~GaiaAuthenticator();

  // This object should only be invoked from the AuthWatcherThread message
  // loop, which is injected here.
  void set_message_loop(const MessageLoop* loop) {
    message_loop_ = loop;
  }

  // Pass credentials to authenticate with, or use saved credentials via an
  // overload. If authentication succeeds, you can retrieve the authentication
  // token via the respective accessors. Returns a boolean indicating whether
  // authentication succeeded or not.
  bool Authenticate(const std::string& user_name, const std::string& password,
                    const std::string& captcha_token,
                    const std::string& captcha_value);

  bool Authenticate(const std::string& user_name, const std::string& password);

  // Pass the LSID to authenticate with. If the authentication succeeds, you can
  // retrieve the authetication token via the respective accessors. Returns a
  // boolean indicating whether authentication succeeded or not.
  // Always returns a long lived token.
  bool AuthenticateWithLsid(const std::string& lsid);

  // Resets all stored cookies to their default values.
  void ResetCredentials();

  void SetUsernamePassword(const std::string& username,
                           const std::string& password);

  void SetUsername(const std::string& username);

  // Virtual for testing
  virtual void RenewAuthToken(const std::string& auth_token);
  void SetAuthToken(const std::string& auth_token);

  struct AuthResults {
    AuthResults();
    ~AuthResults();

    std::string email;
    std::string password;

    // Fields that store various cookies.
    std::string sid;
    std::string lsid;
    std::string auth_token;

    std::string primary_email;

    // Fields for items returned when authentication fails.
    std::string error_msg;
    enum AuthenticationError auth_error;
    std::string auth_error_url;
    std::string captcha_token;
    std::string captcha_url;
  };

 protected:

  struct AuthParams {
    AuthParams();
    ~AuthParams();

    GaiaAuthenticator* authenticator;
    uint32 request_id;
    std::string email;
    std::string password;
    std::string captcha_token;
    std::string captcha_value;
  };

  // mutex_ must be entered before calling this function.
  AuthParams MakeParams(const std::string& user_name,
                        const std::string& password,
                        const std::string& captcha_token,
                        const std::string& captcha_value);

  // The real Authenticate implementations.
  bool AuthenticateImpl(const AuthParams& params);
  bool AuthenticateImpl(const AuthParams& params, AuthResults* results);

  // virtual for testing purposes.
  virtual bool PerformGaiaRequest(const AuthParams& params,
                                  AuthResults* results);
  virtual bool Post(const GURL& url, const std::string& post_body,
                    unsigned long* response_code, std::string* response_body);

  // Caller should fill in results->LSID before calling. Result in
  // results->primary_email.
  virtual bool LookupEmail(AuthResults* results);

  // Subclasses must override to provide a backoff delay. It is virtual instead
  // of pure virtual for testing purposes.
  // TODO(sanjeevr): This should be made pure virtual. But this class is
  // currently directly being used in sync/engine/authenticator.cc, which is
  // wrong.
  virtual int GetBackoffDelaySeconds(int current_backoff_delay);

 public:
  // Retrieve email.
  inline std::string email() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.email;
  }

  // Retrieve password.
  inline std::string password() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.password;
  }

  // Retrieve AuthToken, if previously authenticated; otherwise returns "".
  inline std::string auth_token() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.auth_token;
  }

  // Retrieve SID cookie. For details, see the Google Accounts documentation.
  inline std::string sid() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.sid;
  }

  // Retrieve LSID cookie. For details, see the Google Accounts documentation.
  inline std::string lsid() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.lsid;
  }

  // Get last authentication error.
  inline enum AuthenticationError auth_error() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.auth_error;
  }

  inline std::string auth_error_url() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.auth_error_url;
  }

  inline std::string captcha_token() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.captcha_token;
  }

  inline std::string captcha_url() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_.captcha_url;
  }

  inline AuthResults results() const {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    return auth_results_;
  }

  typedef EventChannel<GaiaAuthEvent, base::Lock> Channel;

  inline Channel* channel() const {
    return channel_;
  }

 private:
  bool IssueAuthToken(AuthResults* results, const std::string& service_id);

  // Helper method to parse response when authentication succeeds.
  void ExtractTokensFrom(const std::string& response, AuthResults* results);
  // Helper method to parse response when authentication fails.
  void ExtractAuthErrorFrom(const std::string& response, AuthResults* results);

  // Fields for the obvious data items.
  const std::string user_agent_;
  const std::string service_id_;
  const std::string gaia_url_;

  AuthResults auth_results_;

  // When multiple async requests are running, only the one that started most
  // recently updates the values.
  //
  // Note that even though this code was written to handle multiple requests
  // simultaneously, the sync code issues auth requests one at a time.
  uint32 request_count_;

  Channel* channel_;

  // Used to compute backoff time for next allowed authentication.
  int delay_;  // In seconds.
  // On Windows, time_t is 64-bit by default. Even though we have defined the
  // _USE_32BIT_TIME_T preprocessor flag, other libraries including this header
  // may not have that preprocessor flag defined resulting in mismatched class
  // sizes. So we explicitly define it as 32-bit on Windows.
  // TODO(sanjeevr): Change this to to use base::Time
#if defined(OS_WIN)
  __time32_t next_allowed_auth_attempt_time_;
#else  // defined(OS_WIN)
  time_t next_allowed_auth_attempt_time_;
#endif  // defined(OS_WIN)
  int early_auth_attempt_count_;

  // The message loop all our methods are invoked on.
  const MessageLoop* message_loop_;
};

}  // namespace gaia
#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR_H_

