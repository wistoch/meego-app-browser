// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options_ui.h"

class DictionaryValue;
class ListValue;

// ChromeOS language options page UI handler.
class LanguageOptionsHandler : public OptionsPageUIHandler {
 public:
  LanguageOptionsHandler();
  virtual ~LanguageOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  // Gets the list of input methods. The return value will look like:
  // [{'id': 'pinyin', 'displayName': 'Pinyin', 'languageCode': 'zh-CW'}, ...]
  ListValue* GetInputMethodList();

  // Gets the list of languages. The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  ListValue* GetLanguageList();

  // Called when the UI language is changed.
  // |value| will be the language code as string (ex. "fr").
  void UiLanguageChangeCallback(const Value* value);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
