// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/system_page_view.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/combobox_model.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/options/language_config_util.h"
#include "chrome/browser/chromeos/options/language_config_view.h"
#include "chrome/browser/chromeos/options/options_window_view.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "unicode/timezone.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/slider/slider.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// DateTimeSection

// Date/Time section for datetime settings
class DateTimeSection : public SettingsPageSection,
                        public views::Combobox::Listener,
                        public SystemLibrary::Observer {
 public:
  explicit DateTimeSection(Profile* profile);
  virtual ~DateTimeSection();

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // Overridden from SystemLibrary::Observer:
  virtual void TimezoneChanged(const icu::TimeZone& timezone);

 protected:
  // SettingsPageSection overrides:
  virtual void InitContents(GridLayout* layout);

 private:
  // The combobox model for the list of timezones.
  class TimezoneComboboxModel : public ComboboxModel {
   public:
    TimezoneComboboxModel() {
      // TODO(chocobo): For now, add all the GMT timezones.
      // We may eventually want to use icu::TimeZone::createEnumeration()
      // to list all the timezones and pick the ones we want to show.
      // NOTE: This currently does not handle daylight savings properly
      // b/c this is just a manually selected list of timezones that
      // happen to span the GMT-11 to GMT+12 Today. When daylight savings
      // kick in, this list might have more than one timezone in the same
      // GMT bucket.
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Pacific/Samoa")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Hawaii")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Alaska")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Pacific")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Mountain")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Central")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Eastern")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("America/Santiago")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("America/Sao_Paulo")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Atlantic/South_Georgia")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Atlantic/Cape_Verde")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/London")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Rome")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Helsinki")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Moscow")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Dubai")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Karachi")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Dhaka")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Bangkok")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Hong_Kong")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Tokyo")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Australia/Sydney")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Magadan")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Pacific/Auckland")));
    }

    virtual ~TimezoneComboboxModel() {
      STLDeleteElements(&timezones_);
    }

    virtual int GetItemCount() {
      return static_cast<int>(timezones_.size());
    }

    virtual std::wstring GetItemAt(int index) {
      icu::UnicodeString name;
      timezones_[index]->getDisplayName(name);
      std::wstring output;
      UTF16ToWide(name.getBuffer(), name.length(), &output);
      int hour_offset = timezones_[index]->getRawOffset() / 3600000;
      return StringPrintf(hour_offset == 0 ? L"(GMT) " : (hour_offset > 0 ?
          L"(GMT+%d) " : L"(GMT%d) "), hour_offset) + output;
    }

    virtual icu::TimeZone* GetTimeZoneAt(int index) {
      return timezones_[index];
    }

   private:
    std::vector<icu::TimeZone*> timezones_;

    DISALLOW_COPY_AND_ASSIGN(TimezoneComboboxModel);
  };

  // TimeZone combobox model.
  views::Combobox* timezone_combobox_;

  // Controls for this section:
  TimezoneComboboxModel timezone_combobox_model_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeSection);
};

DateTimeSection::DateTimeSection(Profile* profile)
    : SettingsPageSection(profile, IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME),
      timezone_combobox_(NULL) {
  CrosLibrary::Get()->GetSystemLibrary()->AddObserver(this);
}

DateTimeSection::~DateTimeSection() {
  CrosLibrary::Get()->GetSystemLibrary()->RemoveObserver(this);
}

void DateTimeSection::ItemChanged(views::Combobox* sender,
                                  int prev_index,
                                  int new_index) {
  if (new_index == prev_index)
    return;

  CrosLibrary::Get()->GetSystemLibrary()->SetTimezone(
      timezone_combobox_model_.GetTimeZoneAt(new_index));
}

void DateTimeSection::TimezoneChanged(const icu::TimeZone& timezone) {
  for (int i = 0; i < timezone_combobox_model_.GetItemCount(); i++) {
    if (*timezone_combobox_model_.GetTimeZoneAt(i) == timezone) {
      timezone_combobox_->SetSelectedItem(i);
      return;
    }
  }
}

