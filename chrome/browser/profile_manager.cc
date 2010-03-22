// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profile_manager.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#endif

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->ShutdownSessionService();
}

// static
Profile* ProfileManager::GetDefaultProfile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

ProfileManager::ProfileManager() : logged_in_(false) {
  SystemMonitor::Get()->AddObserver(this);
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_CHANGED,
      NotificationService::AllSources());
#endif
}

ProfileManager::~ProfileManager() {
  SystemMonitor* system_monitor = SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveObserver(this);

  // Destroy all profiles that we're keeping track of.
  for (const_iterator i(begin()); i != end(); ++i)
    delete *i;
  profiles_.clear();

  // Get rid of available profile list
  for (AvailableProfileVector::const_iterator i(available_profiles_.begin());
       i != available_profiles_.end(); ++i)
    delete *i;
  available_profiles_.clear();
}

FilePath ProfileManager::GetDefaultProfileDir(
    const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(
      FilePath::FromWStringHack(chrome::kNotSignedInProfile));
  return default_profile_dir;
}

FilePath ProfileManager::GetProfilePrefsPath(
    const FilePath &profile_dir) {
  FilePath default_prefs_path(profile_dir);
  default_prefs_path = default_prefs_path.Append(chrome::kPreferencesFilename);
  return default_prefs_path;
}

Profile* ProfileManager::GetDefaultProfile(const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (logged_in_) {
    std::wstring profile_dir;
    // If the user has logged in, pick up the new profile.
    // TODO(davemoore) Delete this once chromium os has started using
    // "--login-profile" instead of "--profile".
    if (command_line.HasSwitch(switches::kLoginProfile)) {
      profile_dir = command_line.GetSwitchValue(switches::kLoginProfile);
    } else if (command_line.HasSwitch(switches::kProfile)) {
      profile_dir = command_line.GetSwitchValue(switches::kProfile);
    } else {
      // We should never be logged in with no profile dir.
      NOTREACHED();
      return NULL;
    }
    default_profile_dir = default_profile_dir.Append(
        FilePath::FromWStringHack(profile_dir));
    return GetProfile(default_profile_dir);
  } else {
    // If not logged in on cros, always return the incognito profile
    default_profile_dir = default_profile_dir.Append(
        FilePath::FromWStringHack(chrome::kNotSignedInProfile));
    Profile*profile = GetProfile(default_profile_dir);

    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail.
    // TODO(davemoore) Fix the tests so they allow OTR profiles.
    if (!command_line.HasSwitch(switches::kTestType))
      profile = profile->GetOffTheRecordProfile();

    return profile;
  }
#else
  default_profile_dir = default_profile_dir.Append(
      FilePath::FromWStringHack(chrome::kNotSignedInProfile));
  return GetProfile(default_profile_dir);
#endif
}

Profile* ProfileManager::GetProfile(const FilePath& profile_dir) {
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (NULL != profile)
    return profile;

  if (!ProfileManager::IsProfile(profile_dir)) {
    // If the profile directory doesn't exist, create it.
    profile = ProfileManager::CreateProfile(profile_dir);
    if (!profile)
      return NULL;
    bool result = AddProfile(profile);
    DCHECK(result);
  } else {
    // The profile already exists on disk, just load it.
    profile = AddProfileByPath(profile_dir);
    if (!profile)
      return NULL;
  }
  DCHECK(profile);
  return profile;
}

Profile* ProfileManager::AddProfileByPath(const FilePath& path) {
  Profile* profile = GetProfileByPath(path);
  if (profile)
    return profile;

  profile = Profile::CreateProfile(path);
  if (AddProfile(profile)) {
    return profile;
  } else {
    return NULL;
  }
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  profiles_.insert(profiles_.end(), profile);
  profile->InitExtensions();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitWebResources();
  return true;
}

Profile* ProfileManager::GetProfileByPath(const FilePath& path) const {
  for (const_iterator i(begin()); i != end(); ++i) {
    if ((*i)->GetPath() == path)
      return *i;
  }

  return NULL;
}

void ProfileManager::OnSuspend() {
  DCHECK(CalledOnValidThread());

  for (const_iterator i(begin()); i != end(); ++i) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableFunction(&ProfileManager::SuspendProfile, *i));
  }
}

void ProfileManager::OnResume() {
  DCHECK(CalledOnValidThread());
  for (const_iterator i(begin()); i != end(); ++i) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableFunction(&ProfileManager::ResumeProfile, *i));
  }
}

void ProfileManager::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == NotificationType::LOGIN_USER_CHANGED) {
    CHECK(chromeos::CrosLibrary::EnsureLoaded());
    // If we don't have a mounted profile directory we're in trouble.
    // TODO(davemoore) Once we have better api this check should ensure that
    // our profile directory is the one that's mounted, and that it's mounted
    // as the current user.
    CHECK(chromeos::CryptohomeLibrary::Get()->IsMounted());
    logged_in_ = true;
  }
#endif
}

void ProfileManager::SuspendProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  for (URLRequestJobTracker::JobIterator i = g_url_request_job_tracker.begin();
       i != g_url_request_job_tracker.end(); ++i)
    (*i)->Kill();

  profile->GetRequestContext()->GetURLRequestContext()->
      http_transaction_factory()->Suspend(true);
}

void ProfileManager::ResumeProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  profile->GetRequestContext()->GetURLRequestContext()->
      http_transaction_factory()->Suspend(false);
}

// static
bool ProfileManager::IsProfile(const FilePath& path) {
  FilePath prefs_path = GetProfilePrefsPath(path);
  FilePath history_path = path;
  history_path = history_path.Append(chrome::kHistoryFilename);

  return file_util::PathExists(prefs_path) &&
      file_util::PathExists(history_path);
}

// static
Profile* ProfileManager::CreateProfile(const FilePath& path) {
  if (IsProfile(path)) {
    DCHECK(false) << "Attempted to create a profile with the path:\n"
        << path.value() << "\n but that path already contains a profile";
  }

  if (!file_util::PathExists(path)) {
    // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
    // profile directory.  We should eventually be able to run in this
    // situation.
    if (!file_util::CreateDirectory(path))
      return NULL;
  }

  return Profile::CreateProfile(path);
}
