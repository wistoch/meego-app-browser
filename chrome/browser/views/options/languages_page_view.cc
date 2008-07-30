// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/views/options/languages_page_view.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/skia_utils.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/options/language_combobox_model.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_button.h"
#include "chrome/views/radio_button.h"
#include "chrome/views/tabbed_pane.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view_container.h"
#include "generated_resources.h"
#include "skia/include/SkBitmap.h"
#include "unicode/uloc.h"

static const wchar_t* const g_supported_spellchecker_languages[] = {
  L"en-US",
  L"en-GB",
  L"fr-FR",
  L"it-IT",
  L"de-DE",
  L"es-ES",
  L"nl-NL",
  L"pt-BR",
  L"ru-RU",
  L"pl-PL",
  // L"th-TH",  // Not to be included in Spellchecker as per B=1277824
  L"sv-SE",
  L"da-DK",
  L"pt-PT",
  L"ro-RO",
  // L"hu-HU",  // Not to be included in Spellchecker as per B=1277824
  // L"he-IL",  // Not to be included in Spellchecker as per B=1252241
  L"id-ID",
  L"cs-CZ",
  L"el-GR",
  L"nb-NO",
  L"vi-VN",
  // L"bg-BG",  // Not to be included in Spellchecker as per B=1277824
  L"hr-HR",
  L"lt-LT",
  L"sk-SK",
  L"sl-SI",
  L"ca-ES"
  L"lv-LV",
  // L"uk-UA",  // Not to be included in Spellchecker as per B=1277824
  L"hi-IN",
  //
  // TODO(Sidchat): Uncomment/remove languages as and when they get resolved.
  //
};

static const wchar_t* const accept_language_list[] = {
  L"af",     // Afrikaans
  L"am",     // Amharic
  L"ar",     // Arabic
  L"az",     // Azerbaijani
  L"be",     // Belarusian
  L"bg",     // Bulgarian
  L"bh",     // Bihari
  L"bn",     // Bengali
  L"br",     // Breton
  L"bs",     // Bosnian
  L"ca",     // Catalan
  L"co",     // Corsican
  L"cs",     // Czech
  L"cy",     // Welsh
  L"da",     // Danish
  L"de",     // German
  L"el",     // Greek
  L"en",     // English
  L"en-GB",  // English (UK)
  L"en-US",  // English (US)
  L"eo",     // Esperanto
  // TODO(jungshik) : Do we want to list all es-Foo for Latin-American
  // Spanish speaking countries?
  L"es",     // Spanish
  L"et",     // Estonian
  L"eu",     // Basque
  L"fa",     // Persian
  L"fi",     // Finnish
  L"fil",    // Filipino
  L"fo",     // Faroese
  L"fr",     // French
  L"fy",     // Frisian
  L"ga",     // Irish
  L"gd",     // Scots Gaelic
  L"gl",     // Galician
  L"gn",     // Guarani
  L"gu",     // Gujarati
  L"he",     // Hebrew
  L"hi",     // Hindi
  L"hr",     // Croatian
  L"hu",     // Hungarian
  L"hy",     // Armenian
  L"ia",     // Interlingua
  L"id",     // Indonesian
  L"is",     // Icelandic
  L"it",     // Italian
  L"ja",     // Japanese
  L"jw",     // Javanese
  L"ka",     // Georgian
  L"kk",     // Kazakh
  L"km",     // Cambodian
  L"kn",     // Kannada
  L"ko",     // Korean
  L"ku",     // Kurdish
  L"ky",     // Kyrgyz
  L"la",     // Latin
  L"ln",     // Lingala
  L"lo",     // Laothian
  L"lt",     // Lithuanian
  L"lv",     // Latvian
  L"mk",     // Macedonian
  L"ml",     // Malayalam
  L"mn",     // Mongolian
  L"mo",     // Moldavian
  L"mr",     // Marathi
  L"ms",     // Malay
  L"mt",     // Maltese
  L"nb",     // Norwegian (Bokmal)
  L"ne",     // Nepali
  L"nl",     // Dutch
  L"nn",     // Norwegian (Nynorsk)
  L"no",     // Norwegian
  L"oc",     // Occitan
  L"or",     // Oriya
  L"pa",     // Punjabi
  L"pl",     // Polish
  L"ps",     // Pashto
  L"pt",     // Portuguese
  L"pt-BR",  // Portuguese (Brazil)
  L"pt-PT",  // Portuguese (Portugal)
  L"qu",     // Quechua
  L"rm",     // Romansh
  L"ro",     // Romanian
  L"ru",     // Russian
  L"sd",     // Sindhi
  L"sh",     // Serbo-Croatian
  L"si",     // Sinhalese
  L"sk",     // Slovak
  L"sl",     // Slovenian
  L"sn",     // Shona
  L"so",     // Somali
  L"sq",     // Albanian
  L"sr",     // Serbian
  L"st",     // Sesotho
  L"su",     // Sundanese
  L"sv",     // Swedish
  L"sw",     // Swahili
  L"ta",     // Tamil
  L"te",     // Telugu
  L"tg",     // Tajik
  L"th",     // Thai
  L"ti",     // Tigrinya
  L"tk",     // Turkmen
  L"to",     // Tonga
  L"tr",     // Turkish
  L"tt",     // Tatar
  L"tw",     // Twi
  L"ug",     // Uighur
  L"uk",     // Ukrainian
  L"ur",     // Urdu
  L"uz",     // Uzbek
  L"vi",     // Vietnamese
  L"xh",     // Xhosa
  L"yi",     // Yiddish
  L"yo",     // Yoruba
  L"zh",     // Chinese
  L"zh-CN",  // Chinese&nbsp;(Simplified)
  L"zh-TW",  // Chinese&nbsp;(Traditional)
  L"zu",     // Zulu
};