void DateTimeSection::InitContents(GridLayout* layout) {
  timezone_combobox_ = new views::Combobox(&timezone_combobox_model_);
  timezone_combobox_->set_listener(this);

  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION)));
  layout->AddView(timezone_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  TimezoneChanged(CrosLibrary::Get()->GetSystemLibrary()->GetTimezone());
}

////////////////////////////////////////////////////////////////////////////////
// TouchpadSection

class TouchpadSection : public SettingsPageSection,
                        public views::ButtonListener,
                        public views::SliderListener {
 public:
  explicit TouchpadSection(Profile* profile);
  virtual ~TouchpadSection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::SliderListener:
  virtual void SliderValueChanged(views::Slider* sender);

 protected:
  // SettingsPageSection overrides:
  virtual void InitContents(GridLayout* layout);
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // The View that contains the contents of the section.
  views::View* contents_;

  // Controls for this section:
  views::Checkbox* enable_tap_to_click_checkbox_;
  views::Checkbox* enable_vert_edge_scroll_checkbox_;
  views::Slider* speed_factor_slider_;
  views::Slider* sensitivity_slider_;

  // Preferences for this section:
  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember sensitivity_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadSection);
};

TouchpadSection::TouchpadSection(Profile* profile)
    : SettingsPageSection(profile, IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD),
      enable_tap_to_click_checkbox_(NULL),
      enable_vert_edge_scroll_checkbox_(NULL),
      speed_factor_slider_(NULL),
      sensitivity_slider_(NULL) {
}

void TouchpadSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == enable_tap_to_click_checkbox_) {
    bool enabled = enable_tap_to_click_checkbox_->checked();
    UserMetricsRecordAction(enabled ?
        UserMetricsAction("Options_TapToClickCheckbox_Enable") :
        UserMetricsAction("Options_TapToClickCheckbox_Disable"),
        profile()->GetPrefs());
    tap_to_click_enabled_.SetValue(enabled);
  } else if (sender == enable_vert_edge_scroll_checkbox_) {
    bool enabled = enable_vert_edge_scroll_checkbox_->checked();
    UserMetricsRecordAction(enabled ?
        UserMetricsAction("Options_VertEdgeScrollCheckbox_Enable") :
        UserMetricsAction("Options_VertEdgeScrollCheckbox_Disable"),
        profile()->GetPrefs());
    vert_edge_scroll_enabled_.SetValue(enabled);
  }
}

void TouchpadSection::SliderValueChanged(views::Slider* sender) {
  if (sender == speed_factor_slider_) {
    double value = speed_factor_slider_->value();
    UserMetricsRecordAction(
        UserMetricsAction("Options_SpeedFactorSlider_Changed"),
        profile()->GetPrefs());
    speed_factor_.SetValue(value);
  } else if (sender == sensitivity_slider_) {
    double value = sensitivity_slider_->value();
    UserMetricsRecordAction(
        UserMetricsAction("Options_SensitivitySlider_Changed"),
        profile()->GetPrefs());
    sensitivity_.SetValue(value);
  }
}

