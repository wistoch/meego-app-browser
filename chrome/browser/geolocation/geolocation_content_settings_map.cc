// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"

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
const ContentSetting
    GeolocationContentSettingsMap::kDefaultSetting = CONTENT_SETTING_ASK;

GeolocationContentSettingsMap::GeolocationContentSettingsMap(Profile* profile)
    : profile_(profile), updating_preferences_(false) {
  PrefService* prefs = profile_->GetPrefs();

  // Read global defaults.
  default_content_setting_ = IntToContentSetting(
      prefs->GetInteger(prefs::kGeolocationDefaultContentSetting));
  if (default_content_setting_ == CONTENT_SETTING_DEFAULT)
    default_content_setting_ = kDefaultSetting;

  // Read exceptions from the preference service.
  ReadExceptions();

  prefs->AddPrefObserver(prefs::kGeolocationContentSettings, this);
}

// static
void GeolocationContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeolocationDefaultContentSetting,
                             CONTENT_SETTING_ASK);
  prefs->RegisterDictionaryPref(prefs::kGeolocationContentSettings);
}

// static
std::string GeolocationContentSettingsMap::OriginToString(const GURL& origin) {
  std::string port_component((origin.IntPort() != url_parse::PORT_UNSPECIFIED) ?
      ":" + origin.port() : "");
  std::string scheme_component(!origin.SchemeIs(chrome::kHttpScheme) ?
      origin.scheme() + chrome::kStandardSchemeSeparator : "");
  return scheme_component + origin.host() + port_component;
}

ContentSetting GeolocationContentSettingsMap::GetDefaultContentSetting() const {
  AutoLock auto_lock(lock_);
  return default_content_setting_;
}

ContentSetting GeolocationContentSettingsMap::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url) const {
  DCHECK(requesting_url.is_valid() && embedding_url.is_valid());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() && embedding_origin.is_valid());
  AutoLock auto_lock(lock_);
  AllOriginsSettings::const_iterator i(content_settings_.find(
      requesting_origin));
  if (i != content_settings_.end()) {
    OneOriginSettings::const_iterator j(i->second.find(embedding_origin));
    if (j != i->second.end())
      return j->second;
    if (requesting_origin != embedding_origin) {
      OneOriginSettings::const_iterator any_embedder(i->second.find(GURL()));
      if (any_embedder != i->second.end())
        return any_embedder->second;
    }
  }
  return default_content_setting_;
}

GeolocationContentSettingsMap::AllOriginsSettings
    GeolocationContentSettingsMap::GetAllOriginsSettings() const {
  AutoLock auto_lock(lock_);
  return content_settings_;
}

void GeolocationContentSettingsMap::SetDefaultContentSetting(
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  {
    AutoLock auto_lock(lock_);
    default_content_setting_ =
        (setting == CONTENT_SETTING_DEFAULT) ? kDefaultSetting : setting;
  }
  profile_->GetPrefs()->SetInteger(prefs::kGeolocationDefaultContentSetting,
                                   default_content_setting_);
}

void GeolocationContentSettingsMap::SetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(requesting_url.is_valid());
  DCHECK(embedding_url.is_valid() || embedding_url.is_empty());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() &&
         (embedding_origin.is_valid() || embedding_url.is_empty()));
  std::wstring wide_requesting_origin(UTF8ToWide(requesting_origin.spec()));
  std::wstring wide_embedding_origin(UTF8ToWide(embedding_origin.spec()));
  DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kGeolocationContentSettings);

  updating_preferences_ = true;
  {
    ScopedPrefUpdate update(profile_->GetPrefs(),
                            prefs::kGeolocationContentSettings);
    AutoLock auto_lock(lock_);
    DictionaryValue* requesting_origin_settings_dictionary;
    all_settings_dictionary->GetDictionaryWithoutPathExpansion(
        wide_requesting_origin, &requesting_origin_settings_dictionary);
    if (setting == CONTENT_SETTING_DEFAULT) {
      if (content_settings_.count(requesting_origin) &&
          content_settings_[requesting_origin].count(embedding_origin)) {
        if (content_settings_[requesting_origin].size() == 1) {
          all_settings_dictionary->RemoveWithoutPathExpansion(
              wide_requesting_origin, NULL);
          content_settings_.erase(requesting_origin);
        } else {
          requesting_origin_settings_dictionary->RemoveWithoutPathExpansion(
              wide_embedding_origin, NULL);
          content_settings_[requesting_origin].erase(embedding_origin);
        }
      }
    } else {
      if (!content_settings_.count(requesting_origin)) {
        requesting_origin_settings_dictionary = new DictionaryValue;
        all_settings_dictionary->SetWithoutPathExpansion(
            wide_requesting_origin, requesting_origin_settings_dictionary);
      }
      content_settings_[requesting_origin][embedding_origin] = setting;
      DCHECK(requesting_origin_settings_dictionary);
      requesting_origin_settings_dictionary->SetWithoutPathExpansion(
          wide_embedding_origin, Value::CreateIntegerValue(setting));
    }
  }
  updating_preferences_ = false;
}

