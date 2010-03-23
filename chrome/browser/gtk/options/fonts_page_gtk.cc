// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/fonts_page_gtk.h"

#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/generated_resources.h"

namespace {

// Make a Gtk font name string from a font family name and pixel size.
std::string MakeFontName(std::wstring family_name, int pixel_size) {
  // The given font might not be available (the default fonts we use are not
  // installed by default on some distros).  So figure out which font we are
  // actually falling back to and display that.  (See crbug.com/31381.)
  std::wstring actual_family_name = gfx::Font::CreateFont(
      family_name, pixel_size).FontName();
  std::string fontname;
  // TODO(mattm): We can pass in the size in pixels (px), and the font button
  // actually honors it, but when you open the selector it interprets it as
  // points.  See crbug.com/17857
  SStringPrintf(&fontname, "%s, %dpx", WideToUTF8(actual_family_name).c_str(),
                pixel_size);
  return fontname;
}

}  // namespace

FontsPageGtk::FontsPageGtk(Profile* profile) : OptionsPageBase(profile) {
  Init();
}

FontsPageGtk::~FontsPageGtk() {
}

void FontsPageGtk::Init() {
  OptionsLayoutBuilderGtk options_builder;

  serif_font_button_ = gtk_font_button_new();
  gtk_font_button_set_use_font(GTK_FONT_BUTTON(serif_font_button_), TRUE);
  gtk_font_button_set_use_size(GTK_FONT_BUTTON(serif_font_button_), TRUE);
  g_signal_connect(serif_font_button_, "font-set",
                   G_CALLBACK(OnSerifFontSetThunk), this);

  sans_font_button_ = gtk_font_button_new();
  gtk_font_button_set_use_font(GTK_FONT_BUTTON(sans_font_button_), TRUE);
  gtk_font_button_set_use_size(GTK_FONT_BUTTON(sans_font_button_), TRUE);
  g_signal_connect(sans_font_button_, "font-set",
                   G_CALLBACK(OnSansFontSetThunk), this);

  fixed_font_button_ = gtk_font_button_new();
  gtk_font_button_set_use_font(GTK_FONT_BUTTON(fixed_font_button_), TRUE);
  gtk_font_button_set_use_size(GTK_FONT_BUTTON(fixed_font_button_), TRUE);
  g_signal_connect(fixed_font_button_, "font-set",
                   G_CALLBACK(OnFixedFontSetThunk), this);

  GtkWidget* font_controls = gtk_util::CreateLabeledControlsGroup(NULL,
      l10n_util::GetStringUTF8(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SERIF_LABEL).c_str(),
      serif_font_button_,
      l10n_util::GetStringUTF8(
        IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SANS_SERIF_LABEL).c_str(),
      sans_font_button_,
      l10n_util::GetStringUTF8(
        IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_FIXED_WIDTH_LABEL).c_str(),
      fixed_font_button_,
      NULL);

  options_builder.AddOptionGroup(l10n_util::GetStringUTF8(
        IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_FONT_TITLE),
      font_controls, false);

  InitDefaultEncodingComboBox();
  std::string encoding_group_description = l10n_util::GetStringUTF8(
      IDS_FONT_LANGUAGE_SETTING_FONT_DEFAULT_ENCODING_SELECTOR_LABEL);
  GtkWidget* encoding_controls = gtk_util::CreateLabeledControlsGroup(NULL,
      encoding_group_description.c_str(),
      default_encoding_combobox_,
      NULL);
  options_builder.AddOptionGroup(l10n_util::GetStringUTF8(
        IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_ENCODING_TITLE),
      encoding_controls, false);

  page_ = options_builder.get_page_widget();

  serif_name_.Init(prefs::kWebKitSerifFontFamily, profile()->GetPrefs(), this);
  sans_serif_name_.Init(prefs::kWebKitSansSerifFontFamily,
                        profile()->GetPrefs(), this);
  variable_width_size_.Init(prefs::kWebKitDefaultFontSize,
                            profile()->GetPrefs(), this);

  fixed_width_name_.Init(prefs::kWebKitFixedFontFamily, profile()->GetPrefs(),
                         this);
  fixed_width_size_.Init(prefs::kWebKitDefaultFixedFontSize,
                         profile()->GetPrefs(), this);

  default_encoding_.Init(prefs::kDefaultCharset, profile()->GetPrefs(), this);

  NotifyPrefChanged(NULL);
}