void TouchpadSection::InitContents(GridLayout* layout) {
  enable_tap_to_click_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  enable_tap_to_click_checkbox_->set_listener(this);
  enable_tap_to_click_checkbox_->SetMultiLine(true);
  enable_vert_edge_scroll_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_VERT_EDGE_SCROLL_ENABLED_DESCRIPTION));
  enable_vert_edge_scroll_checkbox_->set_listener(this);
  enable_vert_edge_scroll_checkbox_->SetMultiLine(true);
  // Create speed factor slider with values between 1 and 10 step 1
  speed_factor_slider_ = new views::Slider(1, 10, 1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_DRAW_VALUE |
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);
  // Create sensitivity slider with values between 1 and 10 step 1
  sensitivity_slider_ = new views::Slider(1, 10, 1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_DRAW_VALUE |
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);

  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION)));
  layout->AddView(sensitivity_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SPEED_FACTOR_DESCRIPTION)));
  layout->AddView(speed_factor_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(enable_tap_to_click_checkbox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(enable_vert_edge_scroll_checkbox_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled,
                             profile()->GetPrefs(), this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled,
                                 profile()->GetPrefs(), this);
  speed_factor_.Init(prefs::kTouchpadSpeedFactor,
                     profile()->GetPrefs(), this);
  sensitivity_.Init(prefs::kTouchpadSensitivity,
                    profile()->GetPrefs(), this);
}

void TouchpadSection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    bool enabled =  tap_to_click_enabled_.GetValue();
    enable_tap_to_click_checkbox_->SetChecked(enabled);
  }
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled) {
    bool enabled =  vert_edge_scroll_enabled_.GetValue();
    enable_vert_edge_scroll_checkbox_->SetChecked(enabled);
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSpeedFactor) {
    double value =  speed_factor_.GetValue();
    speed_factor_slider_->SetValue(value);
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    double value =  sensitivity_.GetValue();
    sensitivity_slider_->SetValue(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LanguageSection

// TextInput section for text input settings.
class LanguageSection : public SettingsPageSection,
                        public views::ButtonListener,
                        public views::Combobox::Listener,
                        public views::SliderListener {
 public:
  explicit LanguageSection(Profile* profile);
  virtual ~LanguageSection() {}

 private:
  enum ButtonTag {
    kCustomizeLanguagesButton,
    kEnableAutoRepeatButton,
  };
  // Overridden from SettingsPageSection:
  virtual void InitContents(GridLayout* layout);
  void NotifyPrefChanged(const std::string* pref_name);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // Overridden from views::SliderListener.
  virtual void SliderValueChanged(views::Slider* sender);

  IntegerPrefMember xkb_remap_search_key_pref_;
  IntegerPrefMember xkb_remap_control_key_pref_;
  IntegerPrefMember xkb_remap_alt_key_pref_;
  views::Combobox* xkb_modifier_combobox_;
  chromeos::LanguageComboboxModel<int> xkb_modifier_combobox_model_;

  BooleanPrefMember xkb_auto_repeat_pref_;
  views::Checkbox* xkb_auto_repeat_checkbox_;

  IntegerPrefMember xkb_auto_repeat_delay_pref_;
  views::Slider* xkb_auto_repeat_delay_slider_;
  IntegerPrefMember xkb_auto_repeat_interval_pref_;
  views::Slider* xkb_auto_repeat_interval_slider_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSection);
};

LanguageSection::LanguageSection(Profile* profile)
    : SettingsPageSection(profile,
                          IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE),
      xkb_modifier_combobox_(NULL),
      xkb_modifier_combobox_model_(
          &language_prefs::kXkbModifierMultipleChoicePrefs),
      xkb_auto_repeat_delay_slider_(NULL),
      xkb_auto_repeat_interval_slider_(NULL) {
  xkb_remap_search_key_pref_.Init(
      prefs::kLanguageXkbRemapSearchKeyTo, profile->GetPrefs(), this);
  xkb_remap_control_key_pref_.Init(
      prefs::kLanguageXkbRemapControlKeyTo, profile->GetPrefs(), this);
  xkb_remap_alt_key_pref_.Init(
      prefs::kLanguageXkbRemapAltKeyTo, profile->GetPrefs(), this);
  xkb_auto_repeat_pref_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, profile->GetPrefs(), this);
  xkb_auto_repeat_delay_pref_.Init(
      language_prefs::kXkbAutoRepeatDelayPref.pref_name,
      profile->GetPrefs(), this);
  xkb_auto_repeat_interval_pref_.Init(
      language_prefs::kXkbAutoRepeatIntervalPref.pref_name,
      profile->GetPrefs(), this);
}

