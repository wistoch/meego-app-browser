// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#include "chrome/browser/views/cookie_prompt_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& origin,
    const std::string& cookie_line,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      dialog_type_(DIALOG_TYPE_COOKIE),
      origin_(origin),
      cookie_line_(cookie_line),
      delegate_(delegate) {
}


CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      dialog_type_(DIALOG_TYPE_LOCAL_STORAGE),
      origin_(origin),
      local_storage_key_(key),
      local_storage_value_(value),
      delegate_(delegate) {
}

// static
void CookiePromptModalDialog::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kCookiePromptExpanded, false);
}
