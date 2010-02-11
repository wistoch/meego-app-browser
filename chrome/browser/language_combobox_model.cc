// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_combobox_model.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "unicode/uloc.h"

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel used to populate a combobox with native names
// corresponding to the language code (e.g. English (United States) for en-US)
//

LanguageComboboxModel::LanguageComboboxModel()
    : profile_(NULL) {
  // Enumerate the languages we know about.
  const std::vector<std::string>& locale_codes =
      l10n_util::GetAvailableLocales();
  InitNativeNames(locale_codes);
}

LanguageComboboxModel::LanguageComboboxModel(
    Profile* profile, const std::vector<std::string>& locale_codes)
    : profile_(profile) {
  InitNativeNames(locale_codes);
}

void LanguageComboboxModel::InitNativeNames(
    const std::vector<std::string>& locale_codes) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < locale_codes.size(); ++i) {
    std::string locale_code_str = locale_codes[i];
    const char* locale_code = locale_codes[i].c_str();

    // TODO(jungshik): Even though these strings are used for the UI,
    // the old code does not add an RTL mark for RTL locales. Make sure
    // that it's ok without that.
    string16 name_in_current_ui =
        l10n_util::GetDisplayNameForLocale(locale_code, app_locale, false);
    string16 name_native =
        l10n_util::GetDisplayNameForLocale(locale_code, locale_code, false);

    locale_names_.push_back(UTF16ToWideHack(name_in_current_ui));
    native_names_[UTF16ToWideHack(name_in_current_ui)] = LocaleData(
        UTF16ToWideHack(name_native), locale_codes[i]);
  }

  // Sort using locale specific sorter.
  l10n_util::SortStrings(g_browser_process->GetApplicationLocale(),
                         &locale_names_);
}

// Overridden from ComboboxModel:
int LanguageComboboxModel::GetItemCount() {
  return static_cast<int>(locale_names_.size());
}

std::wstring LanguageComboboxModel::GetItemAt(int index) {
  DCHECK(static_cast<int>(locale_names_.size()) > index);
  LocaleDataMap::const_iterator it =
      native_names_.find(locale_names_[index]);
  DCHECK(it != native_names_.end());

  // If the name is the same in the native language and local language,
  // don't show it twice.
  if (it->second.native_name == locale_names_[index])
    return it->second.native_name;

  // We must add directionality formatting to both the native name and the
  // locale name in order to avoid text rendering problems such as misplaced
  // parentheses or languages appearing in the wrong order.
  std::wstring locale_name_localized;
  std::wstring locale_name;
  if (l10n_util::AdjustStringForLocaleDirection(locale_names_[index],
                                                &locale_name_localized))
    locale_name.assign(locale_name_localized);
  else
    locale_name.assign(locale_names_[index]);

  std::wstring native_name_localized;
  std::wstring native_name;
  if (l10n_util::AdjustStringForLocaleDirection(it->second.native_name,
                                                &native_name_localized))
    native_name.assign(native_name_localized);
  else
    native_name.assign(it->second.native_name);

  // We used to have a localizable template here, but none of translators
  // changed the format. We also want to switch the order of locale_name
  // and native_name without going back to translators.
  std::wstring formatted_item;
  SStringPrintf(&formatted_item, L"%ls - %ls", locale_name.c_str(),
                native_name.c_str());
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    // Somehow combo box (even with LAYOUTRTL flag) doesn't get this
    // right so we add RTL BDO (U+202E) to set the direction
    // explicitly.
    formatted_item.insert(0, L"\x202E");
  return formatted_item;
}

// Return the locale for the given index.  E.g., may return pt-BR.
std::string LanguageComboboxModel::GetLocaleFromIndex(int index) {
  DCHECK(static_cast<int>(locale_names_.size()) > index);
  LocaleDataMap::const_iterator it =
      native_names_.find(locale_names_[index]);
  DCHECK(it != native_names_.end());

  return it->second.locale_code;
}

int LanguageComboboxModel::GetIndexFromLocale(const std::string& locale) {
  for (size_t i = 0; i < locale_names_.size(); ++i) {
    LocaleDataMap::const_iterator it =
        native_names_.find(locale_names_[i]);
    DCHECK(it != native_names_.end());
    if (it->second.locale_code == locale)
      return static_cast<int>(i);
  }
  return -1;
}

// Returns the index of the language currently specified in the user's
// preference file.  Note that it's possible for language A to be picked
// while chrome is currently in language B if the user specified language B
// via --lang.  Since --lang is not a persistent setting, it seems that it
// shouldn't be reflected in this combo box.  We return -1 if the value in
// the pref doesn't map to a know language (possible if the user edited the
// prefs file manually).
int LanguageComboboxModel::GetSelectedLanguageIndex(const std::wstring& prefs) {
  PrefService* local_state;
  if (!profile_)
    local_state = g_browser_process->local_state();
  else
    local_state = profile_->GetPrefs();

  DCHECK(local_state);
  const std::string& current_locale =
      WideToASCII(local_state->GetString(prefs.c_str()));

  return GetIndexFromLocale(current_locale);
}