void LanguageSection::InitContents(GridLayout* layout) {
  // Add the customize button and XKB combobox.
  layout->StartRow(0, double_column_view_set_id());
  views::NativeButton* customize_languages_button = new views::NativeButton(
      this,
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));
  customize_languages_button->set_tag(kCustomizeLanguagesButton);

  xkb_modifier_combobox_ = new views::Combobox(&xkb_modifier_combobox_model_);
  xkb_modifier_combobox_->set_listener(this);

  xkb_auto_repeat_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_ENABLED));
  xkb_auto_repeat_checkbox_->set_tag(kEnableAutoRepeatButton);
  xkb_auto_repeat_checkbox_->set_listener(this);

  xkb_auto_repeat_delay_slider_ = new views::Slider(
      language_prefs::kXkbAutoRepeatDelayPref.min_pref_value,
      language_prefs::kXkbAutoRepeatDelayPref.max_pref_value,
      1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);
  xkb_auto_repeat_interval_slider_ = new views::Slider(
      language_prefs::kXkbAutoRepeatIntervalPref.min_pref_value,
      language_prefs::kXkbAutoRepeatIntervalPref.max_pref_value,
      1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);

  // Initialize the combobox to what's saved in user preferences. Otherwise,
  // ItemChanged() will be called with |new_index| == 0.
  NotifyPrefChanged(NULL);

  layout->AddView(customize_languages_button, 1, 1,
                  GridLayout::LEADING, GridLayout::CENTER);
  layout->AddView(xkb_modifier_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(xkb_auto_repeat_checkbox_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, quad_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(
          language_prefs::kXkbAutoRepeatDelayPref.message_id)),
                  1, 1, GridLayout::LEADING, GridLayout::CENTER);
  layout->AddView(new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_DELAY_SHORT)));
  layout->AddView(xkb_auto_repeat_delay_slider_);
  layout->AddView(new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_DELAY_LONG)));

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, quad_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(
          language_prefs::kXkbAutoRepeatIntervalPref.message_id)),
                  1, 1, GridLayout::LEADING, GridLayout::CENTER);
  layout->AddView(new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_SPEED_FAST)));
  layout->AddView(xkb_auto_repeat_interval_slider_);
  layout->AddView(new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_REPEAT_SPEED_SLOW)));

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
}

void LanguageSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender->tag() == kCustomizeLanguagesButton) {
    LanguageConfigView::Show(profile(), GetOptionsViewParent());
  } else if (sender->tag() == kEnableAutoRepeatButton) {
    const bool enabled = xkb_auto_repeat_checkbox_->checked();
    xkb_auto_repeat_pref_.SetValue(enabled);
  }
}

void LanguageSection::ItemChanged(views::Combobox* sender,
                                  int prev_index,
                                  int new_index) {
  LOG(INFO) << "Changing XKB modofier pref to " << new_index;
  switch (new_index) {
    default:
      LOG(ERROR) << "Unexpected mapping: " << new_index;
      /* fall through */
    case language_prefs::kNoRemap:
      xkb_remap_search_key_pref_.SetValue(kSearchKey);
      xkb_remap_control_key_pref_.SetValue(kLeftControlKey);
      xkb_remap_alt_key_pref_.SetValue(kLeftAltKey);
      break;
    case language_prefs::kSwapCtrlAndAlt:
      xkb_remap_search_key_pref_.SetValue(kSearchKey);
      xkb_remap_control_key_pref_.SetValue(kLeftAltKey);
      xkb_remap_alt_key_pref_.SetValue(kLeftControlKey);
      break;
    case language_prefs::kSwapSearchAndCtrl:
      xkb_remap_search_key_pref_.SetValue(kLeftControlKey);
      xkb_remap_control_key_pref_.SetValue(kSearchKey);
      xkb_remap_alt_key_pref_.SetValue(kLeftAltKey);
      break;
  }
}

void LanguageSection::SliderValueChanged(views::Slider* sender) {
  if (xkb_auto_repeat_delay_slider_ == sender) {
    xkb_auto_repeat_delay_pref_.SetValue(sender->value());
  } else if (xkb_auto_repeat_interval_slider_ == sender) {
    xkb_auto_repeat_interval_pref_.SetValue(sender->value());
  }
}

