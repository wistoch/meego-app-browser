// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_chewing_options_handler.h"

#include <limits>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dom_ui/language_options_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

namespace chromeos {

LanguageChewingOptionsHandler::LanguageChewingOptionsHandler() {
}

LanguageChewingOptionsHandler::~LanguageChewingOptionsHandler() {
}

void LanguageChewingOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Chewing page - ChromeOS
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    localized_strings->SetString(
        GetI18nContentValue(language_prefs::kChewingBooleanPrefs[i]),
        l10n_util::GetStringUTF16(
            language_prefs::kChewingBooleanPrefs[i].message_id));
  }

  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    const language_prefs::LanguageIntegerRangePreference& preference =
        language_prefs::kChewingIntegerPrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference),
        l10n_util::GetStringUTF16(preference.message_id));
    localized_strings->SetString(
        GetTemplateDataMinName(preference),
        base::IntToString(preference.min_pref_value));
    localized_strings->SetString(
        GetTemplateDataMaxName(preference),
        base::IntToString(preference.max_pref_value));
  }

  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs;
       ++i) {
    const language_prefs::LanguageMultipleChoicePreference<const char*>&
        preference = language_prefs::kChewingMultipleChoicePrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference),
        l10n_util::GetStringUTF16(preference.label_message_id));
    localized_strings->Set(
        GetTemplateDataPropertyName(preference),
        CreateMultipleChoiceList(preference));
  }

  localized_strings->SetString(
      GetI18nContentValue(language_prefs::kChewingHsuSelKeyType),
      l10n_util::GetStringUTF16(
          language_prefs::kChewingHsuSelKeyType.label_message_id));
  localized_strings->Set(
      GetTemplateDataPropertyName(language_prefs::kChewingHsuSelKeyType),
      CreateMultipleChoiceList(language_prefs::kChewingHsuSelKeyType));
}

}  // namespace chromeos
