// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/options/language_chewing_config_view.h"
#include "chrome/browser/chromeos/options/language_hangul_config_view.h"
#include "chrome/browser/chromeos/options/language_mozc_config_view.h"
#include "chrome/browser/chromeos/options/language_pinyin_config_view.h"
#include "chrome/browser/chromeos/options/options_window_view.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/status/language_menu_l10n_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/fill_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {
using views::ColumnSet;
using views::GridLayout;

namespace {

// The code should be compatible with one of codes used for UI languages,
// defined in app/l10_util.cc.
const char kDefaultLanguageCode[] = "en-US";

// The list of language that do not have associated input methods. For
// these languages, we associate input methods here.
const struct ExtraLanguage {
  const char* language_code;
  const char* input_method_id;
} kExtraLanguages[] = {
  { "id", "xkb:us::eng" }, // For Indonesian, use US keyboard layout.
  // The code "fil" comes from app/l10_util.cc.
  { "fil", "xkb:us::eng" },  // For Filipino, use US keyboard layout.
  // The code "es-419" comes from app/l10_util.cc.
  // For Spanish in Latin America, use Spanish keyboard layout.
  { "es-419", "xkb:es::spa" },
};

// The list defines pairs of language code and the default input method
// id. The list is used for reordering input method ids.
//
// TODO(satorux): We may need to handle secondary, and ternary input
// methods, rather than handling the default input method only.
const struct LanguageDefaultInputMethodId {
  const char* language_code;
  const char* input_method_id;
} kLanguageDefaultInputMethodIds[] = {
  { "en-US", "xkb:us::eng", },  // US - English
  { "fr",    "xkb:fr::fra", },  // France - French
  { "de",    "xkb:de::ger", },  // Germany - German
};

// The width of the preferred language table shown on the left side.
const int kPreferredLanguageTableWidth = 300;

// Creates the LanguageHangulConfigView. The function is used to create
// the object via a function pointer. See also InitInputMethodConfigViewMap().
views::DialogDelegate* CreateLanguageChewingConfigView(Profile* profile) {
  return new LanguageChewingConfigView(profile);
}
views::DialogDelegate* CreateLanguageHangulConfigView(Profile* profile) {
  return new LanguageHangulConfigView(profile);
}
views::DialogDelegate* CreateLanguagePinyinConfigView(Profile* profile) {
  return new LanguagePinyinConfigView(profile);
}
views::DialogDelegate* CreateLanguageMozcConfigView(Profile* profile) {
  return new LanguageMozcConfigView(profile);
}

// The tags are used to identify buttons in ButtonPressed().
enum ButtonTag {
  kChangeUiLanguageButton,
  kConfigureInputMethodButton,
  kRemoveLanguageButton,
  kSelectInputMethodButton,
};

// The column set IDs are used for creating the per-language config view.
const int kPerLanguageTitleColumnSetId = 1;
const int kPerLanguageDoubleColumnSetId = 2;
const int kPerLanguageSingleColumnSetId = 3;

}  // namespace

AddLanguageComboboxModel::AddLanguageComboboxModel(
    Profile* profile,
    const std::vector<std::string>& locale_codes)
    : LanguageComboboxModel(profile, locale_codes) {
}

int AddLanguageComboboxModel::GetItemCount() {
  // +1 for "Add language".
  return get_languages_count() + 1 - ignore_set_.size();
}

std::wstring AddLanguageComboboxModel::GetItemAt(int index) {
  // Show "Add language" as the first item.
  if (index == 0) {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_COMBOBOX);
  }
  return LanguageConfigModel::MaybeRewriteLanguageName(
      GetLanguageNameAt(GetLanguageIndex(index)));
}

int AddLanguageComboboxModel::GetLanguageIndex(int index) const {
  // The adjusted_index is counted while ignoring languages in ignore_set_.
  int adjusted_index = 0;
  for (int i = 0; i < get_languages_count(); ++i) {
    if (ignore_set_.count(GetLocaleFromIndex(i)) > 0) {
      continue;
    }
    // -1 for "Add language".
    if (adjusted_index == index - 1) {
      return i;
    }
    ++adjusted_index;
  }
  return 0;
}

void AddLanguageComboboxModel::SetIgnored(
    const std::string& language_code, bool ignored) {
  if (ignored) {
    // Add to the ignore_set_ if the language code is known (i.e. reject
    // unknown language codes just in case).
    if (GetIndexFromLocale(language_code) != -1) {
      ignore_set_.insert(language_code);
    } else {
      LOG(ERROR) << "Unknown language code: " << language_code;
    }
  } else {
    ignore_set_.erase(language_code);
  }
}

