// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the methods useful for uninstalling Chrome.

#include "chrome/installer/setup/uninstall.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

// Build-time generated include file.
#include "registered_dlls.h"

namespace {

// This functions checks for any Chrome instances that are
// running and first asks them to close politely by sending a Windows message.
// If there is an error while sending message or if there are still Chrome
// procesess active after the message has been sent, this function will try
// to kill them.
void CloseAllChromeProcesses() {
  for (int j = 0; j < 4; ++j) {
    std::wstring wnd_class(L"Chrome_WidgetWin_");
    wnd_class.append(IntToWString(j));
    HWND window = FindWindowEx(NULL, NULL, wnd_class.c_str(), NULL);
    while (window) {
      HWND tmpWnd = window;
      window = FindWindowEx(NULL, window, wnd_class.c_str(), NULL);
      if (!SendMessageTimeout(tmpWnd, WM_CLOSE, 0, 0, SMTO_BLOCK, 3000, NULL) &&
          (GetLastError() == ERROR_TIMEOUT)) {
        base::CleanupProcesses(installer_util::kChromeExe, 0,
                               ResultCodes::HUNG, NULL);
        return;
      }
    }
  }

  // If asking politely didn't work, wait for 15 seconds and then kill all
  // chrome.exe. This check is just in case Chrome is ignoring WM_CLOSE
  // messages.
  base::CleanupProcesses(installer_util::kChromeExe, 15000,
                         ResultCodes::HUNG, NULL);
}

// This method tries to figure out if current user has registered Chrome.
// It returns true iff:
// - Software\Clients\StartMenuInternet\Chromium\"" key has a valid value.
// - The value is same as chrome.exe path for the current installation.
bool CurrentUserHasDefaultBrowser(bool system_uninstall) {
  std::wstring reg_key(ShellUtil::kRegStartMenuInternet);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  reg_key.append(L"\\" + dist->GetApplicationName() + ShellUtil::kRegShellOpen);
  RegKey key(HKEY_LOCAL_MACHINE, reg_key.c_str());
  std::wstring reg_exe;
  if (key.ReadValue(L"", &reg_exe) && reg_exe.length() > 2) {
    std::wstring chrome_exe = installer::GetChromeInstallPath(system_uninstall);
    file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
    reg_exe = reg_exe.substr(1, reg_exe.length() - 2);
    if ((reg_exe.size() == chrome_exe.size()) &&
        (std::equal(chrome_exe.begin(), chrome_exe.end(),
                    reg_exe.begin(), CaseInsensitiveCompare<wchar_t>())))
      return true;
  }

  return false;
}

// This method deletes Chrome shortcut folder from Windows Start menu. It
// checks system_uninstall to see if the shortcut is in all users start menu
// or current user start menu.
// We try to remove the standard desktop shortcut but if that fails we try
// to remove the alternate desktop shortcut. Only one of them should be
// present in a given install but at this point we don't know which one.
void DeleteChromeShortcuts(bool system_uninstall) {
  FilePath shortcut_path;
  if (system_uninstall) {
    PathService::Get(base::DIR_COMMON_START_MENU, &shortcut_path);
    if (!ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER |
                                                ShellUtil::SYSTEM_LEVEL, false))
      ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER |
                                             ShellUtil::SYSTEM_LEVEL, true);

    ShellUtil::RemoveChromeQuickLaunchShortcut(ShellUtil::CURRENT_USER |
                                               ShellUtil::SYSTEM_LEVEL);
  } else {
    PathService::Get(base::DIR_START_MENU, &shortcut_path);
    if (!ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER, false))
      ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER, true);

    ShellUtil::RemoveChromeQuickLaunchShortcut(ShellUtil::CURRENT_USER);
  }
  if (shortcut_path.empty()) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    shortcut_path = shortcut_path.Append(dist->GetApplicationName());
    LOG(INFO) << "Deleting shortcut " << shortcut_path.value();
    if (!file_util::Delete(shortcut_path, true))
      LOG(ERROR) << "Failed to delete folder: " << shortcut_path.value();
  }
}

