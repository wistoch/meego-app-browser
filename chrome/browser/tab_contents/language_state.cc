// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/language_state.h"

#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

LanguageState::LanguageState(NavigationController* nav_controller)
    : navigation_controller_(nav_controller),
      translation_pending_(false) {
}

LanguageState::~LanguageState() {
}

void LanguageState::DidNavigate(bool reload) {
  if (!reload) {
    prev_original_lang_ = original_lang_;
    prev_current_lang_ = current_lang_;
    original_lang_.clear();
  }

  current_lang_.clear();

  translation_pending_ = false;
}

void LanguageState::LanguageDetermined(const std::string& page_language) {
  original_lang_ = page_language;
  current_lang_ = page_language;
}

std::string LanguageState::AutoTranslateTo() const {
  // Only auto-translate if:
  // - no translation is pending
  // - this page is in the same language as the previous page
  // - the previous page had been translated
  // - this page is not already translated
  // - the new page was navigated through a link.
  if (!translation_pending_ &&
      prev_original_lang_ == original_lang_ &&
      prev_original_lang_ != prev_current_lang_ &&
      original_lang_ == current_lang_ &&
      navigation_controller_->GetActiveEntry() &&
      navigation_controller_->GetActiveEntry()->transition_type() ==
          PageTransition::LINK) {
    return prev_current_lang_;
  }

  return std::string();
}