// This is a native button associated with input method information.
class InputMethodButton : public views::NativeButton {
 public:
  InputMethodButton(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::string& input_method_id)
      : views::NativeButton(listener, label),
        input_method_id_(input_method_id) {
  }

  const std::string& input_method_id() const {
    return input_method_id_;
  }

 private:
  std::string input_method_id_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodButton);
};

// This is a native button associated with UI language information.
class UiLanguageButton : public views::NativeButton {
 public:
  UiLanguageButton(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::string& language_code)
      : views::NativeButton(listener, label),
        language_code_(language_code) {
  }

  const std::string& language_code() const {
    return language_code_;
  }

 private:
  std::string language_code_;
  DISALLOW_COPY_AND_ASSIGN(UiLanguageButton);
};

// This is a checkbox button associated with input method information.
class InputMethodCheckbox : public views::Checkbox {
 public:
  InputMethodCheckbox(const std::wstring& display_name,
                      const std::string& input_method_id)
      : views::Checkbox(display_name),
        input_method_id_(input_method_id) {
  }

  const std::string& input_method_id() const {
    return input_method_id_;
  }

 private:
  std::string input_method_id_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodCheckbox);
};

LanguageConfigModel::LanguageConfigModel(PrefService* pref_service)
    : pref_service_(pref_service) {
}

void LanguageConfigModel::Init() {
  // Initialize the maps and vectors.
  InitInputMethodIdMapsAndVectors();

  preload_engines_.Init(
      prefs::kLanguagePreloadEngines, pref_service_, this);
  // TODO(yusukes): It might be safer to call GetActiveLanguages() cros API
  // here and compare the result and preload_engines_.GetValue(). If there's
  // a discrepancy between IBus setting and Chrome prefs, we can resolve it
  // by calling preload_engines_SetValue() here.
}

size_t LanguageConfigModel::CountNumActiveInputMethods(
    const std::string& language_code) {
  int num_selected_active_input_methods = 0;
  std::pair<LanguageCodeToIdsMap::const_iterator,
            LanguageCodeToIdsMap::const_iterator> range =
      language_code_to_ids_map_.equal_range(language_code);
  for (LanguageCodeToIdsMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    if (InputMethodIsActivated(iter->second)) {
      ++num_selected_active_input_methods;
    }
  }
  return num_selected_active_input_methods;
}

bool LanguageConfigModel::HasLanguageCode(
    const std::string& language_code) const {
  return std::find(preferred_language_codes_.begin(),
                   preferred_language_codes_.end(),
                   language_code) != preferred_language_codes_.end();
}

size_t LanguageConfigModel::AddLanguageCode(
    const std::string& language_code) {
  preferred_language_codes_.push_back(language_code);
  // Sort the language codes by names. This is not efficient, but
  // acceptable as the language list is about 40 item long at most.  In
  // theory, we could find the position to insert rather than sorting, but
  // it would be complex as we need to use unicode string comparator.
  SortLanguageCodesByNames(&preferred_language_codes_);
  // Find the language code just added in the sorted language codes.
  const int added_at =
      std::distance(preferred_language_codes_.begin(),
                    std::find(preferred_language_codes_.begin(),
                              preferred_language_codes_.end(),
                              language_code));
  return added_at;
}

void LanguageConfigModel::RemoveLanguageAt(size_t row) {
  preferred_language_codes_.erase(preferred_language_codes_.begin() + row);
}

void LanguageConfigModel::UpdateInputMethodPreferences(
    const std::vector<std::string>& in_new_input_method_ids) {
  std::vector<std::string> new_input_method_ids = in_new_input_method_ids;
  // Note: Since |new_input_method_ids| is alphabetically sorted and the sort
  // function below uses stable sort, the relateve order of input methods that
  // belong to the same language (e.g. "mozc" and "xkb:jp::jpn") is maintained.
  SortInputMethodIdsByNames(id_to_language_code_map_, &new_input_method_ids);
  preload_engines_.SetValue(UTF8ToWide(JoinString(new_input_method_ids, ',')));
}

LanguageConfigView::LanguageConfigView(Profile* profile)
    : OptionsPageView(profile),
      model(profile->GetPrefs()),
      root_container_(NULL),
      right_container_(NULL),
      remove_language_button_(NULL),
      preferred_language_table_(NULL) {
}