// Deletes empty parent & empty grandparent dir of given path.
bool DeleteEmptyParentDir(const std::wstring& path) {
  bool ret = true;
  std::wstring parent_dir = file_util::GetDirectoryFromPath(path);
  if (!parent_dir.empty() && file_util::IsDirectoryEmpty(parent_dir)) {
    if (!file_util::Delete(parent_dir, true)) {
      ret = false;
      LOG(ERROR) << "Failed to delete folder: " << parent_dir;
    }

    parent_dir = file_util::GetDirectoryFromPath(parent_dir);
    if (!parent_dir.empty() && file_util::IsDirectoryEmpty(parent_dir)) {
      if (!file_util::Delete(parent_dir, true)) {
        ret = false;
        LOG(ERROR) << "Failed to delete folder: " << parent_dir;
      }
    }
  }
  return ret;
}

enum DeleteResult {
  DELETE_SUCCEEDED,
  DELETE_FAILED,
  DELETE_REQUIRES_REBOOT
};

// Deletes all installed files of Chromium and Folders or schedules them for
// deletion on reboot if they are in use. Before deleting it
// needs to move setup.exe in a temp folder because the current process
// is using that file.
// Returns DELETE_SUCCEEDED if all files were successfully delete.
// Returns DELETE_FAILED if it could not get the path to the install dir.
// Returns DELETE_REQUIRES_REBOOT if the files were in use and so were
// scheduled for deletion on next reboot.
DeleteResult DeleteFilesAndFolders(const std::wstring& exe_path,
    bool system_uninstall, const installer::Version& installed_version,
    std::wstring* local_state_path, bool delete_profile) {
  std::wstring install_path(installer::GetChromeInstallPath(system_uninstall));
  if (install_path.empty()) {
    LOG(ERROR) << "Could not get installation destination path.";
    return DELETE_FAILED;  // Nothing else we can do to uninstall, so we return.
  } else {
    LOG(INFO) << "install destination path: " << install_path;
  }

  // Move setup.exe to the temp path.
  std::wstring setup_exe(installer::GetInstallerPathUnderChrome(
      install_path, installed_version.GetString()));
  file_util::AppendToPath(&setup_exe, file_util::GetFilenameFromPath(exe_path));

  FilePath temp_file;
  if (!file_util::CreateTemporaryFile(&temp_file)) {
    LOG(ERROR) << "Failed to create temporary file for setup.exe.";
  } else {
    FilePath setup_exe_path = FilePath::FromWStringHack(setup_exe);
    file_util::Move(setup_exe_path, temp_file);
  }

  // Obtain the location of the user profile data. Chrome Frame needs to
  // build this path manually since it doesn't use the Chrome default dir.
  FilePath user_local_state;
  bool got_local_state = false;
  if (InstallUtil::IsChromeFrameProcess()) {
    got_local_state =
        chrome::GetChromeFrameUserDataDirectory(&user_local_state);
  } else {
    got_local_state = chrome::GetDefaultUserDataDirectory(&user_local_state);
  }

  // Move the browser's persisted local state
  if (got_local_state) {
    FilePath user_local_file(
        user_local_state.Append(chrome::kLocalStateFilename));
    FilePath path = FilePath::FromWStringHack(*local_state_path);
    if (!file_util::CreateTemporaryFile(&path))
      LOG(ERROR) << "Failed to create temporary file for Local State.";
    else
      file_util::CopyFile(user_local_file, path);
  } else {
    LOG(ERROR) << "Could not retrieve user's profile directory.";
  }

  DeleteResult result = DELETE_SUCCEEDED;

  LOG(INFO) << "Deleting install path " << install_path;
  if (!file_util::Delete(install_path, true)) {
    LOG(ERROR) << "Failed to delete folder (1st try): " << install_path;
    if (InstallUtil::IsChromeFrameProcess()) {
      // We don't try killing Chrome processes for Chrome Frame builds since
      // that is unlikely to help. Instead, schedule files for deletion and
      // return a value that will trigger a reboot prompt.
      ScheduleDirectoryForDeletion(install_path.c_str());
      result = DELETE_REQUIRES_REBOOT;
    } else {
      // Try closing any running chrome processes and deleting files once again.
      CloseAllChromeProcesses();
      if (!file_util::Delete(install_path, true)) {
        LOG(ERROR) << "Failed to delete folder (2nd try): " << install_path;
        result = DELETE_FAILED;
      }
    }
  }

  if (delete_profile && got_local_state) {
    LOG(INFO) << "Deleting user profile" << user_local_state.value();
    if (!file_util::Delete(user_local_state, true)) {
      LOG(ERROR) << "Failed to delete user profile dir: "
                 << user_local_state.value();
      if (InstallUtil::IsChromeFrameProcess()) {
        ScheduleDirectoryForDeletion(user_local_state.value().c_str());
        result = DELETE_REQUIRES_REBOOT;
      } else {
        result = DELETE_FAILED;
      }
    }
    DeleteEmptyParentDir(user_local_state.value());
  }

  // Now check and delete if the parent directories are empty
  // For example Google\Chrome or Chromium
  DeleteEmptyParentDir(install_path);
  return result;
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool DeleteRegistryKey(RegKey& key, const std::wstring& key_path) {
  LOG(INFO) << "Deleting registry key " << key_path;
  if (!key.DeleteKey(key_path.c_str()) &&
      ::GetLastError() != ERROR_MOD_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path;
    return false;
  }
  return true;
}

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool DeleteRegistryValue(HKEY reg_root, const std::wstring& key_path,
                         const std::wstring& value_name) {
  RegKey key(reg_root, key_path.c_str(), KEY_ALL_ACCESS);
  LOG(INFO) << "Deleting registry value " << value_name;
  if (key.ValueExists(value_name.c_str()) &&
      !key.DeleteValue(value_name.c_str())) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name;
    return false;
  }
  return true;
}