void LanguageSection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || (*pref_name == prefs::kLanguageXkbRemapSearchKeyTo ||
                     *pref_name == prefs::kLanguageXkbRemapControlKeyTo ||
                     *pref_name == prefs::kLanguageXkbRemapAltKeyTo)) {
    const int search_remap = xkb_remap_search_key_pref_.GetValue();
    const int control_remap = xkb_remap_control_key_pref_.GetValue();
    const int alt_remap = xkb_remap_alt_key_pref_.GetValue();
    if ((search_remap == kSearchKey) &&
        (control_remap == kLeftControlKey) &&
        (alt_remap == kLeftAltKey)) {
      xkb_modifier_combobox_->SetSelectedItem(language_prefs::kNoRemap);
    } else if ((search_remap == kLeftControlKey) &&
               (control_remap == kSearchKey) &&
               (alt_remap == kLeftAltKey)) {
      xkb_modifier_combobox_->SetSelectedItem(
          language_prefs::kSwapSearchAndCtrl);
    } else if ((search_remap == kSearchKey) &&
               (control_remap == kLeftAltKey) &&
               (alt_remap == kLeftControlKey)) {
      xkb_modifier_combobox_->SetSelectedItem(language_prefs::kSwapCtrlAndAlt);
    } else {
      LOG(ERROR) << "Unexpected mapping. The prefs are updated by DOMUI?";
      xkb_modifier_combobox_->SetSelectedItem(language_prefs::kNoRemap);
    }
  }
  if (!pref_name || *pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    const bool enabled = xkb_auto_repeat_pref_.GetValue();
    xkb_auto_repeat_checkbox_->SetChecked(enabled);
  }
  if (!pref_name ||
      *pref_name == language_prefs::kXkbAutoRepeatDelayPref.pref_name) {
    const int delay_value = xkb_auto_repeat_delay_pref_.GetValue();
    xkb_auto_repeat_delay_slider_->SetValue(delay_value);
  }
  if (!pref_name ||
      *pref_name == language_prefs::kXkbAutoRepeatIntervalPref.pref_name) {
    const int interval_value = xkb_auto_repeat_interval_pref_.GetValue();
    xkb_auto_repeat_interval_slider_->SetValue(interval_value);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AccessibilitySection

// Checkbox for specifying if accessibility should be enabled for this profile
class AccessibilitySection : public SettingsPageSection,
                             public views::ButtonListener {
 public:
  explicit AccessibilitySection(Profile* profile);
  virtual ~AccessibilitySection() {}

 protected:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // Overridden from SettingsPageSection:
  virtual void InitContents(GridLayout* layout);
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // The View that contains the contents of the section.
  views::View* contents_;

  // Controls for this section:
  views::Checkbox* accessibility_checkbox_;

  // Preferences for this section:
  BooleanPrefMember accessibility_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilitySection);
};

AccessibilitySection::AccessibilitySection(Profile* profile)
    : SettingsPageSection(profile,
                          IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY),
      accessibility_checkbox_(NULL) {
}

void AccessibilitySection::InitContents(GridLayout* layout) {
  accessibility_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DESCRIPTION));
  accessibility_checkbox_->set_listener(this);
  accessibility_checkbox_->SetMultiLine(true);

  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(accessibility_checkbox_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  accessibility_enabled_.Init(prefs::kAccessibilityEnabled,
      profile()->GetPrefs(), this);
}

void AccessibilitySection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == accessibility_checkbox_) {
    bool enabled = accessibility_checkbox_->checked();
    // Set the accessibility enabled value in profile/prefs
    accessibility_enabled_.SetValue(enabled);
  }
}

void AccessibilitySection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kAccessibilityEnabled) {
    bool enabled = accessibility_enabled_.GetValue();
    accessibility_checkbox_->SetChecked(enabled);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemPageView

////////////////////////////////////////////////////////////////////////////////
// SystemPageView, SettingsPageView implementation:

void SystemPageView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new DateTimeSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new TouchpadSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new LanguageSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new AccessibilitySection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
