// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_mozc_config_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/language_config_util.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

LanguageMozcConfigView::LanguageMozcConfigView(Profile* profile)
    : OptionsPageView(profile), contents_(NULL) {
  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    current.boolean_pref.Init(
        kMozcBooleanPrefs[i].pref_name, profile->GetPrefs(), this);
    current.checkbox = NULL;
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.multiple_choice_pref.Init(
        kMozcMultipleChoicePrefs[i].pref_name, profile->GetPrefs(), this);
    current.combobox_model =
        new LanguageComboboxModel<const char*>(&kMozcMultipleChoicePrefs[i]);
    current.combobox = NULL;
  }
}

LanguageMozcConfigView::~LanguageMozcConfigView() {
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    delete prefs_and_comboboxes_[i].combobox_model;
  }
}

void LanguageMozcConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  views::Checkbox* checkbox = static_cast<views::Checkbox*>(sender);
  const int pref_id = checkbox->tag();
  DCHECK(pref_id >= 0 && pref_id < static_cast<int>(kNumMozcBooleanPrefs));
  prefs_and_checkboxes_[pref_id].boolean_pref.SetValue(checkbox->checked());
}

void LanguageMozcConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    if (current.combobox == sender) {
      const std::wstring config_value =
          UTF8ToWide(current.combobox_model->GetConfigValueAt(new_index));
      LOG(INFO) << "Changing Mozc pref to " << config_value;
      // Update the Chrome pref.
      current.multiple_choice_pref.SetValue(config_value);
      break;
    }
  }
}

void LanguageMozcConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageMozcConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SETTINGS_TITLE);
}

gfx::Size LanguageMozcConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  gfx::Size preferred_size = views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES);
  // TODO(mazda): Remove the manual adjustment.
  // The padding is needed for accommodating all the controls in the dialog.
  const int kHeightPadding = 20;
  preferred_size.Enlarge(0, kHeightPadding);
  return preferred_size;
}

void LanguageMozcConfigView::InitControlLayout() {
  using views::ColumnSet;
  using views::GridLayout;

  contents_ = new views::View;
  AddChildView(contents_);

  GridLayout* layout = new GridLayout(contents_);
  layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                    kPanelVertMargin, kPanelHorizMargin);
  contents_->SetLayoutManager(layout);

  const int kColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    current.checkbox = new views::Checkbox(
        l10n_util::GetString(kMozcBooleanPrefs[i].message_id));
    current.checkbox->set_listener(this);
    current.checkbox->set_tag(i);
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.combobox = new LanguageCombobox(current.combobox_model);
    current.combobox->set_listener(this);
  }
  NotifyPrefChanged();  // Sync the comboboxes with current Chrome prefs.

  // Show the checkboxes.
  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    const MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(current.checkbox, 3, 1);
  }
  // Show the comboboxes.
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    const MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(new views::Label(current.combobox_model->GetLabel()));
    layout->AddView(current.combobox);
  }
}

void LanguageMozcConfigView::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguageMozcConfigView::NotifyPrefChanged() {
  // Update comboboxes.
  // TODO(yusukes): We don't have to update all UI controls.
  for (size_t i = 0; i < kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    const bool checked = current.boolean_pref.GetValue();
    current.checkbox->SetChecked(checked);
  }
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    const std::wstring value = current.multiple_choice_pref.GetValue();
    for (int i = 0; i < current.combobox_model->num_items(); ++i) {
      if (UTF8ToWide(current.combobox_model->GetConfigValueAt(i)) == value) {
        current.combobox->SetSelectedItem(i);
        break;
      }
    }
  }
}

}  // namespace chromeos
