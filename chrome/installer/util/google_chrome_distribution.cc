// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <windows.h>
#include <wtsapi32.h>
#include <msi.h>
#include <sddl.h>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "base/wmi_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"

#pragma comment(lib, "wtsapi32.lib")

namespace {
const wchar_t kChromeGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBrowserAppId[] = L"Chrome";

// The following strings are the possible outcomes of the toast experiment
// as recorded in the  |client| field. Previously the groups used "TSxx" but
// the data captured is not valid.
const wchar_t kToastExpControlGroup[] =      L"T%lc01";
const wchar_t kToastExpCancelGroup[] =       L"T%lc02";
const wchar_t kToastExpUninstallGroup[] =    L"T%lc04";
const wchar_t kToastExpTriesOkGroup[] =      L"T%lc18";
const wchar_t kToastExpTriesErrorGroup[] =   L"T%lc28";
const wchar_t kToastActiveGroup[] =          L"T%lc40";
const wchar_t kToastUDDirFailure[] =         L"T%lc40";
const wchar_t kToastExpBaseGroup[] =         L"T%lc80";

// Generates the actual group string that gets written in the registry.
// |group| is one of the above kToast* strings and |flavor| is a number
// between 0 and 5.
//
// The big experiment in Dec 2009 used TGxx and THxx.
// The big experiment in Feb 2010 uses TKxx and TLxx.
// The big experiment in Apr 2010 uses TMxx and TNxx.
std::wstring GetExperimentGroup(const wchar_t* group, int flavor) {
  wchar_t c = flavor < 5 ? L'M' + flavor : L'X';
  return StringPrintf(group, c);
}

// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
std::wstring LocalizeUrl(const wchar_t* url) {
  std::wstring language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.
  return ReplaceStringPlaceholders(url, language.c_str(), NULL);
}

std::wstring GetUninstallSurveyUrl() {
  const wchar_t kSurveyUrl[] = L"http://www.google.com/support/chrome/bin/"
                               L"request.py?hl=$1&contact_type=uninstall";
  return LocalizeUrl(kSurveyUrl);
}

std::wstring GetWelcomeBackUrl() {
  const wchar_t kWelcomeUrl[] = L"http://www.google.com/chrome/intl/$1/"
                                L"welcomeback-new.html";
  return LocalizeUrl(kWelcomeUrl);
}

// Converts FILETIME to hours. FILETIME times are absolute times in
// 100 nanosecond units. For example 5:30 pm of June 15, 2009 is 3580464.
int FileTimeToHours(const FILETIME& time) {
  const ULONGLONG k100sNanoSecsToHours = 10000000LL * 60 * 60;
  ULARGE_INTEGER uli = {time.dwLowDateTime, time.dwHighDateTime};
  return static_cast<int>(uli.QuadPart / k100sNanoSecsToHours);
}

// Returns the directory last write time in hours since January 1, 1601.
// Returns -1 if there was an error retrieving the directory time.
int GetDirectoryWriteTimeInHours(const wchar_t* path) {
  // To open a directory you need to pass FILE_FLAG_BACKUP_SEMANTICS.
  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  HANDLE file = ::CreateFileW(path, 0, share, NULL, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return -1;
  FILETIME time;
  if (!::GetFileTime(file, NULL, NULL, &time))
    return -1;
  return FileTimeToHours(time);
}

// Returns the directory last-write time age in hours, relative to current
// time, so if it returns 14 it means that the directory was last written 14
// hours ago. Returns -1 if there was an error retrieving the directory.
int GetDirectoryWriteAgeInHours(const wchar_t* path) {
  int dir_time = GetDirectoryWriteTimeInHours(path);
  if (dir_time < 0)
    return dir_time;
  FILETIME time;
  GetSystemTimeAsFileTime(&time);
  int now_time = FileTimeToHours(time);
  if (dir_time >= now_time)
    return 0;
  return (now_time - dir_time);
}

// Launches again this same process with switch --|flag|=|value|.
// If system_level_toast is true, appends --system-level-toast.
// Does not wait for the process to terminate.
bool RelaunchSetup(const std::wstring& flag, int value,
                   bool system_level_toast) {
  CommandLine cmd_line(CommandLine::ForCurrentProcess()->GetProgram());
  cmd_line.AppendSwitchWithValue(WideToASCII(flag), IntToWString(value));
  if (system_level_toast)
    cmd_line.AppendSwitch(
      WideToASCII(installer_util::switches::kSystemLevelToast));
  return base::LaunchApp(cmd_line, false, false, NULL);
}

// For System level installs, setup.exe lives in the system temp, which
// is normally c:\windows\temp. In many cases files inside this folder
// are not accessible for execution by regular user accounts.
// This function changes the permisions so that any authenticated user
// can launch |exe| later on. This function should only be called if the
// code is running at the system level.
bool FixDACLsForExecute(const wchar_t* exe) {
  // The general strategy to is to add an ACE to the exe DACL the quick
  // and dirty way: a) read the DACL b) convert it to sddl string c) add the
  // new ACE to the string d) convert sddl string back to DACL and finally
  // e) write new dacl.
  char buff[1024];
  DWORD len = sizeof(buff);
  PSECURITY_DESCRIPTOR sd = reinterpret_cast<PSECURITY_DESCRIPTOR>(buff);
  if (!::GetFileSecurityW(exe, DACL_SECURITY_INFORMATION, sd, len, &len))
    return false;
  wchar_t* sddl = 0;
  if (!::ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,
      SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, NULL))
    return false;
  std::wstring new_sddl(sddl);
  ::LocalFree(sddl);
  sd = NULL;
  // See MSDN for the  security descriptor definition language (SDDL) syntax,
  // in our case we add "A;" generic read 'GR' and generic execute 'GX' for
  // the nt\authenticated_users 'AU' group, that becomes:
  const wchar_t kAllowACE[] = L"(A;;GRGX;;;AU)";
  // We should check that there are no special ACES for the group we
  // are interested, which is nt\authenticated_users.
  if (std::wstring::npos != new_sddl.find(L";AU)"))
    return false;
  // Specific ACEs (not inherited) need to go to the front. It is ok if we
  // are the very first one.
  size_t pos_insert = new_sddl.find(L"(");
  if (std::wstring::npos == pos_insert)
    return false;
  // All good, time to change the dacl.
  new_sddl.insert(pos_insert, kAllowACE);
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(new_sddl.c_str(),
      SDDL_REVISION_1, &sd, NULL))
    return false;
  bool rv = ::SetFileSecurityW(exe, DACL_SECURITY_INFORMATION, sd) == TRUE;
  ::LocalFree(sd);
  return rv;
}