LanguageConfigView::~LanguageConfigView() {
}

void LanguageConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender->tag() == kRemoveLanguageButton) {
    OnRemoveLanguage();
  } else if (sender->tag() == kSelectInputMethodButton) {
    InputMethodCheckbox* checkbox =
        static_cast<InputMethodCheckbox*>(sender);
    const std::string& input_method_id = checkbox->input_method_id();
    model.SetInputMethodActivated(input_method_id, checkbox->checked());
    if (checkbox->checked()) {
      EnableAllCheckboxes();
    } else {
      MaybeDisableLastCheckbox();
    }
  } else if (sender->tag() == kConfigureInputMethodButton) {
    InputMethodButton* button = static_cast<InputMethodButton*>(sender);
    views::DialogDelegate* config_view =
        CreateInputMethodConfigureView(button->input_method_id());
    if (!config_view) {
      DLOG(FATAL) << "Config view not found: " << button->input_method_id();
      return;
    }
    views::Window* window = views::Window::CreateChromeWindow(
        GetOptionsViewParent(), gfx::Rect(), config_view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else if (sender->tag() == kChangeUiLanguageButton) {
    UiLanguageButton* button = static_cast<UiLanguageButton*>(sender);
    PrefService* prefs = g_browser_process->local_state();
    if (prefs) {
      prefs->SetString(prefs::kApplicationLocale,
                       UTF8ToWide(button->language_code()));
      prefs->SavePersistentPrefs();
      RestartMessageBox::ShowMessageBox(GetWindow()->GetNativeWindow());
    }
  }
}

void LanguageConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  root_container_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_DONE);
  }
  return L"";
}

std::wstring LanguageConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
}

gfx::Size LanguageConfigView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES));
}

views::View* LanguageConfigView::CreatePerLanguageConfigView(
    const std::string& target_language_code) {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  ColumnSet* column_set = layout->AddColumnSet(kPerLanguageTitleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(kPerLanguageDoubleColumnSetId);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(kPerLanguageSingleColumnSetId);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  AddUiLanguageSection(target_language_code, layout);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  AddInputMethodSection(target_language_code, layout);

  return contents;
}

void LanguageConfigView::AddUiLanguageSection(const std::string& language_code,
                                              views::GridLayout* layout) {
  // Create the language name label.
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  const string16 language_name16 = l10n_util::GetDisplayNameForLocale(
      language_code, application_locale, true);
  const std::wstring language_name
      = LanguageConfigModel::MaybeRewriteLanguageName(
          UTF16ToWide(language_name16));
  views::Label* language_name_label = new views::Label(language_name);
  language_name_label->SetFont(
      language_name_label->font().DeriveFont(0, gfx::Font::BOLD));

  // Add the language name label.
  layout->StartRow(0, kPerLanguageTitleColumnSetId);
  layout->AddView(language_name_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kPerLanguageSingleColumnSetId);
  if (application_locale == language_code) {
    layout->AddView(
        new views::Label(
            l10n_util::GetStringF(
                IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
                l10n_util::GetString(IDS_PRODUCT_OS_NAME))));
  } else {
    UiLanguageButton* button = new UiLanguageButton(
      this, l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)),
      language_code);
    button->set_tag(kChangeUiLanguageButton);
    layout->AddView(button);
  }
}

void LanguageConfigView::AddInputMethodSection(
    const std::string& language_code,
    views::GridLayout* layout) {
  // Create the input method title label.
  views::Label* input_method_title_label = new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  input_method_title_label->SetFont(
      input_method_title_label->font().DeriveFont(0, gfx::Font::BOLD));

  // Add the input method title label.
  layout->StartRow(0, kPerLanguageTitleColumnSetId);
  layout->AddView(input_method_title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add input method names and configuration buttons.
  input_method_checkboxes_.clear();

  // Get the list of input method ids associated with the language code.
  std::vector<std::string> input_method_ids;
  model.GetInputMethodIdsFromLanguageCode(language_code, &input_method_ids);

  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string& input_method_id = input_method_ids[i];
    const std::string display_name = model.GetInputMethodDisplayNameFromId(
        input_method_id);
    layout->StartRow(0, kPerLanguageDoubleColumnSetId);
    InputMethodCheckbox* checkbox
        = new InputMethodCheckbox(UTF8ToWide(display_name),
                                  input_method_id);
    checkbox->set_listener(this);
    checkbox->set_tag(kSelectInputMethodButton);
    if (model.InputMethodIsActivated(input_method_id)) {
      checkbox->SetChecked(true);
    }

    layout->AddView(checkbox);
    input_method_checkboxes_.insert(checkbox);
    // Add "configure" button for the input method if we have a
    // configuration dialog for it.
    if (input_method_config_view_map_.count(input_method_id) > 0) {
      InputMethodButton* button = new InputMethodButton(
          this,
          l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE),
          input_method_id);
      button->set_tag(kConfigureInputMethodButton);
      layout->AddView(button);
    }
  }
}