// This method checks if Chrome is currently running or if the user has
// cancelled the uninstall operation by clicking Cancel on the confirmation
// box that Chrome pops up.
installer_util::InstallStatus IsChromeActiveOrUserCancelled(
    bool system_uninstall) {
  static const std::wstring kCmdLineOptions(L" --uninstall");
  int32 exit_code = ResultCodes::NORMAL_EXIT;

  // Here we want to save user from frustration (in case of Chrome crashes)
  // and continue with the uninstallation as long as chrome.exe process exit
  // code is NOT one of the following:
  // - UNINSTALL_CHROME_ALIVE - chrome.exe is currently running
  // - UNINSTALL_USER_CANCEL - User cancelled uninstallation
  // - HUNG - chrome.exe was killed by HuntForZombieProcesses() (until we can
  //          give this method some brains and not kill chrome.exe launched
  //          by us, we will not uninstall if we get this return code).
  LOG(INFO) << "Launching Chrome to do uninstall tasks.";
  if (installer::LaunchChromeAndWaitForResult(system_uninstall,
                                              kCmdLineOptions,
                                              &exit_code)) {
    LOG(INFO) << "chrome.exe launched for uninstall confirmation returned: "
              << exit_code;
    if ((exit_code == ResultCodes::UNINSTALL_CHROME_ALIVE) ||
        (exit_code == ResultCodes::UNINSTALL_USER_CANCEL) ||
        (exit_code == ResultCodes::HUNG)) {
      return installer_util::UNINSTALL_CANCELLED;
    } else if (exit_code == ResultCodes::UNINSTALL_DELETE_PROFILE) {
      return installer_util::UNINSTALL_DELETE_PROFILE;
    }
  } else {
    LOG(ERROR) << "Failed to launch chrome.exe for uninstall confirmation.";
  }

  return installer_util::UNINSTALL_CONFIRMED;
}
}  // namespace


