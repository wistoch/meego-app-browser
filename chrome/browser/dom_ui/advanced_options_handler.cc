// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/advanced_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/dom_ui/advanced_options_utils.h"
#endif

#if defined(OS_WIN)
#include "net/base/ssl_config_service_win.h"
#endif

AdvancedOptionsHandler::AdvancedOptionsHandler() {
}

AdvancedOptionsHandler::~AdvancedOptionsHandler() {
}

void AdvancedOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("privacyLearnMoreURL",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE_PRIVACY_URL));
  localized_strings->SetString("privacyLearnMoreLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LEARN_MORE_LABEL));
  localized_strings->SetString("downloadLocationGroupName",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME));
  localized_strings->SetString("downloadLocationBrowseButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON));
  localized_strings->SetString("downloadLocationBrowseTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE));
  localized_strings->SetString("downloadLocationBrowseWindowTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_WINDOW_TITLE));
  localized_strings->SetString("downloadLocationAskForSaveLocation",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION));
  localized_strings->SetString("autoOpenFileTypesInfo",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOOPENFILETYPES_INFO));
  localized_strings->SetString("autoOpenFileTypesResetToDefault",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT));
  localized_strings->SetString("gearSettingsGroupName",
      l10n_util::GetStringUTF16(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
  localized_strings->SetString("gearSettingsConfigureGearsButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON));
  localized_strings->SetString("translateEnableTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  localized_strings->SetString("certificatesLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_LABEL));
  localized_strings->SetString("certificatesManageButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON));
  localized_strings->SetString("proxiesLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_LABEL));
  localized_strings->SetString("proxiesConfigureButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString("safeBrowsingEnableProtection",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION));
  localized_strings->SetString("sslGroupDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_GROUP_DESCRIPTION));
  localized_strings->SetString("sslUseSSL2",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USESSL2));
  localized_strings->SetString("sslCheckRevocation",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_CHECKREVOCATION));
  localized_strings->SetString("sslUseSSL3",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USESSL3));
  localized_strings->SetString("sslUseTLS1",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USETLS1));
  localized_strings->SetString("networkDNSPrefetchEnabledDescription",
      l10n_util::GetStringUTF16(IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
  localized_strings->SetString("privacyContentSettingsButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON));
  localized_strings->SetString("privacyClearDataButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON));
  localized_strings->SetString("linkDoctorPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LINKDOCTOR_PREF));
  localized_strings->SetString("suggestPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SUGGEST_PREF));
  localized_strings->SetString("tabsToLinksPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TABS_TO_LINKS_PREF));
  localized_strings->SetString("fontSettingsInfo",
      l10n_util::GetStringUTF16(IDS_OPTIONS_FONTSETTINGS_INFO));
  localized_strings->SetString("fontSettingsConfigureFontsOnlyButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTSONLY_BUTTON));
  localized_strings->SetString("advancedSectionTitlePrivacy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY));
  localized_strings->SetString("advancedSectionTitleContent",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT));
  localized_strings->SetString("advancedSectionTitleSecurity",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY));
  localized_strings->SetString("advancedSectionTitleNetwork",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK));
  localized_strings->SetString("advancedSectionTitleTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE));
  localized_strings->SetString("translateEnableTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  localized_strings->SetString("enableLogging",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_LOGGING));
  localized_strings->SetString("disableServices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DISABLE_SERVICES));
}

void AdvancedOptionsHandler::Initialize() {
  SetupDownloadLocationPath();
  SetupAutoOpenFileTypesDisabledAttribute();
#if defined(OS_WIN)
  SetupSSLConfigSettings();
#endif
}

DOMMessageHandler* AdvancedOptionsHandler::Attach(DOMUI* dom_ui) {
  // Call through to superclass.
  DOMMessageHandler* handler = OptionsPageUIHandler::Attach(dom_ui);

  // Register for preferences that we need to observe manually.  These have
  // special behaviors that aren't handled by the standard prefs UI.
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  pref_service, this);
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen,
                        pref_service, this);

  // Return result from the superclass.
  return handler;
}

void AdvancedOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("selectDownloadLocation",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleSelectDownloadLocation));
  dom_ui_->RegisterMessageCallback("autoOpenFileTypesAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleAutoOpenButton));
#if !defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("showManageSSLCertificates",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowManageSSLCertificates));
  dom_ui_->RegisterMessageCallback("showNetworkProxySettings",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowNetworkProxySettings));
#endif

#if defined(OS_WIN)
  // Setup Windows specific callbacks.
  dom_ui_->RegisterMessageCallback("checkRevocationCheckboxAction",
     NewCallback(this,
                 &AdvancedOptionsHandler::HandleCheckRevocationCheckbox));
  dom_ui_->RegisterMessageCallback("useSSL2CheckboxAction",
     NewCallback(this,
                 &AdvancedOptionsHandler::HandleUseSSL2Checkbox));
#endif
}

void AdvancedOptionsHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kDownloadDefaultDirectory) {
      SetupDownloadLocationPath();
    } else if (*pref_name == prefs::kDownloadExtensionsToOpen) {
      SetupAutoOpenFileTypesDisabledAttribute();
    }
  }
}

void AdvancedOptionsHandler::HandleSelectDownloadLocation(const Value* value) {
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  select_folder_dialog_ = SelectFileDialog::Create(this);
  select_folder_dialog_->SelectFile(
      SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory),
      NULL, 0, FILE_PATH_LITERAL(""),
      dom_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}

void AdvancedOptionsHandler::FileSelected(const FilePath& path, int index,
                                          void* params) {
  default_download_location_.SetValue(path);
  SetupDownloadLocationPath();
}

void AdvancedOptionsHandler::HandleAutoOpenButton(const Value* value) {
  DCHECK(dom_ui_);
  DownloadManager* manager = dom_ui_->GetProfile()->GetDownloadManager();
  if (manager) manager->ResetAutoOpenFiles();
}

#if defined(OS_WIN)
void AdvancedOptionsHandler::HandleCheckRevocationCheckbox(const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST)) {
    LOG(WARNING) << "checkRevocationCheckboxAction called with missing or " <<
        "invalid value";
    return;
  }
  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() < 1) {
    LOG(WARNING) << "checkRevocationCheckboxAction called with too few " <<
        "arguments";
    return;
  }
  std::string checked_str;
  if (list->GetString(0, &checked_str)) {
    net::SSLConfigServiceWin::SetRevCheckingEnabled(checked_str == "true");
  }
}

