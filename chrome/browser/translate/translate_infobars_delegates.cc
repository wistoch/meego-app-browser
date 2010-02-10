// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobars_delegates.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// TranslateInfoBarDelegate: InfoBarDelegate overrides: ------------------------

SkBitmap* TranslateInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_TRANSLATE);
}

bool TranslateInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  TranslateInfoBarDelegate* translate_delegate =
      delegate->AsTranslateInfoBarDelegate();
  // There can be only 1 translate infobar at any one time.
  return (!!translate_delegate);
}

void TranslateInfoBarDelegate::InfoBarClosed() {
  delete this;
}

// TranslateInfoBarDelegate: public: -------------------------------------------

TranslateInfoBarDelegate::TranslateInfoBarDelegate(TabContents* tab_contents,
    PrefService* user_prefs, TranslateState state, const GURL& url,
    const std::string& original_lang_code, const std::string& target_lang_code)
    : InfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      prefs_(user_prefs),
      state_(state),
      site_(url.HostNoBrackets()),
      original_lang_index_(-1),
      target_lang_index_(-1),
      never_translate_language_(false),
      never_translate_site_(false),
      always_translate_(false) {
  TranslationService::GetSupportedLanguages(&supported_languages_);
  for (size_t i = 0; i < supported_languages_.size(); ++i) {
    if (original_lang_code == supported_languages_[i]) {
      original_lang_index_ = i;
      break;
    }
  }
  DCHECK(original_lang_index_ > -1);
  for (size_t i = 0; i < supported_languages_.size(); ++i) {
    if (target_lang_code == supported_languages_[i]) {
      target_lang_index_ = i;
      break;
    }
  }
  DCHECK(target_lang_index_ > -1);
}

void TranslateInfoBarDelegate::UpdateState(TranslateState new_state) {
  if (state_ != new_state)
    state_ = new_state;
}

void TranslateInfoBarDelegate::ModifyOriginalLanguage(int lang_index) {
  original_lang_index_ = lang_index;
  // TODO(kuan): Send stats to Google Translate that original language has been
  // modified.
}

void TranslateInfoBarDelegate::ModifyTargetLanguage(int lang_index) {
  target_lang_index_ = lang_index;
}

void TranslateInfoBarDelegate::GetAvailableOriginalLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void TranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void TranslateInfoBarDelegate::Translate() {
  if (original_lang_index_ != target_lang_index_)
    tab_contents_->TranslatePage(original_lang_code(), target_lang_code());
}

bool TranslateInfoBarDelegate::IsLanguageBlacklisted() {
  if (state_ == kBeforeTranslate) {
    never_translate_language_ =
        prefs_.IsLanguageBlacklisted(original_lang_code());
    return never_translate_language_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

bool TranslateInfoBarDelegate::IsSiteBlacklisted() {
  if (state_ == kBeforeTranslate) {
    never_translate_site_ = prefs_.IsSiteBlacklisted(site_);
    return never_translate_site_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  if (state_ == kAfterTranslate) {
    always_translate_ = prefs_.IsLanguagePairWhitelisted(original_lang_code(),
        target_lang_code());
    return always_translate_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

void TranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  if (state_ == kBeforeTranslate) {
    never_translate_language_ = !never_translate_language_;
    if (never_translate_language_)
      prefs_.BlacklistLanguage(original_lang_code());
    else
      prefs_.RemoveLanguageFromBlacklist(original_lang_code());
  } else {
    NOTREACHED() << "Invalid mehod called for translate state";
  }
}

void TranslateInfoBarDelegate::ToggleSiteBlacklist() {
  if (state_ == kBeforeTranslate) {
    never_translate_site_ = !never_translate_site_;
    if (never_translate_site_)
      prefs_.BlacklistSite(site_);
    else
      prefs_.RemoveSiteFromBlacklist(site_);
  } else {
    NOTREACHED() << "Invalid mehod called for translate state";
  }
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  if (state_ == kAfterTranslate) {
    always_translate_ = !always_translate_;
    if (always_translate_)
      prefs_.WhitelistLanguagePair(original_lang_code(), target_lang_code());
    else
      prefs_.RemoveLanguagePairFromWhitelist(original_lang_code(),
          target_lang_code());
  } else {
    NOTREACHED() << "Invalid mehod called for translate state";
  }
}

// TranslateInfoBarDelegate: static: -------------------------------------------

string16 TranslateInfoBarDelegate::GetDisplayNameForLocale(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

#if !defined(TOOLKIT_VIEWS)
// TranslateInfoBarDelegate: InfoBarDelegate overrides: ------------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif  // !TOOLKIT_VIEWS