bool installer_setup::DeleteChromeRegistrationKeys(HKEY root,
    const std::wstring& browser_entry_suffix,
    installer_util::InstallStatus& exit_code) {
  RegKey key(root, L"", KEY_ALL_ACCESS);

  // Delete Software\Classes\ChromeHTML,
  std::wstring html_prog_id(ShellUtil::kRegClasses);
  file_util::AppendToPath(&html_prog_id, ShellUtil::kChromeHTMLProgId);
  html_prog_id.append(browser_entry_suffix);
  DeleteRegistryKey(key, html_prog_id);

  // Delete Software\Clients\StartMenuInternet\Chromium
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring set_access_key(ShellUtil::kRegStartMenuInternet);
  file_util::AppendToPath(&set_access_key, dist->GetApplicationName());
  set_access_key.append(browser_entry_suffix);
  DeleteRegistryKey(key, set_access_key);

  // We have renamed the StartMenuInternet\chrome.exe to
  // StartMenuInternet\Chromium so for old users we still need to delete
  // the old key.
  std::wstring old_set_access_key(ShellUtil::kRegStartMenuInternet);
  file_util::AppendToPath(&old_set_access_key, installer_util::kChromeExe);
  DeleteRegistryKey(key, old_set_access_key);

  // Delete Software\RegisteredApplications\Chromium
  DeleteRegistryValue(root, ShellUtil::kRegRegisteredApplications,
                      dist->GetApplicationName() + browser_entry_suffix);

  // Delete Software\Classes\Applications\chrome.exe
  std::wstring app_key(ShellUtil::kRegClasses);
  file_util::AppendToPath(&app_key, L"Applications");
  file_util::AppendToPath(&app_key, installer_util::kChromeExe);
  DeleteRegistryKey(key, app_key);

  // Delete the App Paths key that lets explorer find Chrome.
  std::wstring app_path_key(ShellUtil::kAppPathsRegistryKey);
  file_util::AppendToPath(&app_path_key, installer_util::kChromeExe);
  DeleteRegistryKey(key, app_path_key);

  // Cleanup OpenWithList
  for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
    std::wstring open_with_key(ShellUtil::kRegClasses);
    file_util::AppendToPath(&open_with_key, ShellUtil::kFileAssociations[i]);
    file_util::AppendToPath(&open_with_key, L"OpenWithList");
    file_util::AppendToPath(&open_with_key, installer_util::kChromeExe);
    DeleteRegistryKey(key, open_with_key);
  }

  key.Close();
  exit_code = installer_util::UNINSTALL_SUCCESSFUL;
  return true;
}

void installer_setup::RemoveLegacyRegistryKeys() {
  // We used to register Chrome to handle crx files, but this turned out
  // to be not worth the hassle. Remove these old registry entries if
  // they exist. See: http://codereview.chromium.org/210007

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kChromeExtProgId[] = L"ChromeExt";
#else
const wchar_t kChromeExtProgId[] = L"ChromiumExt";
#endif

  HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (size_t i = 0; i < arraysize(roots); ++i) {
    RegKey key(roots[i], L"", KEY_ALL_ACCESS);

    std::wstring suffix;
    if (roots[i] == HKEY_LOCAL_MACHINE &&
        !ShellUtil::GetUserSpecificDefaultBrowserSuffix(&suffix))
      suffix = L"";

    // Delete Software\Classes\ChromeExt,
    std::wstring ext_prog_id(ShellUtil::kRegClasses);
    file_util::AppendToPath(&ext_prog_id, kChromeExtProgId);
    ext_prog_id.append(suffix);
    DeleteRegistryKey(key, ext_prog_id);

    // Delete Software\Classes\.crx,
    std::wstring ext_association(ShellUtil::kRegClasses);
    ext_association.append(L"\\.");
    ext_association.append(chrome::kExtensionFileExtension);
    DeleteRegistryKey(key, ext_association);
  }
}