///////////////////////////////////////////////////////////////////////////////
// AddLanguageWindowView
//
// This opens another window from where a new accept language can be selected.
//
class AddLanguageWindowView : public ChromeViews::View,
                              public ChromeViews::ComboBox::Listener,
                              public ChromeViews::DialogDelegate {
 public:
  AddLanguageWindowView(LanguagesPageView* language_delegate, Profile* profile);
  ChromeViews::Window* container() const { return container_; }
  void set_container(ChromeViews::Window* container) {
    container_ = container;
  }

  // ChromeViews::DialogDelegate methods.
  virtual bool Accept();
  virtual std::wstring GetWindowTitle() const;

  // ChromeViews::WindowDelegate method.
  virtual bool IsModal() const { return true; }
  virtual ChromeViews::View* GetContentsView() { return this; }

  // ChromeViews::ComboBox::Listener implementation:
  virtual void ItemChanged(ChromeViews::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // ChromeViews::View overrides.
  virtual void Layout();
  virtual void GetPreferredSize(CSize *out);

 protected:
  virtual void ViewHierarchyChanged(bool is_add, ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  void Init();

  // The Options dialog window.
  ChromeViews::Window* container_;

  // Used for Call back to LanguagePageView that language has been selected.
  LanguagesPageView* language_delegate_;
  std::wstring accept_language_selected_;

  // Combobox and its corresponding model.
  scoped_ptr<LanguageComboboxModel> accept_language_combobox_model_;
  ChromeViews::ComboBox* accept_language_combobox_;

  // The Profile associated with this window.
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(AddLanguageWindowView);
};

static const int kDialogPadding = 7;
static int kDefaultWindowWidthChars = 60;
static int kDefaultWindowHeightLines = 3;

AddLanguageWindowView::AddLanguageWindowView(LanguagesPageView* language_delegate,
                                             Profile* profile)
    : profile_(profile->GetOriginalProfile()),
      language_delegate_(language_delegate),
      accept_language_combobox_(NULL) {
  Init();

  // Initialize accept_language_selected_ to the first index in drop down.
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(0);
}

std::wstring AddLanguageWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE);
}

bool AddLanguageWindowView::Accept() {
  if (language_delegate_) {
    language_delegate_->OnAddLanguage(accept_language_selected_);
  }
  return true;
}

void AddLanguageWindowView::ItemChanged(ChromeViews::ComboBox* combo_box,
                                  int prev_index,
                                  int new_index) {
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(new_index);
}

void AddLanguageWindowView::Layout() {
  CSize sz;
  accept_language_combobox_->GetPreferredSize(&sz);
  accept_language_combobox_->SetBounds(kDialogPadding, kDialogPadding,
                                       GetWidth() - 2*kDialogPadding, sz.cy);
}

void AddLanguageWindowView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);
  out->cx = font.ave_char_width() * kDefaultWindowWidthChars;
  out->cy = font.height() * kDefaultWindowHeightLines;
}