// This function launches setup as the currently logged-in interactive
// user that is the user whose logon session is attached to winsta0\default.
// It assumes that currently we are running as SYSTEM in a non-interactive
// windowstation.
// The function fails if there is no interactive session active, basically
// the computer is on but nobody has logged in locally.
// Remote Desktop sessions do not count as interactive sessions; running this
// method as a user logged in via remote desktop will do nothing.
bool RelaunchSetupAsConsoleUser(const std::wstring& flag) {
  FilePath setup_exe = CommandLine::ForCurrentProcess()->GetProgram();
  CommandLine cmd_line(setup_exe);
  cmd_line.AppendSwitch(WideToASCII(flag));

  if (win_util::GetWinVersion() > win_util::WINVERSION_XP) {
    // Make sure that in Vista and Above we have the proper DACLs so
    // the interactive user can launch it.
    if (!FixDACLsForExecute(setup_exe.ToWStringHack().c_str())) {
      NOTREACHED();
    }
  }

  DWORD console_id = ::WTSGetActiveConsoleSessionId();
  if (console_id == 0xFFFFFFFF)
    return false;
  HANDLE user_token;
  if (!::WTSQueryUserToken(console_id, &user_token))
    return false;
  bool launched = base::LaunchAppAsUser(user_token,
                                        cmd_line.command_line_string(),
                                        false, NULL, true);
  ::CloseHandle(user_token);
  return launched;
}

}  // namespace

GoogleChromeDistribution::GoogleChromeDistribution()
    : product_guid_(kChromeGuid) {
}

// The functions below are not used by the 64-bit Windows binary -
// see the comment in google_chrome_distribution_dummy.cc
#ifndef _WIN64
bool GoogleChromeDistribution::BuildUninstallMetricsString(
    DictionaryValue* uninstall_metrics_dict, std::wstring* metrics) {
  DCHECK(NULL != metrics);
  bool has_values = false;

  for (DictionaryValue::key_iterator iter(uninstall_metrics_dict->begin_keys());
       iter != uninstall_metrics_dict->end_keys(); ++iter) {
    has_values = true;
    metrics->append(L"&");
    metrics->append(*iter);
    metrics->append(L"=");

    std::wstring value;
    uninstall_metrics_dict->GetStringWithoutPathExpansion(*iter, &value);
    metrics->append(value);
  }

  return has_values;
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const std::wstring& file_path, std::wstring* uninstall_metrics_string) {

  JSONFileValueSerializer json_serializer(FilePath::FromWStringHack(file_path));

  std::string json_error_string;
  scoped_ptr<Value> root(json_serializer.Deserialize(NULL, NULL));
  if (!root.get())
    return false;

  // Preferences should always have a dictionary root.
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return false;

  return ExtractUninstallMetrics(*static_cast<DictionaryValue*>(root.get()),
                                 uninstall_metrics_string);
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, std::wstring* uninstall_metrics_string) {
  // Make sure that the user wants us reporting metrics. If not, don't
  // add our uninstall metrics.
  bool metrics_reporting_enabled = false;
  if (!root.GetBoolean(prefs::kMetricsReportingEnabled,
                       &metrics_reporting_enabled) ||
      !metrics_reporting_enabled) {
    return false;
  }

  DictionaryValue* uninstall_metrics_dict;
  if (!root.HasKey(installer_util::kUninstallMetricsName) ||
      !root.GetDictionary(installer_util::kUninstallMetricsName,
                          &uninstall_metrics_dict)) {
    return false;
  }

  if (!BuildUninstallMetricsString(uninstall_metrics_dict,
                                   uninstall_metrics_string)) {
    return false;
  }

  return true;
}
#endif

