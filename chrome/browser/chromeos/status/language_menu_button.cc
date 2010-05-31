// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_button.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/status/language_menu_l10n_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// The language menu consists of 3 parts (in this order):
//
//   (1) input method names. The size of the list is always >= 1.
//   (2) input method properties. This list might be empty.
//   (3) "Customize language and input..." button.
//
// Example of the menu (Japanese):
//
// ============================== (border of the popup window)
// [ ] English                    (|index| in the following functions is 0)
// [*] Japanese
// [ ] Chinese (Simplified)
// ------------------------------ (separator)
// [*] Hiragana                   (index = 5, The property has 2 radio groups)
// [ ] Katakana
// [ ] HalfWidthKatakana
// [*] Roman
// [ ] Kana
// ------------------------------ (separator)
// Customize language and input...(index = 11)
// ============================== (border of the popup window)
//
// Example of the menu (Simplified Chinese):
//
// ============================== (border of the popup window)
// [ ] English
// [ ] Japanese
// [*] Chinese (Simplified)
// ------------------------------ (separator)
// Switch to full letter mode     (The property has 2 command buttons)
// Switch to half punctuation mode
// ------------------------------ (separator)
// Customize language and input...
// ============================== (border of the popup window)
//

namespace {

// Constants to specify the type of items in |model_|.
enum {
  COMMAND_ID_INPUT_METHODS = 0,  // English, Chinese, Japanese, Arabic, ...
  COMMAND_ID_IME_PROPERTIES,  // Hiragana, Katakana, ...
  COMMAND_ID_CUSTOMIZE_LANGUAGE,  // "Customize language and input..." button.
};

// A group ID for IME properties starts from 0. We use the huge value for the
// input method list to avoid conflict.
const int kRadioGroupLanguage = 1 << 16;
const int kRadioGroupNone = -1;
const size_t kMaxLanguageNameLen = 2;
const wchar_t kSpacer[] = L"MMM";

// Returns the language name for the given |language_code|.
std::wstring GetLanguageName(const std::string& language_code) {
  const string16 language_name = l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
  // TODO(satorux): We should add input method names if multiple input
  // methods are available for one input language.
  return UTF16ToWide(language_name);
}

// Converts chromeos::InputMethodDescriptor object into human readable string.
// Returns a string for the drop-down menu if |for_menu| is true. Otherwise,
// returns a string for the status area.
std::wstring FormatInputLanguage(
    const chromeos::InputMethodDescriptor& input_method, bool for_menu,
    bool add_method_name) {
  const std::string language_code
      = chromeos::LanguageLibrary::GetLanguageCodeFromDescriptor(input_method);

  std::wstring formatted;
  if (language_code == "t") {
    // "t" is used by input methods that do not associate with a
    // particular language. Returns the display name as-is.
    formatted = UTF8ToWide(input_method.display_name);
  }

  if (for_menu) {
    // For the drop-down menu, we'll show language names like
    // "Chinese (Simplified)" and "Japanese", instead of input method names
    // like "Pinyin" and "Anthy".
    if (formatted.empty()) {
      formatted = GetLanguageName(language_code);
      if (add_method_name) {
        formatted += L" - ";
        formatted += chromeos::LanguageMenuL10nUtil::GetString(
            input_method.display_name);
      }
    }
  } else {
    // For the status area, we use two-letter, upper-case language code like
    // "EN" and "JA".
    if (formatted.empty()) {
      formatted = UTF8ToWide(language_code);
    }
    formatted = StringToUpperASCII(formatted).substr(0, kMaxLanguageNameLen);
  }
  DCHECK(!formatted.empty());
  return formatted;
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton

LanguageMenuButton::LanguageMenuButton(StatusAreaHost* host)
    : MenuButton(NULL, std::wstring(), this, false),
      input_method_descriptors_(CrosLibrary::Get()->GetLanguageLibrary()->
                                GetActiveInputMethods()),
      model_(NULL),
      // Be aware that the constructor of |language_menu_| calls GetItemCount()
      // in this class. Therefore, GetItemCount() have to return 0 when
      // |model_| is NULL.
      ALLOW_THIS_IN_INITIALIZER_LIST(language_menu_(this)),
      host_(host) {
  DCHECK(input_method_descriptors_.get() &&
         !input_method_descriptors_->empty());
  set_border(NULL);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(1, gfx::Font::BOLD));
  SetEnabledColor(0xB3FFFFFF);  // White with 70% Alpha
  SetDisabledColor(0x4DFFFFFF);  // White with 30% Alpha
  SetEnabled(false);  // Disable the button until the first FocusIn comes in.
  SetShowHighlighted(false);
  // Update the model
  RebuildModel();
  // Grab the real estate.
  UpdateIcon(kSpacer, L"" /* no tooltip */);
  // Display the default input method name.
  const std::wstring name
      = FormatInputLanguage(input_method_descriptors_->at(0), false, false);
  // TODO(yusukes): The assumption that the input method at index 0 is enabled
  // by default is not always true. We should fix the logic once suzhe's patches
  // for issue 2627 (get/set ibus state without focus) are submitted.
  UpdateIcon(name, L"" /* no tooltip */);
  CrosLibrary::Get()->GetLanguageLibrary()->AddObserver(this);
}

LanguageMenuButton::~LanguageMenuButton() {
  CrosLibrary::Get()->GetLanguageLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, menus::MenuModel implementation:

int LanguageMenuButton::GetCommandIdAt(int index) const {
  return 0;  // dummy
}

bool LanguageMenuButton::IsLabelDynamicAt(int index) const {
  // Menu content for the language button could change time by time.
  return true;
}

bool LanguageMenuButton::GetAcceleratorAt(
    int index, menus::Accelerator* accelerator) const {
  // Views for Chromium OS does not support accelerators yet.
  return false;
}

bool LanguageMenuButton::IsItemCheckedAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  if (IndexIsInInputMethodList(index)) {
    const InputMethodDescriptor& input_method
        = input_method_descriptors_->at(index);
    return input_method == CrosLibrary::Get()->GetLanguageLibrary()->
          current_input_method();
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    return property_list.at(index).is_selection_item_checked;
  }

  // Separator(s) or the "Customize language and input..." button.
  return false;
}

int LanguageMenuButton::GetGroupIdAt(int index) const {
  DCHECK_GE(index, 0);

  if (IndexIsInInputMethodList(index)) {
    return kRadioGroupLanguage;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    return property_list.at(index).selection_item_id;
  }

  return kRadioGroupNone;
}

bool LanguageMenuButton::HasIcons() const  {
  // We don't support icons on Chrome OS.
  return false;
}

bool LanguageMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

bool LanguageMenuButton::IsEnabledAt(int index) const {
  // Just return true so all input method names and input method propertie names
  // could be clicked.
  return true;
}

menus::MenuModel* LanguageMenuButton::GetSubmenuModelAt(int index) const {
  // We don't use nested menus.
  return NULL;
}

void LanguageMenuButton::HighlightChangedTo(int index) {
  // Views for Chromium OS does not support this interface yet.
}

void LanguageMenuButton::MenuWillShow() {
  // Views for Chromium OS does not support this interface yet.
}

int LanguageMenuButton::GetItemCount() const {
  if (!model_.get()) {
    // Model is not constructed yet. This means that LanguageMenuButton is
    // being constructed. Return zero.
    return 0;
  }
  return model_->GetItemCount();
}

menus::MenuModel::ItemType LanguageMenuButton::GetTypeAt(int index) const {
  DCHECK_GE(index, 0);

  if (IndexPointsToConfigureImeMenuItem(index)) {
    return menus::MenuModel::TYPE_COMMAND;  // "Customize language and input"
  }

  if (IndexIsInInputMethodList(index)) {
    return menus::MenuModel::TYPE_RADIO;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    if (property_list.at(index).is_selection_item) {
      return menus::MenuModel::TYPE_RADIO;
    }
    return menus::MenuModel::TYPE_COMMAND;
  }

  return menus::MenuModel::TYPE_SEPARATOR;
}

string16 LanguageMenuButton::GetLabelAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  // We use IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE here as the button
  // opens the same dialog that is opened from the main options dialog.
  if (IndexPointsToConfigureImeMenuItem(index)) {
    return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE);
  }

  std::wstring name;
  if (IndexIsInInputMethodList(index)) {
    const std::string language_code =
        chromeos::LanguageLibrary::GetLanguageCodeFromDescriptor(
            input_method_descriptors_->at(index));
    bool need_method_name = (need_method_name_.count(language_code) > 0);
    name = FormatInputLanguage(input_method_descriptors_->at(index), true,
                               need_method_name);
  } else if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    return LanguageMenuL10nUtil::GetStringUTF16(
        property_list.at(index).label);
  }

  return WideToUTF16(name);
}