void LanguageConfigView::OnSelectionChanged() {
  right_container_->RemoveAllChildViews(true);  // Delete the child views.

  const int row = preferred_language_table_->GetFirstSelectedRow();
  const std::string& language_code = model.preferred_language_code_at(row);

  // Count the number of all active input methods.
  std::vector<std::string> active_input_method_ids;
  model.GetActiveInputMethodIds(&active_input_method_ids);
  const int num_all_active_input_methods = active_input_method_ids.size();

  // Count the number of active input methods for the selected language.
  int num_selected_active_input_methods =
      model.CountNumActiveInputMethods(language_code);

  bool remove_button_enabled = false;
  // Allow removing the language only if the following conditions are met:
  // 1. There are more than one language.
  // 2. The languge in the current row is not set to the display language.
  // 3. Removing the selected language does not result in "zero input method".
  if (preferred_language_table_->GetRowCount() > 1 &&
      language_code != g_browser_process->GetApplicationLocale() &&
      num_all_active_input_methods > num_selected_active_input_methods) {
    remove_button_enabled = true;
  }
  remove_language_button_->SetEnabled(remove_button_enabled);

  // Add the per language config view to the right area.
  right_container_->AddChildView(CreatePerLanguageConfigView(language_code));
  MaybeDisableLastCheckbox();
  // Let the parent container layout again. This is needed to the the
  // contents on the right to display.
  root_container_->Layout();
}

std::wstring LanguageConfigView::GetText(int row, int column_id) {
  if (row >= 0 && row < static_cast<int>(
          model.num_preferred_language_codes())) {
    return LanguageConfigModel::
        GetLanguageDisplayNameFromCode(model.preferred_language_code_at(row));
  }
  NOTREACHED();
  return L"";
}

void LanguageConfigView::Show(Profile* profile, gfx::NativeWindow parent) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageConfigView_Open"));
  views::Window* window = views::Window::CreateChromeWindow(
      parent, gfx::Rect(), new LanguageConfigView(profile));
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void LanguageConfigView::SetObserver(TableModelObserver* observer) {
  // We don't need the observer for the table mode, since we implement the
  // table model as part of the LanguageConfigView class.
  // http://crbug.com/38266
}

int LanguageConfigView::RowCount() {
  // Returns the number of rows of the language table.
  return model.num_preferred_language_codes();
}

void LanguageConfigView::InitControlLayout() {
  // Initialize the model.
  model.Init();
  // Initialize the map.
  InitInputMethodConfigViewMap();

  root_container_ = new views::View;
  AddChildView(root_container_);

  // Set up the layout manager for the root container.  We'll place the
  // language table on the left, and the per language config on the right.
  GridLayout* root_layout = new GridLayout(root_container_);
  root_container_->SetLayoutManager(root_layout);
  root_layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                         kPanelVertMargin, kPanelHorizMargin);

  // Set up column sets for the grid layout.
  const int kMainColumnSetId = 0;
  ColumnSet* column_set = root_layout->AddColumnSet(kMainColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::FIXED, kPreferredLanguageTableWidth, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::USE_PREF, 0, 0);
  const int kBottomColumnSetId = 1;
  column_set = root_layout->AddColumnSet(kBottomColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Initialize the language codes currently activated.
  model.NotifyPrefChanged();

  // Set up the container for the contents on the right.  Just adds a
  // place holder here. This will get replaced in OnSelectionChanged().
  right_container_ = new views::View;
  right_container_->SetLayoutManager(new views::FillLayout);
  right_container_->AddChildView(new views::View);

  // Add the contents on the left and the right.
  root_layout->StartRow(1 /* expand */, kMainColumnSetId);
  root_layout->AddView(CreateContentsOnLeft());
  root_layout->AddView(right_container_);

  // Add the contents on the bottom.
  root_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  root_layout->StartRow(0, kBottomColumnSetId);
  root_layout->AddView(CreateContentsOnBottom());

  // Select the first row in the language table.
  // There should be at least one language in the table, but we check it
  // here so this won't result in crash in case there is no row in the table.
  if (model.num_preferred_language_codes() > 0) {
    preferred_language_table_->SelectRow(0);
  }
}