void AddLanguageWindowView::ViewHierarchyChanged(
    bool is_add, ChromeViews::View* parent, ChromeViews::View* child) {
  // Can't init before we're inserted into a ViewContainer, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

void AddLanguageWindowView::Init() {
  // Determine Locale Codes.
  std::vector<std::wstring> locale_codes;
  const std::wstring app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < arraysize(accept_language_list); ++i) {
    std::wstring local_name =
        l10n_util::GetLocalName(accept_language_list[i], app_locale, false);
    // This is a hack. If ICU doesn't have a translated name for
    // this language, GetLocalName will just return the language code.
    // In that case, we skip it.
    // TODO(jungshik) : Put them at the of the list with language codes
    // enclosed by brackets.
    if (local_name != accept_language_list[i])
      locale_codes.push_back(accept_language_list[i]);
  }
  accept_language_combobox_model_.reset(new LanguageComboboxModel(
    profile_, locale_codes));
  accept_language_combobox_ = new ChromeViews::ComboBox(
      accept_language_combobox_model_.get());
  accept_language_combobox_->SetSelectedItem(0);
  accept_language_combobox_->SetListener(this);
  AddChildView(accept_language_combobox_);
}

class LanguageOrderTableModel : public ChromeViews::TableModel {
 public:
  LanguageOrderTableModel();

  // Set Language List.
  void SetAcceptLanguagesString(const std::wstring& language_list);

  // Add at the end.
  void Add(const std::wstring& language);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Move down the entry at the specified index.
  void MoveDown(int index);

  // Move up the entry at the specified index.
  void MoveUp(int index);

  // Returns the set of languagess this model contains.
  std::wstring GetLanguageList() { return VectorToList(languages_); }

  // ChromeViews::TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(ChromeViews::TableModelObserver* observer);

 private:
  // This method converts a comma separated list to a vector of strings.
  void ListToVector(const std::wstring& list,
                    std::vector<std::wstring>* vector);

  // This method returns a comma separated string given a string vector.
  std::wstring VectorToList(const std::vector<std::wstring>& vector);

  // Set of entries we're showing.
  std::vector<std::wstring> languages_;
  std::wstring comma_separated_language_list_;

  ChromeViews::TableModelObserver* observer_;

  DISALLOW_EVIL_CONSTRUCTORS(LanguageOrderTableModel);
};

LanguageOrderTableModel::LanguageOrderTableModel()
    : observer_(NULL) {
}

void LanguageOrderTableModel::SetAcceptLanguagesString(
    const std::wstring& language_list) {
  std::vector<std::wstring> languages_vector;
  ListToVector(language_list, &languages_vector);
  for (int i = 0; i < static_cast<int>(languages_vector.size()); i++) {
    Add(languages_vector.at(i));
  }
}

void LanguageOrderTableModel::SetObserver(
    ChromeViews::TableModelObserver* observer) {
  observer_ = observer;
}

std::wstring LanguageOrderTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  const std::wstring app_locale = g_browser_process->GetApplicationLocale();
  return l10n_util::GetLocalName(languages_.at(row), app_locale, true);
}

void LanguageOrderTableModel::Add(const std::wstring& language) {
  if (language.empty())
    return;
  // Check for selecting duplicated language.
  for (std::vector<std::wstring>::const_iterator cit = languages_.begin();
       cit != languages_.end(); ++cit)
    if (*cit == language)
      return;
  languages_.push_back(language);
  if (observer_)
    observer_->OnItemsAdded(RowCount() - 1, 1);
}

void LanguageOrderTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  languages_.erase(languages_.begin() + index);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