void LanguageMenuButton::ActivatedAt(int index) {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  if (IndexPointsToConfigureImeMenuItem(index)) {
    host_->OpenButtonOptions(this);
    return;
  }

  if (IndexIsInInputMethodList(index)) {
    // Inter-IME switching.
    const InputMethodDescriptor& input_method
        = input_method_descriptors_->at(index);
    CrosLibrary::Get()->GetLanguageLibrary()->ChangeInputMethod(
        input_method.id);
    return;
  }

  if (GetPropertyIndex(index, &index)) {
    // Intra-IME switching (e.g. Japanese-Hiragana to Japanese-Katakana).
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    const std::string key = property_list.at(index).key;
    if (property_list.at(index).is_selection_item) {
      // Radio button is clicked.
      const int id = property_list.at(index).selection_item_id;
      // First, deactivate all other properties in the same radio group.
      for (int i = 0; i < static_cast<int>(property_list.size()); ++i) {
        if (i != index && id == property_list.at(i).selection_item_id) {
          CrosLibrary::Get()->GetLanguageLibrary()->SetImePropertyActivated(
              property_list.at(i).key, false);
        }
      }
      // Then, activate the property clicked.
      CrosLibrary::Get()->GetLanguageLibrary()->SetImePropertyActivated(key,
                                                                        true);
    } else {
      // Command button like "Switch to half punctuation mode" is clicked.
      // We can always use "Deactivate" for command buttons.
      CrosLibrary::Get()->GetLanguageLibrary()->SetImePropertyActivated(key,
                                                                        false);
    }
    return;
  }

  LOG(ERROR) << "Unexpected index: " << index;
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, views::ViewMenuDelegate implementation:

void LanguageMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  input_method_descriptors_.reset(CrosLibrary::Get()->GetLanguageLibrary()->
                                  GetActiveInputMethods());
  RebuildModel();
  language_menu_.Rebuild();
  language_menu_.UpdateStates();
  language_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// LanguageLibrary::Observer implementation:

void LanguageMenuButton::InputMethodChanged(LanguageLibrary* obj) {
  const chromeos::InputMethodDescriptor& input_method =
      obj->current_input_method();
  const std::wstring name = FormatInputLanguage(input_method, false, false);
  const std::wstring tooltip = FormatInputLanguage(input_method, true, true);
  UpdateIcon(name, tooltip);
}

void LanguageMenuButton::ImePropertiesChanged(LanguageLibrary* obj) {
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

void LanguageMenuButton::LocaleChanged() {
  const chromeos::InputMethodDescriptor& input_method =
      CrosLibrary::Get()->GetLanguageLibrary()->current_input_method();
  const std::wstring name = FormatInputLanguage(input_method, false, false);
  const std::wstring tooltip = FormatInputLanguage(input_method, true, true);
  UpdateIcon(name, tooltip);
  Layout();
  SchedulePaint();
}

void LanguageMenuButton::FocusChanged(LanguageLibrary* obj) {
  LanguageLibrary* language_library = CrosLibrary::Get()->GetLanguageLibrary();
  SetEnabled(language_library->is_focused());
  SchedulePaint();
}

void LanguageMenuButton::UpdateIcon(
    const std::wstring& name, const std::wstring& tooltip) {
  if (!tooltip.empty()) {
    SetTooltipText(tooltip);
  }
  SetText(name);
  set_alignment(TextButton::ALIGN_RIGHT);
  SchedulePaint();
}

void LanguageMenuButton::RebuildModel() {
  model_.reset(new menus::SimpleMenuModel(NULL));
  string16 dummy_label = UTF8ToUTF16("");
  // Indicates if separator's needed before each section.
  bool need_separator = false;

  need_method_name_.clear();
  std::set<std::string> languages_seen;
  if (!input_method_descriptors_->empty()) {
    // We "abuse" the command_id and group_id arguments of AddRadioItem method.
    // A COMMAND_ID_XXX enum value is passed as command_id, and array index of
    // |input_method_descriptors_| or |property_list| is passed as group_id.
    for (size_t i = 0; i < input_method_descriptors_->size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_INPUT_METHODS, dummy_label, i);

      const std::string language_code
          = chromeos::LanguageLibrary::GetLanguageCodeFromDescriptor(
              input_method_descriptors_->at(i));
      // If there is more than one input method for this language, then we need
      // to display the method name.
      if (languages_seen.find(language_code) == languages_seen.end()) {
        languages_seen.insert(language_code);
      } else {
        need_method_name_.insert(language_code);
      }
    }
    need_separator = true;
  }

  const ImePropertyList& property_list
      = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
  if (!property_list.empty()) {
    if (need_separator)
      model_->AddSeparator();
    for (size_t i = 0; i < property_list.size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_IME_PROPERTIES, dummy_label, i);
    }
    need_separator = true;
  }

  if (host_->ShouldOpenButtonOptions(this)) {
    // Note: We use AddSeparator() for separators, and AddRadioItem() for all
    // other items even if an item is not actually a radio item.
    if (need_separator)
      model_->AddSeparator();
    model_->AddRadioItem(COMMAND_ID_CUSTOMIZE_LANGUAGE, dummy_label,
                         0 /* dummy */);
  }
}

bool LanguageMenuButton::IndexIsInInputMethodList(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_INPUT_METHODS) &&
          input_method_descriptors_.get() &&
          (index < static_cast<int>(input_method_descriptors_->size())));
}

bool LanguageMenuButton::GetPropertyIndex(
    int index, int* property_index) const {
  DCHECK_GE(index, 0);
  DCHECK(property_index);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  if ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
      (model_->GetCommandIdAt(index) == COMMAND_ID_IME_PROPERTIES)) {
    const int tmp_property_index = model_->GetGroupIdAt(index);
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetLanguageLibrary()->current_ime_properties();
    if (tmp_property_index < static_cast<int>(property_list.size())) {
      *property_index = tmp_property_index;
      return true;
    }
  }
  return false;
}

bool LanguageMenuButton::IndexPointsToConfigureImeMenuItem(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_CUSTOMIZE_LANGUAGE));
}

}  // namespace chromeos