void LanguageConfigView::InitInputMethodConfigViewMap() {
  input_method_config_view_map_["chewing"] = CreateLanguageChewingConfigView;
  input_method_config_view_map_["hangul"] = CreateLanguageHangulConfigView;
  input_method_config_view_map_["pinyin"] = CreateLanguagePinyinConfigView;

  // TODO(yusukes): Enable the following two mappings when ibus-mozc starts
  //                supporting IBus style configs.
  // input_method_config_view_map_["mozc"] = CreateLanguageMozcConfigView;
  // input_method_config_view_map_["mozc-jp"] = CreateLanguageMozcConfigView;
}

void LanguageConfigModel::InitInputMethodIdMapsAndVectors() {
  // The two sets are used to build lists without duplication.
  std::set<std::string> supported_language_code_set;
  std::set<std::string> supported_input_method_id_set;
  // Build the id to descriptor map for handling kExtraLanguages later.
  std::map<std::string, const InputMethodDescriptor*> id_to_descriptor_map;

  // GetSupportedLanguages() never return NULL.
  scoped_ptr<InputMethodDescriptors> supported_input_methods(
      CrosLibrary::Get()->GetLanguageLibrary()->GetSupportedInputMethods());
  for (size_t i = 0; i < supported_input_methods->size(); ++i) {
    const InputMethodDescriptor& input_method = supported_input_methods->at(i);
    const std::string language_code =
        LanguageLibrary::GetLanguageCodeFromDescriptor(input_method);
    AddInputMethodToMaps(language_code, input_method);
    // Add the language code and the input method id to the sets.
    supported_language_code_set.insert(language_code);
    supported_input_method_id_set.insert(input_method.id);
    // Remember the pair.
    id_to_descriptor_map.insert(
        std::make_pair(input_method.id, &input_method));
  }

  // Go through the languages listed in kExtraLanguages.
  for (size_t i = 0; i < arraysize(kExtraLanguages); ++i) {
    const char* language_code = kExtraLanguages[i].language_code;
    const char* input_method_id = kExtraLanguages[i].input_method_id;
    std::map<std::string, const InputMethodDescriptor*>::const_iterator iter =
        id_to_descriptor_map.find(input_method_id);
    // If the associated input method descriptor is found, add the
    // language code and the input method.
    if (iter != id_to_descriptor_map.end()) {
      const InputMethodDescriptor& input_method = *(iter->second);
      AddInputMethodToMaps(language_code, input_method);
      // Add the language code and the input method id to the sets.
      supported_language_code_set.insert(language_code);
      supported_input_method_id_set.insert(input_method.id);
    }
  }

  // Build the vectors from the sets.
  supported_language_codes_.assign(supported_language_code_set.begin(),
                                   supported_language_code_set.end());
  supported_input_method_ids_.assign(supported_input_method_id_set.begin(),
                                     supported_input_method_id_set.end());
}

void LanguageConfigModel::AddInputMethodToMaps(
    const std::string& language_code,
    const InputMethodDescriptor& input_method) {
  id_to_language_code_map_.insert(
      std::make_pair(input_method.id, language_code));
  id_to_display_name_map_.insert(
      std::make_pair(input_method.id, LanguageMenuL10nUtil::GetStringUTF8(
          input_method.display_name)));
  language_code_to_ids_map_.insert(
      std::make_pair(language_code, input_method.id));
}

views::View* LanguageConfigView::CreateContentsOnLeft() {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kTableColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kTableColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Create the language table.
  std::vector<TableColumn> columns;
  TableColumn column(0,
                     l10n_util::GetString(
                         IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES),
                     TableColumn::LEFT, -1, 0);
  columns.push_back(column);
  // We don't show horizontal and vertical lines.
  const int options = (views::TableView2::SINGLE_SELECTION |
                       views::TableView2::RESIZABLE_COLUMNS |
                       views::TableView2::AUTOSIZE_COLUMNS);
  preferred_language_table_ =
      new views::TableView2(this, columns, views::TEXT_ONLY, options);
  // Set the observer so OnSelectionChanged() will be invoked when a
  // selection is changed in the table.
  preferred_language_table_->SetObserver(this);

  // Add the language table.
  layout->StartRow(1 /* expand vertically */, kTableColumnSetId);
  layout->AddView(preferred_language_table_);

  return contents;
}

