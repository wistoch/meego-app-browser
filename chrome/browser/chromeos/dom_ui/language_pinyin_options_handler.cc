// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_pinyin_options_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dom_ui/language_options_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

// TODO(kochi): Add prefix for each language (e.g. "Pinyin") for each i18n
// identifier.

namespace chromeos {

LanguagePinyinOptionsHandler::LanguagePinyinOptionsHandler() {
}

LanguagePinyinOptionsHandler::~LanguagePinyinOptionsHandler() {
}

void LanguagePinyinOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Pinyin page - ChromeOS
  for (size_t i = 0; i < kNumPinyinBooleanPrefs; ++i) {
    localized_strings->SetString(
        GetI18nContentValue(kPinyinBooleanPrefs[i]),
        l10n_util::GetString(kPinyinBooleanPrefs[i].message_id));
  }

  localized_strings->SetString(
      GetI18nContentValue(kPinyinDoublePinyinSchema),
      l10n_util::GetString(
          kPinyinDoublePinyinSchema.label_message_id));
  ListValue* list_value = new ListValue();
  for (size_t i = 0;
       i < LanguageMultipleChoicePreference<int>::kMaxItems;
       ++i) {
    if (kPinyinDoublePinyinSchema.values_and_ids[i].
        item_message_id == 0)
      break;
    ListValue* option = new ListValue();
    option->Append(Value::CreateIntegerValue(
        kPinyinDoublePinyinSchema.values_and_ids[i].
        ibus_config_value));
    option->Append(Value::CreateStringValue(l10n_util::GetString(
        kPinyinDoublePinyinSchema.values_and_ids[i].
        item_message_id)));
    list_value->Append(option);
  }
  localized_strings->Set(
      GetTemplateDataPropertyName(kPinyinDoublePinyinSchema),
      list_value);
}

}  // namespace chromeos
