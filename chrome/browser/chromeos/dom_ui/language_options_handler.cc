// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"

#include <map>
#include <string>
#include <utility>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"languagePage",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE));
  localized_strings->SetString(L"add_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON));
  localized_strings->SetString(L"configure",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  localized_strings->SetString(L"input_method",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  localized_strings->SetString(L"languages",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES));
  localized_strings->SetString(L"remove_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  localized_strings->SetString(L"others",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS));
  localized_strings->SetString(L"is_displayed_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"display_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"restart_required",
      l10n_util::GetString(IDS_OPTIONS_RESTART_REQUIRED));

  // The followigns are resources, rather than local strings.
  localized_strings->SetString(L"currentUiLanguageCode",
      UTF8ToWide(g_browser_process->GetApplicationLocale()));
  localized_strings->Set(L"inputMethodList", GetInputMethodList());
  localized_strings->Set(L"languageList", GetLanguageList());
}

void LanguageOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("uiLanguageChange",
      NewCallback(this,
                  &LanguageOptionsHandler::UiLanguageChangeCallback));
}

ListValue* LanguageOptionsHandler::GetInputMethodList() {
  using chromeos::CrosLibrary;

  ListValue* input_method_list = new ListValue();

  // GetSupportedLanguages() never return NULL.
  scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetSupportedInputMethods());
  for (size_t i = 0; i < descriptors->size(); ++i) {
    const chromeos::InputMethodDescriptor& descriptor = descriptors->at(i);
    const std::string language_code =
        chromeos::input_method::GetLanguageCodeFromDescriptor(descriptor);
    const std::string display_name =
        chromeos::input_method::GetInputMethodDisplayNameFromId(descriptor.id);

    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString(L"id", UTF8ToWide(descriptor.id));
    dictionary->SetString(L"displayName", UTF8ToWide(display_name));
    dictionary->SetString(L"languageCode", UTF8ToWide(language_code));
    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

ListValue* LanguageOptionsHandler::GetLanguageList() {
  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, std::wstring> LanguagePair;
  typedef std::map<std::wstring, LanguagePair> LanguageMap;
  LanguageMap language_map;
  // The auxiliary vector mentioned above.
  std::vector<std::wstring> display_names;

  // Build the list of display names, and build the language map.
  const std::vector<std::string>& locales = l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < locales.size(); ++i) {
    const std::wstring display_name =
        chromeos::input_method::GetLanguageDisplayNameFromCode(locales[i]);
    const std::wstring native_display_name =
        chromeos::input_method::GetLanguageNativeDisplayNameFromCode(
            locales[i]);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(locales[i], native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings(g_browser_process->GetApplicationLocale(),
                         &display_names);

  // Build the language list from the language map.
  ListValue* language_list = new ListValue();
  for (size_t i = 0; i < display_names.size(); ++i) {
    const LanguagePair& pair = language_map[display_names[i]];
    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString(L"code",  pair.first);
    dictionary->SetString(L"displayName", display_names[i]);
    dictionary->SetString(L"nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

void LanguageOptionsHandler::UiLanguageChangeCallback(
    const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    LOG(INFO) << "NOTREACHED";
    return;
  }
  const ListValue* list_value = static_cast<const ListValue*>(value);
  std::string language_code;
  if (list_value->GetSize() != 1 ||
      !list_value->GetString(0, &language_code)) {
    NOTREACHED();
    LOG(INFO) << "NOTREACHED";
    return;
  }
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, language_code);
  prefs->SavePersistentPrefs();
  dom_ui_->CallJavascriptFunction(
      L"options.LanguageOptions.uiLanguageSaved");
}
