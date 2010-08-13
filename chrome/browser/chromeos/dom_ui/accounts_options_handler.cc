// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/accounts_options_handler.h"

#include "app/l10n_util.h"
#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "grit/generated_resources.h"

namespace chromeos {

AccountsOptionsHandler::AccountsOptionsHandler() {
}

AccountsOptionsHandler::~AccountsOptionsHandler() {
}

void AccountsOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("accountsPage", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_TAB_LABEL));

  localized_strings->SetString("allow_BWSI", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString("allow_guest",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_GUEST_DESCRIPTION));
  localized_strings->SetString("show_user_on_signin",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_SHOW_USER_NAMES_ON_SINGIN_DESCRIPTION));
  localized_strings->SetString("username_edit_hint",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_EDIT_HINT));
  localized_strings->SetString("username_format",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_FORMAT));
  localized_strings->SetString("add_users",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ADD_USERS));
}

}  // namespace chromeos