views::View* LanguageConfigView::CreateContentsOnBottom() {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kButtonsColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kButtonsColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Create the add language combobox model.
  // LanguageComboboxModel sorts languages by their display names.
  add_language_combobox_model_.reset(
      new AddLanguageComboboxModel(NULL, model.supported_language_codes()));
  // Mark the existing preferred languages to be ignored.
  for (size_t i = 0; i < model.num_preferred_language_codes(); ++i) {
    add_language_combobox_model_->SetIgnored(
        model.preferred_language_code_at(i),
        true);
  }
  // Create the add language combobox.
  add_language_combobox_
      = new views::Combobox(add_language_combobox_model_.get());
  add_language_combobox_->set_listener(this);
  ResetAddLanguageCombobox();

  // Create the remove button.
  remove_language_button_ = new views::NativeButton(
      this, l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  remove_language_button_->set_tag(kRemoveLanguageButton);

  // Add the add and remove buttons.
  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(add_language_combobox_);
  layout->AddView(remove_language_button_);

  return contents;
}

void LanguageConfigView::OnAddLanguage(const std::string& language_code) {
  // Skip if the language is already in the preferred_language_codes_.
  if (model.HasLanguageCode(language_code)) {
    return;
  }
  // Activate the first input language associated with the language. We have
  // to call this before the OnItemsAdded() call below so the checkbox
  // for the first input language gets checked.
  std::vector<std::string> input_method_ids;
  model.GetInputMethodIdsFromLanguageCode(language_code, &input_method_ids);
  if (!input_method_ids.empty()) {
    model.SetInputMethodActivated(input_method_ids[0], true);
  }

  // Append the language to the list of language codes.
  const int added_at = model.AddLanguageCode(language_code);
  // Notify the table that the new row added at |added_at|.
  preferred_language_table_->OnItemsAdded(added_at, 1);
  // For some reason, OnItemsAdded() alone does not redraw the table. Need
  // to tell the table that items are changed. TODO(satorux): Investigate
  // if it's a bug in TableView2.
  preferred_language_table_->OnItemsChanged(
      0, model.num_preferred_language_codes());
  // Switch to the row added.
  preferred_language_table_->SelectRow(added_at);

  // Mark the language to be ignored.
  add_language_combobox_model_->SetIgnored(language_code, true);
  ResetAddLanguageCombobox();
}

void LanguageConfigView::OnRemoveLanguage() {
  const int row = preferred_language_table_->GetFirstSelectedRow();
  const std::string& language_code = model.preferred_language_code_at(row);
  // Mark the language not to be ignored.
  add_language_combobox_model_->SetIgnored(language_code, false);
  ResetAddLanguageCombobox();
  // Deactivate the associated input methods.
  model.DeactivateInputMethodsFor(language_code);
  // Remove the language code and the row from the table.
  model.RemoveLanguageAt(row);
  preferred_language_table_->OnItemsRemoved(row, 1);
  // Switch to the previous row, or the first row.
  // There should be at least one row in the table.
  preferred_language_table_->SelectRow(std::max(row - 1, 0));
}

void LanguageConfigView::ResetAddLanguageCombobox() {
  // -1 to ignore "Add language". If there are more than one language,
  // enable the combobox. Otherwise, disable it.
  if (add_language_combobox_model_->GetItemCount() - 1 > 0) {
    add_language_combobox_->SetEnabled(true);
  } else {
    add_language_combobox_->SetEnabled(false);
  }
  // Go back to the initial "Add language" state.
  add_language_combobox_->ModelChanged();
  add_language_combobox_->SetSelectedItem(0);
}

void LanguageConfigModel::DeactivateInputMethodsFor(
    const std::string& language_code) {
  for (size_t i = 0; i < num_supported_input_method_ids(); ++i) {
    if (GetLanguageCodeFromInputMethodId(
            supported_input_method_id_at(i)) ==
        language_code) {
      // What happens if we disable the input method currently active?
      // IBus should take care of it, so we don't do anything special
      // here. See crosbug.com/2443.
      SetInputMethodActivated(supported_input_method_id_at(i), false);
      // Do not break; here in order to disable all engines that belong to
      // |language_code|.
    }
  }
}

views::DialogDelegate* LanguageConfigView::CreateInputMethodConfigureView(
    const std::string& input_method_id) {
  InputMethodConfigViewMap::const_iterator iter =
      input_method_config_view_map_.find(input_method_id);
  if (iter != input_method_config_view_map_.end()) {
    CreateDialogDelegateFunction function = iter->second;
    return function(profile());
  }
  return NULL;
}

void LanguageConfigModel::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguageConfigView::ItemChanged(views::Combobox* combobox,
                                     int prev_index,
                                     int new_index) {
  // Ignore the first item used for showing "Add language".
  if (new_index <= 0) {
    return;
  }
  // Get the language selected.
  std::string language_selected = add_language_combobox_model_->
      GetLocaleFromIndex(
          add_language_combobox_model_->GetLanguageIndex(new_index));
  OnAddLanguage(language_selected);
}

void LanguageConfigModel::SetInputMethodActivated(
    const std::string& input_method_id, bool activated) {
  DCHECK(!input_method_id.empty());
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> input_method_id_set(input_method_ids.begin(),
                                            input_method_ids.end());
  if (activated) {
    // Add |id| if it's not already added.
    input_method_id_set.insert(input_method_id);
  } else {
    input_method_id_set.erase(input_method_id);
  }

  // Update Chrome's preference.
  std::vector<std::string> new_input_method_ids(input_method_id_set.begin(),
                                                input_method_id_set.end());
  UpdateInputMethodPreferences(new_input_method_ids);
}

bool LanguageConfigModel::InputMethodIsActivated(
    const std::string& input_method_id) {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);
  return (std::find(input_method_ids.begin(), input_method_ids.end(),
                    input_method_id) != input_method_ids.end());
}

