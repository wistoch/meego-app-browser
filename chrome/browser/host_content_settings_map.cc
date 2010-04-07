// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/scoped_pref_update.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/dns_util.h"
#include "net/base/static_cookie_policy.h"

// static
const wchar_t*
    HostContentSettingsMap::kTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  L"cookies",
  L"images",
  L"javascript",
  L"plugins",
  L"popups",
  NULL,  // Not used for Geolocation
};

// static
const ContentSetting
    HostContentSettingsMap::kDefaultSettings[CONTENT_SETTINGS_NUM_TYPES] = {
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,  // CONTENT_SETTINGS_TYPE_POPUPS
  CONTENT_SETTING_ASK,    // Not used for Geolocation
};

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      block_third_party_cookies_(false) {
  PrefService* prefs = profile_->GetPrefs();

  // Migrate obsolete cookie pref.
  if (prefs->HasPrefPath(prefs::kCookieBehavior)) {
    int cookie_behavior = prefs->GetInteger(prefs::kCookieBehavior);
    prefs->ClearPref(prefs::kCookieBehavior);
    if (!prefs->HasPrefPath(prefs::kDefaultContentSettings)) {
        SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
            (cookie_behavior == net::StaticCookiePolicy::BLOCK_ALL_COOKIES) ?
                CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
    }
    if (!prefs->HasPrefPath(prefs::kBlockThirdPartyCookies)) {
      SetBlockThirdPartyCookies(cookie_behavior ==
          net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    }
  }

  // Migrate obsolete popups pref.
  if (prefs->HasPrefPath(prefs::kPopupWhitelistedHosts)) {
    const ListValue* whitelist_pref =
        prefs->GetList(prefs::kPopupWhitelistedHosts);
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      SetContentSetting(host, CONTENT_SETTINGS_TYPE_POPUPS,
                        CONTENT_SETTING_ALLOW);
    }
    prefs->ClearPref(prefs::kPopupWhitelistedHosts);
  }

  // Read global defaults and host-speficic exceptions from preferences.
  ReadDefaultSettings(false);
  ReadPerHostSettings(false);

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies);

  prefs->AddPrefObserver(prefs::kDefaultContentSettings, this);
  prefs->AddPrefObserver(prefs::kPerHostContentSettings, this);
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies, false);
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Obsolete prefs, for migration:
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::StaticCookiePolicy::ALLOW_ALL_COOKIES);
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  AutoLock auto_lock(lock_);
  return default_content_settings_.settings[content_type];
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const std::string& host,
    ContentSettingsType content_type) const {
  AutoLock auto_lock(lock_);
  HostContentSettings::const_iterator i(host_content_settings_.find(
    net::TrimEndingDot(host)));
  if (i != host_content_settings_.end()) {
    ContentSetting setting = i->second.settings[content_type];
    if (setting != CONTENT_SETTING_DEFAULT)
      return setting;
  }
  return default_content_settings_.settings[content_type];
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& url,
    ContentSettingsType content_type) const {
  return ShouldAllowAllContent(url) ?
      CONTENT_SETTING_ALLOW : GetContentSetting(url.host(), content_type);
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const std::string& host) const {
  AutoLock auto_lock(lock_);
  HostContentSettings::const_iterator i(host_content_settings_.find(
    net::TrimEndingDot(host)));
  if (i == host_content_settings_.end())
    return default_content_settings_;

  ContentSettings output = i->second;
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    if (output.settings[j] == CONTENT_SETTING_DEFAULT)
      output.settings[j] = default_content_settings_.settings[j];
  }
  return output;
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const GURL& url) const {
  return ShouldAllowAllContent(url) ?
      ContentSettings(CONTENT_SETTING_ALLOW) : GetContentSettings(url.host());
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    SettingsForOneType* settings) const {
  DCHECK(settings);
  settings->clear();

  AutoLock auto_lock(lock_);
  for (HostContentSettings::const_iterator i(host_content_settings_.begin());
       i != host_content_settings_.end(); ++i) {
    ContentSetting setting = i->second.settings[content_type];
    if (setting != CONTENT_SETTING_DEFAULT) {
      // Use of push_back() relies on the map iterator traversing in order of
      // ascending keys.
      settings->push_back(std::make_pair(i->first, setting));
    }
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* prefs = profile_->GetPrefs();

  DictionaryValue* default_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kDefaultContentSettings);
  std::wstring dictionary_path(kTypeNames[content_type]);

  updating_settings_ = true;
  {
    AutoLock auto_lock(lock_);
    ScopedPrefUpdate update_settings(prefs, prefs::kDefaultContentSettings);
    if ((setting == CONTENT_SETTING_DEFAULT) ||
        (setting == kDefaultSettings[content_type])) {
      default_content_settings_.settings[content_type] =
          kDefaultSettings[content_type];
      default_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                              NULL);
    } else {
      default_content_settings_.settings[content_type] = setting;
      default_settings_dictionary->SetWithoutPathExpansion(
          dictionary_path, Value::CreateIntegerValue(setting));
    }
  }
  updating_settings_ = false;

  NotifyObservers(std::string());
}

