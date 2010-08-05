// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/personal_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

PersonalOptionsHandler::PersonalOptionsHandler() {
}

PersonalOptionsHandler::~PersonalOptionsHandler() {
}

void PersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {

  DCHECK(localized_strings);
  // Personal Stuff page
  localized_strings->SetString(L"sync_section",
      l10n_util::GetString(IDS_SYNC_OPTIONS_GROUP_NAME));
  localized_strings->SetString(L"sync_not_setup_info",
      l10n_util::GetStringF(IDS_SYNC_NOT_SET_UP_INFO,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
  localized_strings->SetString(L"start_sync",
      l10n_util::GetString(IDS_SYNC_START_SYNC_BUTTON_LABEL));
  localized_strings->SetString(L"sync_customize",
      l10n_util::GetString(IDS_SYNC_CUSTOMIZE_BUTTON_LABEL));
  localized_strings->SetString(L"stop_sync",
      l10n_util::GetString(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL));

  localized_strings->SetString(L"passwords",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME));
  localized_strings->SetString(L"passwords_asktosave",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE));
  localized_strings->SetString(L"passwords_neversave",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE));
  localized_strings->SetString(L"showpasswords",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS));

  localized_strings->SetString(L"autofill",
      l10n_util::GetString(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME));
  localized_strings->SetString(L"autofill_options",
      l10n_util::GetString(IDS_AUTOFILL_OPTIONS));

  localized_strings->SetString(L"browsing_data",
      l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME));
  localized_strings->SetString(L"import_data",
      l10n_util::GetString(IDS_OPTIONS_IMPORT_DATA_BUTTON));

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_OPENBSD)
  localized_strings->SetString(L"appearance",
      l10n_util::GetString(IDS_APPEARANCE_GROUP_NAME));
  localized_strings->SetString(L"themes_GTK_button",
      l10n_util::GetString(IDS_THEMES_GTK_BUTTON));
  localized_strings->SetString(L"themes_set_classic",
      l10n_util::GetString(IDS_THEMES_SET_CLASSIC));
  localized_strings->SetString(L"showWindow_decorations_radio",
      l10n_util::GetString(IDS_SHOW_WINDOW_DECORATIONS_RADIO));
  localized_strings->SetString(L"hideWindow_decorations_radio",
      l10n_util::GetString(IDS_HIDE_WINDOW_DECORATIONS_RADIO));
  localized_strings->SetString(L"themes_gallery",
      l10n_util::GetString(IDS_THEMES_GALLERY_BUTTON));
#else
  localized_strings->SetString(L"themes",
      l10n_util::GetString(IDS_THEMES_GROUP_NAME));
  localized_strings->SetString(L"themes_reset",
      l10n_util::GetString(IDS_THEMES_RESET_BUTTON));
  localized_strings->SetString(L"themes_gallery",
      l10n_util::GetString(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString(L"themes_default",
      l10n_util::GetString(IDS_THEMES_DEFAULT_THEME_LABEL));
#endif
}

void PersonalOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback(
      "getSyncStatus",
      NewCallback(this,&PersonalOptionsHandler::SetSyncStatusUIString));
}

void PersonalOptionsHandler::SetSyncStatusUIString(const Value* value) {
  DCHECK(dom_ui_);

  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
  if(service != NULL && ProfileSyncService::IsSyncEnabled()) {
    scoped_ptr<Value> status_string(Value::CreateStringValue(
        l10n_util::GetStringFUTF16(IDS_SYNC_ACCOUNT_SYNCED_TO_USER_WITH_TIME,
                                   service->GetAuthenticatedUsername(),
                                   service->GetLastSyncedTimeString())));

    dom_ui_->CallJavascriptFunction(
        L"PersonalOptions.syncStatusCallback",
        *(status_string.get()));
  }
}
