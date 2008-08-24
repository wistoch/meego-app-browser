// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <atlbase.h>
#include <windows.h>
#include <msi.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/wmi_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/logging_installer.h"

#include "installer_util_strings.h"

namespace {
// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
std::wstring GetUninstallSurveyUrl() {
  std::wstring kSurveyUrl = L"http://www.google.com/support/chrome/bin/request.py?hl=$1&contact_type=uninstall";

  std::wstring language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.

  return ReplaceStringPlaceholders(kSurveyUrl.c_str(), language.c_str(), NULL);
}
}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const installer::Version& version) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const std::wstring kVersionParam = L"crversion";
  const std::wstring kVersion = version.GetString();
  const std::wstring kOSParam = L"os";
  std::wstring os_version = L"na";
  OSVERSIONINFO version_info;
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (GetVersionEx(&version_info)) {
    os_version = StringPrintf(L"%d.%d.%d", version_info.dwMajorVersion,
                                           version_info.dwMinorVersion,
                                           version_info.dwBuildNumber);
  }

  std::wstring iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  file_util::AppendToPath(&iexplore, L"Internet Explorer");
  file_util::AppendToPath(&iexplore, L"iexplore.exe");

  std::wstring command = iexplore + L" " + GetUninstallSurveyUrl() + L"&" +
      kVersionParam + L"=" + kVersion + L"&" + kOSParam + L"=" + os_version;
  int pid = 0;
  WMIProcessUtil::Launch(command, &pid);
}

// Uninstall Chrome specific Gears. First we find Gears MSI ProductId (that
// changes with every new version of Gears) using Gears MSI UpgradeCode (that
// does not change) and then uninstall Gears using API.
void GoogleChromeDistribution::DoPreUninstallOperations() {
  /* TODO(rahulk) this comment is commented for now because it is causing extra
  dependencies for the renderer. Need to remove ifdef from uninstall.cc and
  uncomment this function.
  wchar_t product[39];  // GUID + '\0'
  MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);  // Don't show any UI to user.
  for (int i = 0; MsiEnumRelatedProducts(google_update::kGearsUpgradeCode, 0, i,
                                         product) != ERROR_NO_MORE_ITEMS; ++i) {
    LOG(INFO) << "Uninstalling Gears - " << product;
    unsigned int ret = MsiConfigureProduct(product, INSTALLLEVEL_MAXIMUM,
                                           INSTALLSTATE_ABSENT);
    if (ret != ERROR_SUCCESS)
      LOG(ERROR) << "Failed to uninstall Gears " << product << ": " << ret;
  }*/
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return product_name;
}

std::wstring GoogleChromeDistribution::GetInstallSubDir() {
  return L"Google\\Chrome";
}

std::wstring GoogleChromeDistribution::GetNewGoogleUpdateApKey(bool diff_install,
    installer_util::InstallStatus status, const std::wstring& value) {
  // Magic suffix that we need to add or remove to "ap" key value.
  const std::wstring kMagicSuffix = L"-full";

  bool has_magic_string = false;
  if ((value.length() >= kMagicSuffix.length()) &&
      (value.rfind(kMagicSuffix) == (value.length() - kMagicSuffix.length()))) {
    LOG(INFO) << "Incremental installer failure key already set.";
    has_magic_string = true;
  }

  std::wstring new_value(value);
  if ((!diff_install || !GetInstallReturnCode(status)) && has_magic_string) {
    LOG(INFO) << "Removing failure key from value " << value;
    new_value = value.substr(0, value.length() - kMagicSuffix.length());
  } else if ((diff_install && GetInstallReturnCode(status)) &&
             !has_magic_string) {
    LOG(INFO) << "Incremental installer failed, setting failure key.";
    new_value.append(kMagicSuffix);
  }

  return new_value;
}

std::wstring GoogleChromeDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer_util::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

int GoogleChromeDistribution::GetInstallReturnCode(
    installer_util::InstallStatus status) {
  switch (status) {
    case installer_util::FIRST_INSTALL_SUCCESS:
    case installer_util::INSTALL_REPAIRED:
    case installer_util::NEW_VERSION_UPDATED:
    case installer_util::HIGHER_VERSION_EXISTS:
      return 0;  // For Google Update's benefit we need to return 0 for success
    default:
      return status;
  }
}

std::wstring GoogleChromeDistribution::GetUninstallLinkName() {
  const std::wstring& link_name =
      installer_util::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE);
  return link_name;
}

std::wstring GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome";
}

std::wstring GoogleChromeDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
  return key;
}

// This method checks if we need to change "ap" key in Google Update to try
// full installer as fall back method in case incremental installer fails.
// - If incremental installer fails we append a magic string ("-full"), if
// it is not present already, so that Google Update server next time will send
// full installer to update Chrome on the local machine
// - If we are currently running full installer, we remove this magic
// string (if it is present) regardless of whether installer failed or not.
// There is no fall-back for full installer :)
void GoogleChromeDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer_util::InstallStatus install_status) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring ap_key_value;
  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClientState);
  chrome_google_update_state_key.append(L"\\");
  chrome_google_update_state_key.append(google_update::kChromeGuid);
  if (!key.Open(reg_root, chrome_google_update_state_key.c_str(),
      KEY_ALL_ACCESS) || !key.ReadValue(google_update::kRegApFieldName,
      &ap_key_value)) {
    LOG(INFO) << "Application key not found. Returning without changing it.";
    key.Close();
    return;
  }

  std::wstring new_value = GoogleChromeDistribution::GetNewGoogleUpdateApKey(
      incremental_install, install_status, ap_key_value);
  if ((new_value.compare(ap_key_value) != 0) &&
      !key.WriteValue(google_update::kRegApFieldName, new_value.c_str())) {
    LOG(ERROR) << "Failed to write value " << new_value
               << " to the registry field " << google_update::kRegApFieldName;
  }
  key.Close();
}

