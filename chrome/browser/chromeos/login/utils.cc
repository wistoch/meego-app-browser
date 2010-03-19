// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/utils.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/pam_google_authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

namespace login_utils {

void CompleteLogin(const std::string& username,
                   std::vector<std::string> cookies) {
  LOG(INFO) << "LoginManagerView: OnLoginSuccess()";

  if (CrosLibrary::EnsureLoaded())
    LoginLibrary::Get()->StartSession(username, "");

  UserManager::Get()->UserLoggedIn(username);

  // Send notification of success
  AuthenticationNotificationDetails details(true);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_AUTHENTICATION,
      NotificationService::AllSources(),
      Details<AuthenticationNotificationDetails>(&details));

  // Now launch the initial browser window.
  BrowserInit browser_init;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  int return_code;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kInChromeAuth)) {
    ExternalCookieHandler::GetCookies(command_line, profile);
  } else {
    GURL url(ExternalCookieHandler::kGoogleAccountsUrl);
    net::CookieOptions options;
    options.set_include_httponly();
    profile->GetRequestContext()->GetCookieStore()->SetCookiesWithOptions(
        url, cookies, options);
  }
  browser_init.LaunchBrowser(command_line, profile, std::wstring(), true,
                             &return_code);
}

Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInChromeAuth))
    return new GoogleAuthenticator(consumer);
  return new PamGoogleAuthenticator(consumer);
}

}  // namespace login_utils

}  // namespace chromeos