installer_util::InstallStatus installer_setup::UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    bool remove_all, bool force_uninstall,
    const CommandLine& cmd_line, const wchar_t* cmd_params) {
  installer_util::InstallStatus status = installer_util::UNINSTALL_CONFIRMED;
  std::wstring suffix;
  if (!ShellUtil::GetUserSpecificDefaultBrowserSuffix(&suffix))
    suffix = L"";

  if (force_uninstall) {
    // Since --force-uninstall command line option is used, we are going to
    // do silent uninstall. Try to close all running Chrome instances.
    if (!InstallUtil::IsChromeFrameProcess())
      CloseAllChromeProcesses();
  } else {  // no --force-uninstall so lets show some UI dialog boxes.
    status = IsChromeActiveOrUserCancelled(system_uninstall);
    if (status != installer_util::UNINSTALL_CONFIRMED &&
        status != installer_util::UNINSTALL_DELETE_PROFILE)
      return status;

    // Check if we need admin rights to cleanup HKLM. If we do, try to launch
    // another uninstaller (silent) in elevated mode to do HKLM cleanup.
    // And continue uninstalling in the current process also to do HKCU cleanup.
    if (remove_all &&
        (!suffix.empty() || CurrentUserHasDefaultBrowser(system_uninstall)) &&
        !::IsUserAnAdmin() &&
        (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) &&
        !cmd_line.HasSwitch(installer_util::switches::kRunAsAdmin)) {
      std::wstring exe = cmd_line.program();
      std::wstring params(cmd_params);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      params.append(L" --");
      params.append(installer_util::switches::kRunAsAdmin);
      // Append --remove-chrome-registration to remove registry keys only.
      params.append(L" --");
      params.append(installer_util::switches::kRemoveChromeRegistration);
      if (!suffix.empty()) {
        params.append(L" --");
        params.append(installer_util::switches::kRegisterChromeBrowserSuffix);
        params.append(L"=\"" + suffix + L"\"");
      }
      DWORD exit_code = installer_util::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(exe, params, &exit_code);
    }
  }

  // Get the version of installed Chrome (if any)
  scoped_ptr<installer::Version>
      installed_version(InstallUtil::GetChromeVersion(system_uninstall));

  // Chrome is not in use so lets uninstall Chrome by deleting various files
  // and registry entries. Here we will just make best effort and keep going
  // in case of errors.

  // First delete shortcuts from Start->Programs, Desktop & Quick Launch.
  DeleteChromeShortcuts(system_uninstall);

  // Delete the registry keys (Uninstall key and Version key).
  HKEY reg_root = system_uninstall ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key(reg_root, L"", KEY_ALL_ACCESS);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Note that we must retrieve the distribution-specific data before deleting
  // dist->GetVersionKey().
  std::wstring distribution_data(dist->GetDistributionData(&key));

  // Remove Control Panel uninstall link and Omaha product key.
  DeleteRegistryKey(key, dist->GetUninstallRegPath());
  DeleteRegistryKey(key, dist->GetVersionKey());

  // Remove all Chrome registration keys.
  installer_util::InstallStatus ret = installer_util::UNKNOWN_STATUS;
  DeleteChromeRegistrationKeys(reg_root, suffix, ret);

  // For user level install also we end up creating some keys in HKLM if user
  // sets Chrome as default browser. So delete those as well (needs admin).
  if (remove_all && !system_uninstall &&
      (!suffix.empty() || CurrentUserHasDefaultBrowser(system_uninstall)))
    DeleteChromeRegistrationKeys(HKEY_LOCAL_MACHINE, suffix, ret);

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    // Delete media player registry key that exists only in HKLM.
    RegKey hklm_key(HKEY_LOCAL_MACHINE, L"", KEY_ALL_ACCESS);
    std::wstring reg_path(installer::kMediaPlayerRegPath);
    file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
    DeleteRegistryKey(hklm_key, reg_path);
    hklm_key.Close();

    if (installed_version.get()) {
      // Unregister any dll servers that we may have registered.
      std::wstring dll_path(installer::GetChromeInstallPath(system_uninstall));
      file_util::AppendToPath(&dll_path, installed_version->GetString());

      scoped_ptr<WorkItemList> dll_list(WorkItem::CreateWorkItemList());
      if (InstallUtil::BuildDLLRegistrationList(dll_path, kDllsToRegister,
                                                kNumDllsToRegister, false,
                                                dll_list.get())) {
        dll_list->Do();
      }
    }
  }

  if (!installed_version.get())
    return installer_util::UNINSTALL_SUCCESSFUL;

  // Finally delete all the files from Chrome folder after moving setup.exe
  // and the user's Local State to a temp location.
  bool delete_profile = (status == installer_util::UNINSTALL_DELETE_PROFILE) ||
      (cmd_line.HasSwitch(installer_util::switches::kDeleteProfile));
  std::wstring local_state_path;
  ret = installer_util::UNINSTALL_SUCCESSFUL;

  DeleteResult delete_result = DeleteFilesAndFolders(exe_path,
      system_uninstall, *installed_version, &local_state_path, delete_profile);
  if (delete_result == DELETE_FAILED) {
    ret = installer_util::UNINSTALL_FAILED;
  } else if (delete_result == DELETE_REQUIRES_REBOOT) {
    ret = installer_util::UNINSTALL_REQUIRES_REBOOT;
  }

  if (!force_uninstall) {
    LOG(INFO) << "Uninstallation complete. Launching Uninstall survey.";
    dist->DoPostUninstallOperations(*installed_version, local_state_path,
                                    distribution_data);
  }

  // Try and delete the preserved local state once the post-install
  // operations are complete.
  if (!local_state_path.empty())
    file_util::Delete(local_state_path, false);

  return ret;
}