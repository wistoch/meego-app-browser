// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <set>

#include "chrome/browser/profile_manager.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#include "generated_resources.h"

// static
void ProfileManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kProfileName, L"");
  prefs->RegisterStringPref(prefs::kProfileNickname, L"");
  prefs->RegisterStringPref(prefs::kProfileID, L"");
}

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->ShutdownSessionService();
}

ProfileManager::ProfileManager() {
}

ProfileManager::~ProfileManager() {
  // Destroy all profiles that we're keeping track of.
  for (ProfileVector::const_iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    delete *iter;
  }
  profiles_.clear();

  // Get rid of available profile list
  for (AvailableProfileVector::const_iterator iter =
           available_profiles_.begin();
       iter != available_profiles_.end(); ++iter) {
    delete *iter;
  }
  available_profiles_.clear();
}

Profile* ProfileManager::GetDefaultProfile(const std::wstring& user_data_dir) {
  // Initialize profile, creating default if necessary
  std::wstring default_profile_path = user_data_dir;
  file_util::AppendToPath(&default_profile_path,
                          chrome::kNotSignedInProfile);

  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(default_profile_path);
  if (NULL != profile)
    return profile;

  if (!ProfileManager::IsProfile(default_profile_path)) {
    // If the profile directory doesn't exist, create it.
    profile = ProfileManager::CreateProfile(default_profile_path,
        L"",  // No name.
        L"",  // No nickname.
        chrome::kNotSignedInID);
    if (!profile)
      return NULL;
    bool result = AddProfile(profile);
    DCHECK(result);
  } else {
    // The profile already exists on disk, just load it.
    profile = AddProfileByPath(default_profile_path);
    if (!profile)
      return NULL;

    if (profile->GetID() != chrome::kNotSignedInID) {
      // Something must've gone wrong with the profile section of the
      // Preferences file, fix it.
      profile->SetID(chrome::kNotSignedInID);
      profile->SetName(chrome::kNotSignedInProfile);
    }
  }
  DCHECK(profile);
  return profile;
}

Profile* ProfileManager::AddProfileByPath(const std::wstring& path) {
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

void ProfileManager::NewWindowWithProfile(Profile* profile) {
  DCHECK(profile);

  Browser* new_browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile,
                                     BrowserType::TABBED_BROWSER, L"");
  new_browser->AddTabWithURL(GURL(), PageTransition::TYPED, true, NULL);
  new_browser->Show();
}

Profile* ProfileManager::AddProfileByID(const std::wstring& id) {
  AvailableProfile* available = GetAvailableProfileByID(id);
  if (!available)
    return NULL;

  std::wstring path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  file_util::AppendToPath(&path, available->directory());

  return AddProfileByPath(path);
}

AvailableProfile* ProfileManager::GetAvailableProfileByID(
    const std::wstring& id) {
  AvailableProfileVector::const_iterator iter = available_profiles_.begin();
  for (; iter != available_profiles_.end(); ++iter) {
    if ((*iter)->id() == id) {
      return *iter;
    }
  }

  return NULL;
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath() << ") as an already-loaded profile.";
    return false;
  }
  if (GetProfileByID(profile->GetID())) {
    NOTREACHED() << "Attempted to add profile with the same ID (" <<
                    profile->GetID() << ") as an already-loaded profile.";
    return false;
  }

  profiles_.insert(profiles_.end(), profile);
  return true;
}

void ProfileManager::RemoveProfile(Profile* profile) {
  for (ProfileVector::iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter) == profile) {
      profiles_.erase(iter);
      return;
    }
  }
}

void ProfileManager::RemoveProfileByPath(const std::wstring& path) {
  for (ProfileVector::iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter)->GetPath() == path) {
      delete *iter;
      profiles_.erase(iter);
      return;
    }
  }

  NOTREACHED() << "Attempted to remove non-loaded profile: " << path;
}

Profile* ProfileManager::GetProfileByPath(const std::wstring& path) const {
  for (ProfileVector::const_iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter)->GetPath() == path)
      return *iter;
  }

  return NULL;
}

Profile* ProfileManager::GetProfileByID(const std::wstring& id) const {
  for (ProfileVector::const_iterator iter = profiles_.begin();
    iter != profiles_.end(); ++iter) {
      if ((*iter)->GetID() == id)
        return *iter;
  }

  return NULL;
}


// static
bool ProfileManager::IsProfile(const std::wstring& path) {
  std::wstring prefs_path = path;
  file_util::AppendToPath(&prefs_path, chrome::kPreferencesFilename);

  std::wstring history_path = path;
  file_util::AppendToPath(&history_path, chrome::kHistoryFilename);

  return file_util::PathExists(prefs_path) &&
         file_util::PathExists(history_path);
}

// static
bool ProfileManager::CopyProfileData(const std::wstring& source_path,
                                     const std::wstring& destination_path) {
  // create destination directory if necessary
  if (!file_util::PathExists(destination_path)) {
    bool result = !!CreateDirectory(destination_path.c_str(), NULL);
    if (!result) {
      DLOG(WARNING) << "Unable to create destination directory " <<
                       destination_path;
      return false;
    }
  }

  // copy files in directory
  WIN32_FIND_DATA find_file_data;
  std::wstring filename_spec = source_path;
  file_util::AppendToPath(&filename_spec, L"*");
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      // skip directories
      if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        continue;

      std::wstring source_file = source_path;
      file_util::AppendToPath(&source_file, find_file_data.cFileName);
      std::wstring dest_file = destination_path;
      file_util::AppendToPath(&dest_file, find_file_data.cFileName);
      bool result = !!CopyFileW(source_file.c_str(),
                                dest_file.c_str(),
                                FALSE /* overwrite */);
      if (!result)
        return false;
    } while (FindNextFile(find_handle,  &find_file_data));
    FindClose(find_handle);
  }

  return true;
}

// static
Profile* ProfileManager::CreateProfile(const std::wstring& path,
                                       const std::wstring& name,
                                       const std::wstring& nickname,
                                       const std::wstring& id) {
  DCHECK_LE(nickname.length(), name.length());

  if (IsProfile(path)) {
    DCHECK(false) << "Attempted to create a profile with the path:\n" << path
        << "\n but that path already contains a profile";
  }

  if (!file_util::PathExists(path)) {
    // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
    // profile directory.  We should eventually be able to run in this
    // situation.
    if (!file_util::CreateDirectory(path))
      return NULL;
  }

  Profile* profile = Profile::CreateProfile(path);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kProfileName, name);
  prefs->SetString(prefs::kProfileNickname, nickname);
  prefs->SetString(prefs::kProfileID, id);

  return profile;
}

// static
std::wstring ProfileManager::CanonicalizeID(const std::wstring& id) {
  std::wstring no_whitespace;
  TrimWhitespace(id, TRIM_ALL, &no_whitespace);
  return StringToLowerASCII(no_whitespace);
}