void FontsPageGtk::InitDefaultEncodingComboBox() {
  default_encoding_combobox_ = gtk_combo_box_new_text();
  g_signal_connect(default_encoding_combobox_, "changed",
                   G_CALLBACK(OnDefaultEncodingChangedThunk), this);
  int canonical_encoding_names_length =
      CharacterEncoding::GetSupportCanonicalEncodingCount();
  // Initialize the vector of all sorted encodings according to current
  // UI locale.
  std::string locale = g_browser_process->GetApplicationLocale();
  for (int i = 0; i < canonical_encoding_names_length; i++) {
    sorted_encoding_list_.push_back(CharacterEncoding::EncodingInfo(
        CharacterEncoding::GetEncodingCommandIdByIndex(i)));
  }
  l10n_util::SortVectorWithStringKey(locale, &sorted_encoding_list_, true);
  for (size_t i = 0; i < sorted_encoding_list_.size(); i++) {
    gtk_combo_box_append_text(
        GTK_COMBO_BOX(default_encoding_combobox_),
        WideToUTF8(sorted_encoding_list_[i].encoding_display_name).c_str());
  }
}

void FontsPageGtk::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kWebKitSerifFontFamily ||
      *pref_name == prefs::kWebKitDefaultFontSize) {
    gtk_font_button_set_font_name(GTK_FONT_BUTTON(serif_font_button_),
        MakeFontName(serif_name_.GetValue(),
          variable_width_size_.GetValue()).c_str());
  }
  if (!pref_name || *pref_name == prefs::kWebKitSansSerifFontFamily ||
      *pref_name == prefs::kWebKitDefaultFontSize) {
    gtk_font_button_set_font_name(GTK_FONT_BUTTON(sans_font_button_),
        MakeFontName(sans_serif_name_.GetValue(),
          variable_width_size_.GetValue()).c_str());
  }
  if (!pref_name || *pref_name == prefs::kWebKitFixedFontFamily ||
      *pref_name == prefs::kWebKitDefaultFixedFontSize) {
    gtk_font_button_set_font_name(GTK_FONT_BUTTON(fixed_font_button_),
        MakeFontName(fixed_width_name_.GetValue(),
          fixed_width_size_.GetValue()).c_str());
  }
  if (!pref_name || *pref_name == prefs::kDefaultCharset) {
    const std::string current_encoding =
        WideToASCII(default_encoding_.GetValue());
    for (size_t i = 0; i < sorted_encoding_list_.size(); i++) {
      if (CharacterEncoding::GetCanonicalEncodingNameByCommandId(
          sorted_encoding_list_[i].encoding_id) == current_encoding) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(default_encoding_combobox_), i);
        break;
      }
    }
  }
}

void FontsPageGtk::SetFontsFromButton(StringPrefMember* name_pref,
                                      IntegerPrefMember* size_pref,
                                      GtkWidget* font_button) {
  PangoFontDescription* desc = pango_font_description_from_string(
      gtk_font_button_get_font_name(GTK_FONT_BUTTON(font_button)));
  int size = pango_font_description_get_size(desc);
  name_pref->SetValue(UTF8ToWide(pango_font_description_get_family(desc)));
  size_pref->SetValue(size / PANGO_SCALE);
  pango_font_description_free(desc);
  // Reset the button font in px, since the chooser will have set it in points.
  // Also, both sans and serif share the same size so we need to update them
  // both.
  NotifyPrefChanged(NULL);
}

void FontsPageGtk::OnSerifFontSet(GtkWidget* font_button) {
  SetFontsFromButton(&serif_name_,
                     &variable_width_size_,
                     font_button);
}

void FontsPageGtk::OnSansFontSet(GtkWidget* font_button) {
  SetFontsFromButton(&sans_serif_name_,
                     &variable_width_size_,
                     font_button);
}

void FontsPageGtk::OnFixedFontSet(GtkWidget* font_button) {
  SetFontsFromButton(&fixed_width_name_,
                     &fixed_width_size_,
                     font_button);
}

void FontsPageGtk::OnDefaultEncodingChanged(GtkWidget* combo_box) {
  int index = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_box));
  if (index < 0 ||
      static_cast<size_t>(index) >= sorted_encoding_list_.size()) {
    NOTREACHED();
    return;
  }
  default_encoding_.SetValue(
      ASCIIToWide(CharacterEncoding::GetCanonicalEncodingNameByCommandId(
          sorted_encoding_list_[index].encoding_id)));
}