void GoogleChromeDistribution::DoPostUninstallOperations(
    const installer::Version& version, const std::wstring& local_data_path,
    const std::wstring& distribution_data) {
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

  FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  std::wstring command = iexplore.value() + L" " + GetUninstallSurveyUrl() +
      L"&" + kVersionParam + L"=" + kVersion + L"&" + kOSParam + L"=" +
      os_version;

  std::wstring uninstall_metrics;
  if (ExtractUninstallMetricsFromFile(local_data_path, &uninstall_metrics)) {
    // The user has opted into anonymous usage data collection, so append
    // metrics and distribution data.
    command += uninstall_metrics;
    if (!distribution_data.empty()) {
      command += L"&";
      command += distribution_data;
    }
  }

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  WMIProcessUtil::Launch(command, &pid);
}

std::wstring GoogleChromeDistribution::GetAppGuid() {
  return product_guid();
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return product_name;
}

std::wstring GoogleChromeDistribution::GetAlternateApplicationName() {
  const std::wstring& alt_product_name =
      installer_util::GetLocalizedString(IDS_OEM_MAIN_SHORTCUT_NAME_BASE);
  return alt_product_name;
}

std::wstring GoogleChromeDistribution::GetBrowserAppId() {
  return kBrowserAppId;
}

std::wstring GoogleChromeDistribution::GetInstallSubDir() {
  std::wstring sub_dir(installer_util::kGoogleChromeInstallSubDir1);
  sub_dir.append(L"\\");
  sub_dir.append(installer_util::kGoogleChromeInstallSubDir2);
  return sub_dir;
}

std::wstring GoogleChromeDistribution::GetNewGoogleUpdateApKey(
    bool diff_install, installer_util::InstallStatus status,
    const std::wstring& value) {
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

std::wstring GoogleChromeDistribution::GetAppDescription() {
  const std::wstring& app_description =
      installer_util::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  return "googlechrome";
}

std::wstring GoogleChromeDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

std::wstring GoogleChromeDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

std::wstring GoogleChromeDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::wstring GoogleChromeDistribution::GetDistributionData(RegKey* key) {
  DCHECK(NULL != key);
  std::wstring sub_key(google_update::kRegPathClientState);
  sub_key.append(L"\\");
  sub_key.append(product_guid());

  RegKey client_state_key(key->Handle(), sub_key.c_str());
  std::wstring result;
  std::wstring brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value)) {
    result = google_update::kRegRLZBrandField;
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  std::wstring client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value)) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  std::wstring ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  return result;
}

std::wstring GoogleChromeDistribution::GetUninstallLinkName() {
  const std::wstring& link_name =
      installer_util::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE);
  return link_name;
}

std::wstring GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome";
}

std::wstring GoogleChromeDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(product_guid());
  return key;
}

std::wstring GoogleChromeDistribution::GetEnvVersionKey() {
  return L"CHROME_VERSION";
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
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(product_guid());
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
      !key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    LOG(INFO) << "Application key not found.";
    if (!incremental_install || !GetInstallReturnCode(install_status)) {
      LOG(INFO) << "Returning without changing application key.";
      key.Close();
      return;
    } else if (!key.Valid()) {
      reg_key.assign(google_update::kRegPathClientState);
      if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
          !key.CreateKey(product_guid().c_str(), KEY_ALL_ACCESS)) {
        LOG(ERROR) << "Failed to create application key.";
        key.Close();
        return;
      }
    }
  }

  std::wstring new_value = GoogleChromeDistribution::GetNewGoogleUpdateApKey(
      incremental_install, install_status, ap_key_value);
  if ((new_value.compare(ap_key_value) != 0) &&
      !key.WriteValue(google_update::kRegApField, new_value.c_str())) {
    LOG(ERROR) << "Failed to write value " << new_value
               << " to the registry field " << google_update::kRegApField;
  }
  key.Close();
}

