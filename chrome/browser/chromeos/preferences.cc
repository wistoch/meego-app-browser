// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/cros/synaptics_library.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "unicode/timezone.h"

namespace chromeos {

// static
void Preferences::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kTimeZone, L"US/Pacific");
  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled, false);
  prefs->RegisterBooleanPref(prefs::kAccessibilityEnabled, false);
  prefs->RegisterBooleanPref(prefs::kVertEdgeScrollEnabled, false);
  prefs->RegisterIntegerPref(prefs::kTouchpadSpeedFactor, 9);
  prefs->RegisterIntegerPref(prefs::kTouchpadSensitivity, 5);
  prefs->RegisterStringPref(prefs::kLanguageCurrentInputMethod, L"");
  prefs->RegisterStringPref(prefs::kLanguagePreviousInputMethod, L"");
  prefs->RegisterStringPref(prefs::kLanguageHotkeyNextEngineInMenu,
                            kHotkeyNextEngineInMenu);
  prefs->RegisterStringPref(prefs::kLanguageHotkeyPreviousEngine,
                            kHotkeyPreviousEngine);
  prefs->RegisterStringPref(prefs::kLanguagePreloadEngines,
                            UTF8ToWide(kFallbackInputMethodId));  // EN layout
  for (size_t i = 0; i < kNumChewingBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(kChewingBooleanPrefs[i].pref_name,
                               kChewingBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < kNumChewingMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        kChewingMultipleChoicePrefs[i].pref_name,
        UTF8ToWide(kChewingMultipleChoicePrefs[i].default_pref_value));
  }
  prefs->RegisterIntegerPref(kChewingHsuSelKeyType.pref_name,
                             kChewingHsuSelKeyType.default_pref_value);

  for (size_t i = 0; i < kNumChewingIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(kChewingIntegerPrefs[i].pref_name,
                               kChewingIntegerPrefs[i].default_pref_value);
  }
  prefs->RegisterStringPref(prefs::kLanguageHangulKeyboard,
                            kHangulKeyboardNameIDPairs[0].keyboard_id);
  for (size_t i = 0; i < kNumPinyinBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(kPinyinBooleanPrefs[i].pref_name,
                               kPinyinBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < kNumPinyinIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(kPinyinIntegerPrefs[i].pref_name,
                               kPinyinIntegerPrefs[i].default_pref_value);
  }
  prefs->RegisterIntegerPref(kPinyinDoublePinyinSchema.pref_name,
                             kPinyinDoublePinyinSchema.default_pref_value);

  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(kMozcBooleanPrefs[i].pref_name,
                               kMozcBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        kMozcMultipleChoicePrefs[i].pref_name,
        UTF8ToWide(kMozcMultipleChoicePrefs[i].default_pref_value));
  }
}