void GeolocationContentSettingsMap::ClearOneRequestingOrigin(
    const GURL& requesting_origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(requesting_origin.is_valid());

  {
    AutoLock auto_lock(lock_);
    AllOriginsSettings::iterator i(content_settings_.find(requesting_origin));
    if (i == content_settings_.end())
      return;
    content_settings_.erase(i);
  }

  PrefService* prefs = profile_->GetPrefs();
  DictionaryValue* all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kGeolocationContentSettings);
  updating_preferences_ = true;
  {
    ScopedPrefUpdate update(prefs, prefs::kGeolocationContentSettings);
    all_settings_dictionary->RemoveWithoutPathExpansion(
        UTF8ToWide(requesting_origin.spec()), NULL);
  }
  updating_preferences_ = false;
}

void GeolocationContentSettingsMap::ResetToDefault() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_setting_ = kDefaultSetting;
    content_settings_.clear();
  }

  PrefService* prefs = profile_->GetPrefs();
  updating_preferences_ = true;
  {
    prefs->ClearPref(prefs::kGeolocationDefaultContentSetting);
    prefs->ClearPref(prefs::kGeolocationContentSettings);
  }
  updating_preferences_ = false;
}

void GeolocationContentSettingsMap::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(NotificationType::PREF_CHANGED == type);
  DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
  if (updating_preferences_)
    return;

  std::wstring* name = Details<std::wstring>(details).ptr();
  if (prefs::kGeolocationContentSettings == *name) {
   ReadExceptions();
  } else {
   NOTREACHED() << "Unexpected preference observed.";
  }
}

GeolocationContentSettingsMap::~GeolocationContentSettingsMap() {
  profile_->GetPrefs()->RemovePrefObserver(prefs::kGeolocationContentSettings,
                                           this);
}

void GeolocationContentSettingsMap::ReadExceptions() {
  PrefService* prefs = profile_->GetPrefs();
  const DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(prefs::kGeolocationContentSettings);
  content_settings_.clear();
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(
             all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::wstring& wide_origin(*i);
      DictionaryValue* requesting_origin_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          wide_origin, &requesting_origin_settings_dictionary);
      DCHECK(found);
      GURL origin_as_url(WideToUTF8(wide_origin));
      if (!origin_as_url.is_valid())
        continue;
      OneOriginSettings* requesting_origin_settings =
          &content_settings_[origin_as_url];
      GetOneOriginSettingsFromDictionary(
          requesting_origin_settings_dictionary,
          requesting_origin_settings);
    }
  }
}

// static
void GeolocationContentSettingsMap::GetOneOriginSettingsFromDictionary(
    const DictionaryValue* dictionary,
    OneOriginSettings* one_origin_settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::wstring& target(*i);
    int setting = kDefaultSetting;
    bool found = dictionary->GetIntegerWithoutPathExpansion(target, &setting);
    DCHECK(found);
    GURL target_url(WideToUTF8(target));
    // An empty URL has a special meaning (wildcard), so only accept invalid
    // URLs if the original version was empty (avoids treating corrupted prefs
    // as the wildcard entry; see http://crbug.com/39685)
    if (target_url.is_valid() || target.empty())
      (*one_origin_settings)[target_url] = IntToContentSetting(setting);
  }
}