void HostContentSettingsMap::SetContentSetting(const std::string& host,
                                               ContentSettingsType content_type,
                                               ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* prefs = profile_->GetPrefs();

  bool early_exit = false;
  std::wstring wide_host(UTF8ToWide(host));
  DictionaryValue* all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kPerHostContentSettings);

  updating_settings_ = true;
  {
    ScopedPrefUpdate update_settings(prefs, prefs::kPerHostContentSettings);
    {
      AutoLock auto_lock(lock_);
      if (!host_content_settings_.count(host))
        host_content_settings_[host] = ContentSettings();
      HostContentSettings::iterator i(host_content_settings_.find(host));
      ContentSettings& settings = i->second;
      settings.settings[content_type] = setting;
      if (AllDefault(settings)) {
        host_content_settings_.erase(i);
        all_settings_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);

        // We can't just return because |NotifyObservers()| needs to be called,
        // without |lock_| being held.
        early_exit = true;
      }
    }

    if (!early_exit) {
      DictionaryValue* host_settings_dictionary;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          wide_host, &host_settings_dictionary);
      if (!found) {
        host_settings_dictionary = new DictionaryValue;
        all_settings_dictionary->SetWithoutPathExpansion(
            wide_host, host_settings_dictionary);
        DCHECK_NE(setting, CONTENT_SETTING_DEFAULT);
      }
      std::wstring dictionary_path(kTypeNames[content_type]);
      if (setting == CONTENT_SETTING_DEFAULT) {
        host_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                             NULL);
      } else {
        host_settings_dictionary->SetWithoutPathExpansion(
            dictionary_path, Value::CreateIntegerValue(setting));
      }
    }
  }
  updating_settings_ = false;

  NotifyObservers(host);
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  PrefService* prefs = profile_->GetPrefs();

  updating_settings_ = true;
  {
    AutoLock auto_lock(lock_);
    for (HostContentSettings::iterator i(host_content_settings_.begin());
         i != host_content_settings_.end(); ) {
      if (i->second.settings[content_type] != CONTENT_SETTING_DEFAULT) {
        i->second.settings[content_type] = CONTENT_SETTING_DEFAULT;
        std::wstring wide_host(UTF8ToWide(i->first));
        DictionaryValue* all_settings_dictionary =
            prefs->GetMutableDictionary(prefs::kPerHostContentSettings);
        ScopedPrefUpdate update_settings(prefs, prefs::kPerHostContentSettings);
        if (AllDefault(i->second)) {
          all_settings_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
          host_content_settings_.erase(i++);
        } else {
          DictionaryValue* host_settings_dictionary;
          bool found =
              all_settings_dictionary->GetDictionaryWithoutPathExpansion(
                  wide_host, &host_settings_dictionary);
          DCHECK(found);
          host_settings_dictionary->RemoveWithoutPathExpansion(
              kTypeNames[content_type], NULL);
          ++i;
        }
      } else {
        ++i;
      }
    }
  }
  updating_settings_ = true;

  NotifyObservers(std::string());
}