void LanguageOrderTableModel::MoveDown(int index) {
  if (index < 0 || index >= RowCount() - 1)
    return;
  std::wstring item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  if (index == RowCount() - 1)
    languages_.push_back(item);
  else
    languages_.insert(languages_.begin() + index + 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

void LanguageOrderTableModel::MoveUp(int index) {
  if (index <= 0 || index >= static_cast<int>(languages_.size()))
    return;
  std::wstring item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  languages_.insert(languages_.begin() + index - 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

int LanguageOrderTableModel::RowCount() {
  return static_cast<int>(languages_.size());
}

void LanguageOrderTableModel::ListToVector(const std::wstring& list,
                                           std::vector<std::wstring>* vector) {
  SplitString(list, L',', vector);
}

std::wstring LanguageOrderTableModel::VectorToList(
    const std::vector<std::wstring>& vector)  {
  std::wstring list;
  for (int i = 0 ; i < static_cast<int>(vector.size()) ; i++) {
    list += vector.at(i);
    if (i != vector.size() - 1)
      list += ',';
  }
  return list;
}

LanguagesPageView::LanguagesPageView(Profile* profile)
    : languages_instructions_(NULL),
      languages_contents_(NULL),
      language_order_table_(NULL),
      add_button_(NULL),
      remove_button_(NULL),
      move_up_button_(NULL),
      move_down_button_(NULL),
      button_stack_(NULL),
      language_info_label_(NULL),
      ui_language_label_(NULL),
      change_ui_language_combobox_(NULL),
      change_dictionary_language_combobox_(NULL),
      dictionary_language_label_(NULL),
      OptionsPageView(profile),
      language_table_edited_(false) {
  accept_languages_.Init(prefs::kAcceptLanguages,
      profile->GetPrefs(), NULL);
}

LanguagesPageView::~LanguagesPageView() {
  if (language_order_table_)
    language_order_table_->SetModel(NULL);
}

void LanguagesPageView::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == move_up_button_) {
    OnMoveUpLanguage();
    language_table_edited_ = true;
  } else if (sender == move_down_button_) {
    OnMoveDownLanguage();
    language_table_edited_ = true;
  } else if (sender == remove_button_) {
    OnRemoveLanguage();
    language_table_edited_ = true;
  } else if (sender == add_button_) {
    ChromeViews::Window::CreateChromeWindow(
        GetViewContainer()->GetHWND(),
        gfx::Rect(),
        new AddLanguageWindowView(this, profile()))->Show();
    language_table_edited_ = true;
  }
}

void LanguagesPageView::OnAddLanguage(const std::wstring& new_language) {
  language_order_table_model_->Add(new_language);
  language_order_table_->Select(language_order_table_model_->RowCount() - 1);
  OnSelectionChanged();
}

void LanguagesPageView::InitControlLayout() {
  // Define the buttons.
  add_button_ = new ChromeViews::NativeButton(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_ADD_BUTTON_LABEL));
  add_button_->SetListener(this);
  remove_button_ = new ChromeViews::NativeButton(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_REMOVE_BUTTON_LABEL));
  remove_button_->SetEnabled(false);
  remove_button_->SetListener(this);
  move_up_button_ = new ChromeViews::NativeButton(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEUP_BUTTON_LABEL));
  move_up_button_->SetEnabled(false);
  move_up_button_->SetListener(this);
  move_down_button_ = new ChromeViews::NativeButton(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEDOWN_BUTTON_LABEL));
  move_down_button_->SetEnabled(false);
  move_down_button_->SetListener(this);

  languages_contents_ = new ChromeViews::View;
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);

  // Add the instructions label.
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                      GridLayout::USE_PREF, 0, 0);
  languages_instructions_ = new ChromeViews::Label(
      l10n_util::GetString(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_INSTRUCTIONS));
  languages_instructions_->SetMultiLine(true);
  languages_instructions_->SetHorizontalAlignment(
      ChromeViews::Label::ALIGN_LEFT);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(languages_instructions_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add two columns - for table, and for button stack.
  std::vector<ChromeViews::TableColumn> columns;
  columns.push_back(ChromeViews::TableColumn());
  language_order_table_model_.reset(new LanguageOrderTableModel);
  language_order_table_ = new ChromeViews::TableView(
      language_order_table_model_.get(), columns,
      ChromeViews::TEXT_ONLY, false, true, true);
  language_order_table_->SetObserver(this);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);

  // Add the table to the the first column.
  layout->AddView(language_order_table_);

  // Now add the four buttons to the second column.
  button_stack_ = new ChromeViews::View;
  GridLayout* button_stack_layout = new GridLayout(button_stack_);
  button_stack_->SetLayoutManager(button_stack_layout);

  column_set = button_stack_layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_up_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_down_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(remove_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(add_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);

  layout->AddView(button_stack_);

  layout->AddPaddingRow(0, kUnrelatedControlLargeVerticalSpacing);

  language_info_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_LANGUAGE_INFO));
  language_info_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  ui_language_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_UI_LANGUAGE));
  ui_language_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  ui_language_model_.reset(new LanguageComboboxModel);
  change_ui_language_combobox_ =
      new ChromeViews::ComboBox(ui_language_model_.get());
  change_ui_language_combobox_->SetListener(this);
  dictionary_language_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_CHROME_DICTIONARY_LANGUAGE));
  dictionary_language_label_->SetHorizontalAlignment(
      ChromeViews::Label::ALIGN_LEFT);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(language_info_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int double_column_view_set_2_id = 2;
  column_set = layout->AddColumnSet(double_column_view_set_2_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Determine Locale Codes.
  std::vector<std::wstring> locale_codes;
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages); ++i)
    locale_codes.push_back(g_supported_spellchecker_languages[i]);
  dictionary_language_model_.reset(new LanguageComboboxModel(profile(),
                                                             locale_codes));
  change_dictionary_language_combobox_ =
      new ChromeViews::ComboBox(dictionary_language_model_.get());
  change_dictionary_language_combobox_->SetListener(this);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(ui_language_label_);
  layout->AddView(change_ui_language_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(dictionary_language_label_);
  layout->AddView(change_dictionary_language_combobox_);

  // Init member prefs so we can update the controls if prefs change.
  app_locale_.Init(prefs::kApplicationLocale,
                   g_browser_process->local_state(), this);
  dictionary_language_.Init(prefs::kSpellCheckDictionary,
                            profile()->GetPrefs(), this);
}

void LanguagesPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kAcceptLanguages) {
    language_order_table_model_->SetAcceptLanguagesString(
        accept_languages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kApplicationLocale) {
    int index = ui_language_model_->GetSelectedLanguageIndex(
        prefs::kApplicationLocale);
    if (-1 == index) {
      // The pref value for locale isn't valid.  Use the current app locale
      // (which is what we're currently using).
      index = ui_language_model_->GetIndexFromLocale(
          g_browser_process->GetApplicationLocale());
    }
    DCHECK(-1 != index);
    change_ui_language_combobox_->SetSelectedItem(index);
  }
  if (!pref_name || *pref_name == prefs::kSpellCheckDictionary) {
    int index = dictionary_language_model_->GetSelectedLanguageIndex(
        prefs::kSpellCheckDictionary);
    change_dictionary_language_combobox_->SetSelectedItem(index);
  }
}

void LanguagesPageView::ItemChanged(ChromeViews::ComboBox* sender,
                                    int prev_index,
                                    int new_index) {
  if (sender == change_ui_language_combobox_) {
    UserMetricsRecordAction(L"Options_AppLanguage",
                            g_browser_process->local_state());
    app_locale_.SetValue(ui_language_model_->GetLocaleFromIndex(new_index));
    RestartMessageBox::ShowMessageBox(GetRootWindow());
  } else if (sender == change_dictionary_language_combobox_) {
    UserMetricsRecordAction(L"Options_DictionaryLanguage",
                            profile()->GetPrefs());
    dictionary_language_.SetValue(dictionary_language_model_->
        GetLocaleFromIndex(new_index));
    RestartMessageBox::ShowMessageBox(GetRootWindow());
  }
}

void LanguagesPageView::OnSelectionChanged() {
  move_up_button_->SetEnabled(language_order_table_->FirstSelectedRow() > 0 &&
                              language_order_table_->SelectedRowCount() == 1);
  move_down_button_->SetEnabled(language_order_table_->FirstSelectedRow() <
                                language_order_table_->RowCount() - 1 &&
                                language_order_table_->SelectedRowCount() ==
                                1);
  remove_button_->SetEnabled(language_order_table_->SelectedRowCount() > 0);
}

void LanguagesPageView::OnRemoveLanguage() {
  int item_selected = 0;
  for (ChromeViews::TableView::iterator i =
       language_order_table_->SelectionBegin();
       i != language_order_table_->SelectionEnd(); ++i) {
    language_order_table_model_->Remove(*i);
    item_selected = *i;
  }

  move_up_button_->SetEnabled(false);
  move_down_button_->SetEnabled(false);
  remove_button_->SetEnabled(false);
  int items_left = language_order_table_model_->RowCount();
  if (items_left <= 0)
    return;
  if (item_selected > items_left - 1)
    item_selected = items_left - 1;
  language_order_table_->Select(item_selected);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveDownLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveDown(item_selected);
  language_order_table_->Select(item_selected + 1);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveUpLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveUp(item_selected);
  language_order_table_->Select(item_selected - 1);

  OnSelectionChanged();
}

void LanguagesPageView::SaveChanges() {
  if (language_order_table_model_.get() && language_table_edited_)
    accept_languages_.SetValue(language_order_table_model_->GetLanguageList());
}