void LanguageConfigModel::GetActiveInputMethodIds(
    std::vector<std::string>* out_input_method_ids) {
  const std::wstring value = preload_engines_.GetValue();
  out_input_method_ids->clear();
  if (!value.empty()) {
    SplitString(WideToUTF8(value), ',', out_input_method_ids);
  }
}

void LanguageConfigView::MaybeDisableLastCheckbox() {
  std::vector<std::string> input_method_ids;
  model.GetActiveInputMethodIds(&input_method_ids);
  if (input_method_ids.size() <= 1) {
    for (std::set<InputMethodCheckbox*>::iterator checkbox =
             input_method_checkboxes_.begin();
         checkbox != input_method_checkboxes_.end(); ++checkbox) {
      if ((*checkbox)->checked())
        (*checkbox)->SetEnabled(false);
    }
  }
}

void LanguageConfigView::EnableAllCheckboxes() {
  for (std::set<InputMethodCheckbox*>::iterator checkbox =
           input_method_checkboxes_.begin();
       checkbox != input_method_checkboxes_.end(); ++checkbox) {
    (*checkbox)->SetEnabled(true);
  }
}

std::string LanguageConfigModel::GetLanguageCodeFromInputMethodId(
    const std::string& input_method_id) const {
  std::map<std::string, std::string>::const_iterator iter
      = id_to_language_code_map_.find(input_method_id);
  return (iter == id_to_language_code_map_.end()) ?
      // Returning |kDefaultLanguageCode| is not for Chrome OS but for Ubuntu
      // where the ibus-xkb-layouts module could be missing.
      kDefaultLanguageCode : iter->second;
}

std::string LanguageConfigModel::GetInputMethodDisplayNameFromId(
    const std::string& input_method_id) const {
  // |kDefaultDisplayName| is not for Chrome OS. See the comment above.
  static const char kDefaultDisplayName[] = "USA";
  std::map<std::string, std::string>::const_iterator iter
      = id_to_display_name_map_.find(input_method_id);
  return (iter == id_to_display_name_map_.end()) ?
      kDefaultDisplayName : iter->second;
}

void LanguageConfigModel::GetInputMethodIdsFromLanguageCode(
    const std::string& language_code,
    std::vector<std::string>* input_method_ids) const {
  DCHECK(input_method_ids);
  input_method_ids->clear();

  std::pair<LanguageCodeToIdsMap::const_iterator,
            LanguageCodeToIdsMap::const_iterator> range =
      language_code_to_ids_map_.equal_range(language_code);
  for (LanguageCodeToIdsMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    input_method_ids->push_back(iter->second);
  }
  // Reorder the input methods.
  ReorderInputMethodIdsForLanguageCode(language_code, input_method_ids);
}

