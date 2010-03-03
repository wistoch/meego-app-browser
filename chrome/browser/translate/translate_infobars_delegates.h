// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/translate/translate_prefs.h"

class SkBitmap;

// An interface derived from InfoBarDelegate to form the base interface for
// translate infobars.
class TranslateInfoBarDelegate : public InfoBarDelegate {
 public:
  enum TranslateState {
    kBeforeTranslate = 1,
    kTranslating,
    kAfterTranslate,
    kTranslationFailed,
  };

  // Instantiates a TranslateInfoBarDelegate. Can return NULL if the passed
  // languages are not supported.
  static TranslateInfoBarDelegate* Create(TabContents* contents,
                                          PrefService* user_prefs,
                                          TranslateState state,
                                          const GURL& url,
                                          const std::string& original_language,
                                          const std::string& target_language);

  void UpdateState(TranslateState new_state);
  void GetAvailableOriginalLanguages(std::vector<std::string>* languages);
  void GetAvailableTargetLanguages(std::vector<std::string>* languages);
  void ModifyOriginalLanguage(int lang_index);
  void ModifyTargetLanguage(int lang_index);
  void Translate();
  void TranslationDeclined();
  bool IsLanguageBlacklisted();
  void ToggleLanguageBlacklist();
  bool IsSiteBlacklisted();
  void ToggleSiteBlacklist();
  bool ShouldAlwaysTranslate();
  void ToggleAlwaysTranslate();

  int original_lang_index() const {
    return original_lang_index_;
  }
  int target_lang_index() const {
    return target_lang_index_;
  }
  const std::string& original_lang_code() const {
    return supported_languages_[original_lang_index_];
  }
  const std::string& target_lang_code() const {
    return supported_languages_[target_lang_index_];
  }
  const std::string& GetLocaleFromIndex(int lang_index) const {
    return supported_languages_[lang_index];
  }
  TabContents* tab_contents() const {
    return tab_contents_;
  }
  TranslateState state() const {
    return state_;
  }

  // Retrieve the text for the toolbar label.  The toolbar label is a bit
  // strange since we need to place popup menus inside the string in question.
  // To do this we use two placeholders.
  //
  // |message_text| is the text to display for the label.
  // |offsets| contains the offsets of the number of placeholders in the text
  // + message_text->length() i.e. it can contain 2 or 3 elements.
  // offsets[0] < offsets[1] even in cases where the languages need to be
  // displayed in reverse order.
  // |swapped_language_placeholders| is true if we need to flip the order
  // of the menus in the current locale.
  void GetMessageText(string16 *message_text,
                      std::vector<size_t> *offsets,
                      bool *swapped_language_placeholders);

  // Overridden from InfoBarDelegate.
  virtual Type GetInfoBarType() {
    return PAGE_ACTION_TYPE;
  }
  virtual SkBitmap* GetIcon() const;
  virtual TranslateInfoBarDelegate* AsTranslateInfoBarDelegate() {
    return this;
  }
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual void InfoBarDismissed();
  virtual void InfoBarClosed();

  // Returns the printable version of the language code |language_code|.
  static string16 GetDisplayNameForLocale(const std::string& language_code);

  // Overridden from InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();

 private:
  TranslateInfoBarDelegate(TabContents* contents,
                           PrefService* user_prefs,
                           TranslateState state,
                           const GURL& url,
                           int original_language_index,
                           int target_language_index);

  TabContents* tab_contents_;  // Weak.
  TranslatePrefs prefs_;
  TranslateState state_;
  std::string site_;
  int original_lang_index_;
  int target_lang_index_;
  // The list of languages supported.
  std::vector<std::string> supported_languages_;
  bool never_translate_language_;
  bool never_translate_site_;
  bool always_translate_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_