// The functions below are not used by the 64-bit Windows binary -
// see the comment in google_chrome_distribution_dummy.cc
#ifndef _WIN64
// Currently we only have one experiment: the inactive user toast. Which only
// applies for users doing upgrades.
//
// There are three scenarios when this function is called:
// 1- Is a per-user-install and it updated: perform the experiment
// 2- Is a system-install and it updated : relaunch as the interactive user
// 3- It has been re-launched from the #2 case. In this case we enter
//    this function with |system_install| true and a REENTRY_SYS_UPDATE status.
void GoogleChromeDistribution::LaunchUserExperiment(
    installer_util::InstallStatus status, const installer::Version& version,
    bool system_install) {

  if (system_install) {
    if (installer_util::NEW_VERSION_UPDATED == status) {
      // We need to relaunch as the interactive user.
      RelaunchSetupAsConsoleUser(installer_util::switches::kSystemLevelToast);
      return;
    }
  } else {
    if ((installer_util::NEW_VERSION_UPDATED != status) &&
        (installer_util::REENTRY_SYS_UPDATE != status)) {
      // We are not updating or in re-launch. Exit.
      return;
    }
  }

  // currently only two equal experiment groups. 90% get the welcome back url.
  int flavor = (base::RandDouble() > 0.1) ? 0 : 1;

  std::wstring brand;
  if (GoogleUpdateSettings::GetBrand(&brand) && (brand == L"CHXX")) {
    // Testing only: the user automatically qualifies for the experiment.
    LOG(INFO) << "Experiment qualification bypass";
  } else {
    // Check browser usage inactivity by the age of the last-write time of the
    // chrome user data directory.
    std::wstring user_data_dir = installer::GetChromeUserDataPath();
    // TODO(cpu): re-enable experiment.
    const int kThirtyDays = 3000 * 24;
    int dir_age_hours = GetDirectoryWriteAgeInHours(user_data_dir.c_str());
    if (dir_age_hours < 0) {
      // This means that we failed to find the user data dir. The most likey
      // cause is that this user has not ever used chrome at all which can
      // happen in a system-level install.
      GoogleUpdateSettings::SetClient(
          GetExperimentGroup(kToastUDDirFailure, flavor));
      return;
    } else if (dir_age_hours < kThirtyDays) {
      // An active user, so it does not qualify.
      LOG(INFO) << "Chrome used in last " << dir_age_hours << " hours";
      GoogleUpdateSettings::SetClient(
          GetExperimentGroup(kToastActiveGroup, flavor));
      return;
    }
    // 1% are in the control group that qualifies but does not get drafted.
    if (base::RandDouble() > 0.99) {
      GoogleUpdateSettings::SetClient(
          GetExperimentGroup(kToastExpControlGroup, flavor));
      LOG(INFO) << "User is control group";
      return;
    }
  }
  LOG(INFO) << "User drafted for toast experiment " << flavor;
  GoogleUpdateSettings::SetClient(
      GetExperimentGroup(kToastExpBaseGroup, flavor));
  // The experiment needs to be performed in a different process because
  // google_update expects the upgrade process to be quick and nimble.
  RelaunchSetup(installer_util::switches::kInactiveUserToast, flavor,
                system_install);
}

// User qualifies for the experiment. Launch chrome with --try-chrome=flavor.
void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor,
    bool system_install) {
  bool has_welcome_url = (flavor == 0);
  // Possibly add a url to launch depending on the experiment flavor.
  std::wstring options(StringPrintf(L"--%ls=%d",
      ASCIIToUTF16(switches::kTryChromeAgain).c_str(), flavor));
  if (has_welcome_url) {
    const std::wstring url(GetWelcomeBackUrl());
    options.append(L" -- ");
    options.append(url);
  }
  // Launch chrome now. It will show the toast UI.
  int32 exit_code = 0;
  if (!installer::LaunchChromeAndWaitForResult(system_install,
                                               options, &exit_code))
    return;
  // The chrome process has exited, figure out what happened.
  const wchar_t* outcome = NULL;
  switch (exit_code) {
    case ResultCodes::NORMAL_EXIT:
      outcome = kToastExpTriesOkGroup;
      break;
    case ResultCodes::NORMAL_EXIT_CANCEL:
      outcome = kToastExpCancelGroup;
      break;
    case ResultCodes::NORMAL_EXIT_EXP2:
      outcome = kToastExpUninstallGroup;
      break;
    default:
      outcome = kToastExpTriesErrorGroup;
  };
  GoogleUpdateSettings::SetClient(GetExperimentGroup(outcome, flavor));
  if (outcome != kToastExpUninstallGroup)
    return;
  // The user wants to uninstall. This is a best effort operation. Note that
  // we waited for chrome to exit so the uninstall would not detect chrome
  // running.
  base::LaunchApp(InstallUtil::GetChromeUninstallCmd(system_install),
                  false, false, NULL);
}
#endif
