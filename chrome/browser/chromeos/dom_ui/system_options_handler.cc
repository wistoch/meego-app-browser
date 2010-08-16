// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/system_options_handler.h"

#include <string>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dom_ui/system_settings_provider.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

SystemOptionsHandler::SystemOptionsHandler()
    : chromeos::CrosOptionsPageUIHandler(
        new chromeos::SystemSettingsProvider()) {
}

SystemOptionsHandler::~SystemOptionsHandler() {
}

void SystemOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // System page - ChromeOS
  localized_strings->SetString("systemPage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SYSTEM_TAB_LABEL));
  localized_strings->SetString("datetime_title",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME));
  localized_strings->SetString("timezone",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION));

  localized_strings->SetString("touchpad",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
  localized_strings->SetString("enable_tap_to_click",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  localized_strings->SetString("enable_vert_edge_scroll",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_VERT_EDGE_SCROLL_ENABLED_DESCRIPTION));
  localized_strings->SetString("sensitivity",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION));
  localized_strings->SetString("speed_factor",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SPEED_FACTOR_DESCRIPTION));

  localized_strings->SetString("language",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE));
  localized_strings->SetString("language_customize",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));

  localized_strings->SetString("keyboard",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_KEYBOARD));
  localized_strings->SetString("repeat_keys",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_ENABLED));
  localized_strings->SetString("repeat_delay",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_DELAY));
  localized_strings->SetString("repeat_delay_short",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_DELAY_SHORT));
  localized_strings->SetString("repeat_delay_long",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_DELAY_LONG));
  localized_strings->SetString("repeat_delay_min",
      base::IntToString(chromeos::kXkbAutoRepeatDelayPref.min_pref_value));
  localized_strings->SetString("repeat_delay_max",
      base::IntToString(chromeos::kXkbAutoRepeatDelayPref.max_pref_value));
  localized_strings->SetString("repeat_speed",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_SPEED));
  localized_strings->SetString("repeat_speed_fast",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_SPEED_FAST));
  localized_strings->SetString("repeat_speed_slow",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_SPEED_SLOW));
  localized_strings->SetString("repeat_speed_min",
      base::IntToString(chromeos::kXkbAutoRepeatIntervalPref.min_pref_value));
  localized_strings->SetString("repeat_speed_max",
      base::IntToString(chromeos::kXkbAutoRepeatIntervalPref.max_pref_value));

  localized_strings->SetString("accessibility_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY));
  localized_strings->SetString("accessibility",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DESCRIPTION));

  localized_strings->Set(L"timezoneList",
      reinterpret_cast<chromeos::SystemSettingsProvider*>(
          settings_provider_.get())->GetTimezoneList());
}