void Preferences::Init(PrefService* prefs) {
  timezone_.Init(prefs::kTimeZone, prefs, this);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  accessibility_enabled_.Init(prefs::kAccessibilityEnabled, prefs, this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled, prefs, this);
  speed_factor_.Init(prefs::kTouchpadSpeedFactor, prefs, this);
  sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, this);
  language_hotkey_next_engine_in_menu_.Init(
      prefs::kLanguageHotkeyNextEngineInMenu, prefs, this);
  language_hotkey_previous_engine_.Init(
      prefs::kLanguageHotkeyPreviousEngine, prefs, this);
  language_preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, this);
  for (size_t i = 0; i < kNumChewingBooleanPrefs; ++i) {
    language_chewing_boolean_prefs_[i].Init(
        kChewingBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < kNumChewingMultipleChoicePrefs; ++i) {
    language_chewing_multiple_choice_prefs_[i].Init(
        kChewingMultipleChoicePrefs[i].pref_name, prefs, this);
  }
  language_chewing_hsu_sel_key_type_.Init(
      kChewingHsuSelKeyType.pref_name, prefs, this);
  for (size_t i = 0; i < kNumChewingIntegerPrefs; ++i) {
    language_chewing_integer_prefs_[i].Init(
        kChewingIntegerPrefs[i].pref_name, prefs, this);
  }
  language_hangul_keyboard_.Init(prefs::kLanguageHangulKeyboard, prefs, this);
  for (size_t i = 0; i < kNumPinyinBooleanPrefs; ++i) {
    language_pinyin_boolean_prefs_[i].Init(
        kPinyinBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < kNumPinyinIntegerPrefs; ++i) {
    language_pinyin_int_prefs_[i].Init(
        kPinyinIntegerPrefs[i].pref_name, prefs, this);
  }
  language_pinyin_double_pinyin_schema_.Init(
      kPinyinDoublePinyinSchema.pref_name, prefs, this);
  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    language_mozc_boolean_prefs_[i].Init(
        kMozcBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    language_mozc_multiple_choice_prefs_[i].Init(
        kMozcMultipleChoicePrefs[i].pref_name, prefs, this);
  }

  // Initialize touchpad settings to what's saved in user preferences.
  NotifyPrefChanged(NULL);
}

void Preferences::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

void Preferences::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTimeZone)
    SetTimeZone(timezone_.GetValue());
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    CrosLibrary::Get()->GetSynapticsLibrary()->SetBoolParameter(
        PARAM_BOOL_TAP_TO_CLICK,
        tap_to_click_enabled_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled) {
    CrosLibrary::Get()->GetSynapticsLibrary()->SetBoolParameter(
        PARAM_BOOL_VERTICAL_EDGE_SCROLLING,
        vert_edge_scroll_enabled_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSpeedFactor) {
    CrosLibrary::Get()->GetSynapticsLibrary()->SetRangeParameter(
        PARAM_RANGE_SPEED_SENSITIVITY,
        speed_factor_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    CrosLibrary::Get()->GetSynapticsLibrary()->SetRangeParameter(
          PARAM_RANGE_TOUCH_SENSITIVITY,
          sensitivity_.GetValue());
  }

  // We don't handle prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // here.

  if (!pref_name || *pref_name == prefs::kLanguageHotkeyNextEngineInMenu) {
    SetLanguageConfigStringListAsCSV(kHotKeySectionName,
                             kNextEngineInMenuConfigName,
                             language_hotkey_next_engine_in_menu_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguageHotkeyPreviousEngine) {
    SetLanguageConfigStringListAsCSV(kHotKeySectionName,
                             kPreviousEngineConfigName,
                             language_hotkey_previous_engine_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguagePreloadEngines) {
    SetLanguageConfigStringListAsCSV(kGeneralSectionName,
                                     kPreloadEnginesConfigName,
                                     language_preload_engines_.GetValue());
  }
  for (size_t i = 0; i < kNumChewingBooleanPrefs; ++i) {
    if (!pref_name || *pref_name == kChewingBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(kChewingSectionName,
                               kChewingBooleanPrefs[i].ibus_config_name,
                               language_chewing_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < kNumChewingMultipleChoicePrefs; ++i) {
    if (!pref_name || *pref_name == kChewingMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          kChewingSectionName,
          kChewingMultipleChoicePrefs[i].ibus_config_name,
          language_chewing_multiple_choice_prefs_[i].GetValue());
    }
  }
  if (!pref_name || *pref_name == kChewingHsuSelKeyType.pref_name) {
    SetLanguageConfigInteger(
        kChewingSectionName,
        kChewingHsuSelKeyType.ibus_config_name,
        language_chewing_hsu_sel_key_type_.GetValue());
  }
  for (size_t i = 0; i < kNumChewingIntegerPrefs; ++i) {
    if (!pref_name || *pref_name == kChewingIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(kChewingSectionName,
                               kChewingIntegerPrefs[i].ibus_config_name,
                               language_chewing_integer_prefs_[i].GetValue());
    }
  }
  if (!pref_name || *pref_name == prefs::kLanguageHangulKeyboard) {
    SetLanguageConfigString(kHangulSectionName, kHangulKeyboardConfigName,
                            language_hangul_keyboard_.GetValue());
  }
  for (size_t i = 0; i < kNumPinyinBooleanPrefs; ++i) {
    if (!pref_name || *pref_name == kPinyinBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(kPinyinSectionName,
                               kPinyinBooleanPrefs[i].ibus_config_name,
                               language_pinyin_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < kNumPinyinIntegerPrefs; ++i) {
    if (!pref_name || *pref_name == kPinyinIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(kPinyinSectionName,
                               kPinyinIntegerPrefs[i].ibus_config_name,
                               language_pinyin_int_prefs_[i].GetValue());
    }
  }
  if (!pref_name || *pref_name == kPinyinDoublePinyinSchema.pref_name) {
    SetLanguageConfigInteger(
        kPinyinSectionName,
        kPinyinDoublePinyinSchema.ibus_config_name,
        language_pinyin_double_pinyin_schema_.GetValue());
  }
  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    if (!pref_name || *pref_name == kMozcBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(kMozcSectionName,
                               kMozcBooleanPrefs[i].ibus_config_name,
                               language_mozc_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    if (!pref_name || *pref_name == kMozcMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          kMozcSectionName,
          kMozcMultipleChoicePrefs[i].ibus_config_name,
          language_mozc_multiple_choice_prefs_[i].GetValue());
    }
  }
}

void Preferences::SetTimeZone(const std::wstring& id) {
  icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(WideToASCII(id)));
  icu::TimeZone::adoptDefault(timezone);
}

void Preferences::SetLanguageConfigBoolean(const char* section,
                                           const char* name,
                                           bool value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeBool;
  config.bool_value = value;
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigInteger(const char* section,
                                           const char* name,
                                           int value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeInt;
  config.int_value = value;
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigString(const char* section,
                                          const char* name,
                                          const std::wstring& value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeString;
  config.string_value = WideToUTF8(value);
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringList(
    const char* section,
    const char* name,
    const std::vector<std::wstring>& values) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeStringList;
  for (size_t i = 0; i < values.size(); ++i) {
    config.string_list_value.push_back(WideToUTF8(values[i]));
  }
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::wstring& value) {
  LOG(INFO) << "Setting " << name << " to '" << value << "'";

  std::vector<std::wstring> split_values;
  if (!value.empty()) {
    SplitString(value, L',', &split_values);
  }
  // We should call the cros API even when |value| is empty, to disable default
  // config.
  SetLanguageConfigStringList(section, name, split_values);
}

}  // namespace chromeos
