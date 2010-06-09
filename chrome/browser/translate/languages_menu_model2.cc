// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/languages_menu_model2.h"

#include "base/histogram.h"
#include "chrome/browser/translate/translate_infobar_delegate2.h"

LanguagesMenuModel2::LanguagesMenuModel2(
    TranslateInfoBarDelegate2* translate_delegate,
    LanguageType language_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menus::SimpleMenuModel(this)),
      translate_infobar_delegate_(translate_delegate),
      language_type_(language_type) {
  for (int i = 0; i < translate_delegate->GetLanguageCount(); ++i)
    AddCheckItem(i, translate_delegate->GetLanguageDisplayableNameAt(i));
}

LanguagesMenuModel2::~LanguagesMenuModel2() {
}

bool LanguagesMenuModel2::IsCommandIdChecked(int command_id) const {
  if (language_type_ == ORIGINAL)
    return command_id == translate_infobar_delegate_->original_language_index();
  return command_id == translate_infobar_delegate_->target_language_index();
}

bool LanguagesMenuModel2::IsCommandIdEnabled(int command_id) const {
  // Prevent from having the same language selectable in original and target.
  if (language_type_ == ORIGINAL)
    return true;
  return command_id != translate_infobar_delegate_->original_language_index();
}

bool LanguagesMenuModel2::GetAcceleratorForCommandId(
    int command_id, menus::Accelerator* accelerator) {
  return false;
}

void LanguagesMenuModel2::ExecuteCommand(int command_id) {
  if (language_type_ == ORIGINAL) {
    UMA_HISTOGRAM_COUNTS("Translate.ModifyOriginalLang", 1);
    translate_infobar_delegate_->SetOriginalLanguage(command_id);
    return;
  }
  UMA_HISTOGRAM_COUNTS("Translate.ModifyTargetLang", 1);
  translate_infobar_delegate_->SetTargetLanguage(command_id);
}