void AdvancedOptionsHandler::HandleUseSSL2Checkbox(const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST)) {
    LOG(WARNING) << "useSSL2CheckboxAction called with missing or " <<
    "invalid value";
    return;
  }
  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() < 1) {
    LOG(WARNING) << "useSSL2CheckboxAction called with too few " <<
    "arguments";
    return;
  }
  std::string checked_str;
  if (list->GetString(0, &checked_str)) {
    net::SSLConfigServiceWin::SetSSL2Enabled(checked_str == "true");
  }
}
#endif

#if !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::ShowNetworkProxySettings(const Value* value) {
  UserMetricsRecordAction(UserMetricsAction("Options_ShowProxySettings"), NULL);
  DCHECK(dom_ui_);
  AdvancedOptionsUtilities::ShowNetworkProxySettings(dom_ui_->tab_contents());
}

void AdvancedOptionsHandler::ShowManageSSLCertificates(const Value* value) {
  UserMetricsRecordAction(UserMetricsAction("Options_ManageSSLCertificates"),
                          NULL);
  DCHECK(dom_ui_);
  AdvancedOptionsUtilities::ShowManageSSLCertificates(dom_ui_->tab_contents());
}
#endif

void AdvancedOptionsHandler::SetupDownloadLocationPath() {
  DCHECK(dom_ui_);
  StringValue value(default_download_location_.GetValue().value());
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetDownloadLocationPath", value);
}

void AdvancedOptionsHandler::SetupAutoOpenFileTypesDisabledAttribute() {
  // Set the enabled state for the AutoOpenFileTypesResetToDefault button.
  // We enable the button if the user has any auto-open file types registered.
  DCHECK(dom_ui_);
  DownloadManager* manager = dom_ui_->GetProfile()->GetDownloadManager();
  bool disabled = !(manager && manager->HasAutoOpenFileTypesRegistered());
  FundamentalValue value(disabled);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetAutoOpenFileTypesDisabledAttribute", value);
}

#if defined(OS_WIN)
void AdvancedOptionsHandler::SetupSSLConfigSettings() {
  DCHECK(dom_ui_);
  bool checkRevocationSetting = false;
  bool useSSLSetting = false;

  net::SSLConfig config;
  if (net::SSLConfigServiceWin::GetSSLConfigNow(&config)) {
    checkRevocationSetting = config.rev_checking_enabled;
    useSSLSetting = config.ssl2_enabled;
  }
  FundamentalValue checkRevocationValue(checkRevocationSetting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetCheckRevocationCheckboxState",
      checkRevocationValue);
  FundamentalValue useSSLValue(useSSLSetting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetUseSSL2CheckboxStatechecked", useSSLValue);
}
#endif