void LanguageConfigModel::NotifyPrefChanged() {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> language_code_set;
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string language_code =
        GetLanguageCodeFromInputMethodId(input_method_ids[i]);
    language_code_set.insert(language_code);
  }

  preferred_language_codes_.clear();
  preferred_language_codes_.assign(
      language_code_set.begin(), language_code_set.end());
  LanguageConfigModel::SortLanguageCodesByNames(&preferred_language_codes_);
}

std::wstring LanguageConfigModel::MaybeRewriteLanguageName(
    const std::wstring& language_name) {
  // "t" is used as the language code for input methods that don't fall
  // under any other languages.
  if (language_name == L"t") {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS);
  }
  return language_name;
}

std::wstring LanguageConfigModel::GetLanguageDisplayNameFromCode(
    const std::string& language_code) {
  return MaybeRewriteLanguageName(UTF16ToWide(
      l10n_util::GetDisplayNameForLocale(
          language_code, g_browser_process->GetApplicationLocale(),
          true)));
}

namespace {

// The comparator is used for sorting language codes by their
// corresponding language names, using the ICU collator.
struct CompareLanguageCodesByLanguageName
    : std::binary_function<const std::string&, const std::string&, bool> {
  explicit CompareLanguageCodesByLanguageName(icu::Collator* collator)
      : collator_(collator) {
  }

  // Calling GetLanguageDisplayNameFromCode() in the comparator is not
  // efficient, but acceptable as the function is cheap, and the language
  // list is short (about 40 at most).
  bool operator()(const std::string& s1, const std::string& s2) const {
    const std::wstring key1 =
            LanguageConfigModel::GetLanguageDisplayNameFromCode(s1);
    const std::wstring key2 =
            LanguageConfigModel::GetLanguageDisplayNameFromCode(s2);
    return l10n_util::StringComparator<std::wstring>(collator_)(key1, key2);
  }

  icu::Collator* collator_;
};

// The comparator is used for sorting input method ids by their
// corresponding language names, using the ICU collator.
struct CompareInputMethodIdsByLanguageName
    : std::binary_function<const std::string&, const std::string&, bool> {
  CompareInputMethodIdsByLanguageName(
      icu::Collator* collator,
      const std::map<std::string, std::string>& id_to_language_code_map)
      : comparator_(collator),
        id_to_language_code_map_(id_to_language_code_map) {
  }

  bool operator()(const std::string& s1, const std::string& s2) const {
    std::string language_code_1;
    std::map<std::string, std::string>::const_iterator iter =
        id_to_language_code_map_.find(s1);
    if (iter != id_to_language_code_map_.end()) {
      language_code_1 = iter->second;
    }
    std::string language_code_2;
    iter = id_to_language_code_map_.find(s2);
    if (iter != id_to_language_code_map_.end()) {
      language_code_2 = iter->second;
    }
    return comparator_(language_code_1, language_code_2);
  }

  const CompareLanguageCodesByLanguageName comparator_;
  const std::map<std::string, std::string>& id_to_language_code_map_;
};

}  // namespace

void LanguageConfigModel::SortLanguageCodesByNames(
    std::vector<std::string>* language_codes) {
  // We should build collator outside of the comparator. We cannot have
  // scoped_ptr<> in the comparator for a subtle STL reason.
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  std::sort(language_codes->begin(), language_codes->end(),
            CompareLanguageCodesByLanguageName(collator.get()));
}

void LanguageConfigModel::SortInputMethodIdsByNames(
    const std::map<std::string, std::string>& id_to_language_code_map,
    std::vector<std::string>* input_method_ids) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  std::stable_sort(input_method_ids->begin(), input_method_ids->end(),
                   CompareInputMethodIdsByLanguageName(
                       collator.get(), id_to_language_code_map));
}

void LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
    const std::string& language_code,
    std::vector<std::string>* input_method_ids) {
  for (size_t i = 0; i < arraysize(kLanguageDefaultInputMethodIds); ++i) {
    if (language_code == kLanguageDefaultInputMethodIds[i].language_code) {
      std::vector<std::string>::iterator iter =
          std::find(input_method_ids->begin(), input_method_ids->end(),
                    kLanguageDefaultInputMethodIds[i].input_method_id);
      // If it's not on the top of |input_method_id|, swap it with the top one.
      if (iter != input_method_ids->end() &&
          iter != input_method_ids->begin()) {
        std::swap(*input_method_ids->begin(), *iter);
      }
      break;  // Don't have to check other language codes.
    }
  }
}

}  // namespace chromeos