void HostContentSettingsMap::SetBlockThirdPartyCookies(bool block) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    block_third_party_cookies_ = block;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (block)
    prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  else
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
}

void HostContentSettingsMap::ResetToDefaults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_settings_ = ContentSettings();
    ForceDefaultsToBeExplicit();
    host_content_settings_.clear();
    block_third_party_cookies_ = false;
  }

  PrefService* prefs = profile_->GetPrefs();
  updating_settings_ = true;
  prefs->ClearPref(prefs::kDefaultContentSettings);
  prefs->ClearPref(prefs::kPerHostContentSettings);
  prefs->ClearPref(prefs::kBlockThirdPartyCookies);
  updating_settings_ = false;

  NotifyObservers(std::string());
}

void HostContentSettingsMap::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(NotificationType::PREF_CHANGED == type);
  DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
  if (updating_settings_)
    return;

  std::wstring* name = Details<std::wstring>(details).ptr();
  if (prefs::kDefaultContentSettings == *name) {
    ReadDefaultSettings(true);
    NotifyObservers(std::string());
  } else if (prefs::kPerHostContentSettings == *name) {
    ReadPerHostSettings(true);
    NotifyObservers(std::string());
  } else {
    NOTREACHED() << "Unexpected preference observed.";
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
}

void HostContentSettingsMap::ReadDefaultSettings(bool overwrite) {
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  const DictionaryValue* default_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kDefaultContentSettings);
  AutoLock auto_lock(lock_);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary != NULL) {
    if (overwrite) default_content_settings_ = ContentSettings();
    GetSettingsFromDictionary(default_settings_dictionary,
                              &default_content_settings_);
  }
  ForceDefaultsToBeExplicit();
}

void HostContentSettingsMap::ReadPerHostSettings(bool overwrite) {
  const DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kPerHostContentSettings);
  AutoLock auto_lock(lock_);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    if (overwrite) host_content_settings_.clear();
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      std::wstring wide_host(*i);
      DictionaryValue* host_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          wide_host, &host_settings_dictionary);
      DCHECK(found);
      ContentSettings settings;
      GetSettingsFromDictionary(host_settings_dictionary, &settings);
      host_content_settings_[WideToUTF8(wide_host)] = settings;
    }
  }
}

// static
bool HostContentSettingsMap::ShouldAllowAllContent(const GURL& url) {
  return url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme) ||
         url.SchemeIs(chrome::kGearsScheme) ||
         url.SchemeIs(chrome::kUserScriptScheme);
}

void HostContentSettingsMap::GetSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    std::wstring content_type(*i);
    int setting = CONTENT_SETTING_DEFAULT;
    bool found = dictionary->GetIntegerWithoutPathExpansion(content_type,
                                                            &setting);
    DCHECK(found);
    for (size_t type = 0; type < arraysize(kTypeNames); ++type) {
      if ((kTypeNames[type] != NULL) &&
          (std::wstring(kTypeNames[type]) == content_type)) {
        settings->settings[type] = IntToContentSetting(setting);
        break;
      }
    }
  }
}

void HostContentSettingsMap::ForceDefaultsToBeExplicit() {
  DCHECK_EQ(arraysize(kDefaultSettings),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (default_content_settings_.settings[i] == CONTENT_SETTING_DEFAULT)
      default_content_settings_.settings[i] = kDefaultSettings[i];
  }
}

bool HostContentSettingsMap::AllDefault(const ContentSettings& settings) const {
  for (size_t i = 0; i < arraysize(settings.settings); ++i) {
    if (settings.settings[i] != CONTENT_SETTING_DEFAULT)
      return false;
  }
  return true;
}

void HostContentSettingsMap::NotifyObservers(const std::string& host) {
  ContentSettingsDetails details(host);
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(this),
      Details<ContentSettingsDetails>(&details));
}
