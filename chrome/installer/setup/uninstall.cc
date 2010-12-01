// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the methods useful for uninstalling Chrome.

#include "chrome/installer/setup/uninstall.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_handle.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
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
#include "registered_dlls.h"  // NOLINT

using base::win::RegKey;
using installer_util::InstallStatus;

namespace installer {

// This functions checks for any Chrome instances that are
// running and first asks them to close politely by sending a Windows message.
// If there is an error while sending message or if there are still Chrome
// procesess active after the message has been sent, this function will try
// to kill them.
void CloseAllChromeProcesses() {
  for (int j = 0; j < 4; ++j) {
    std::wstring wnd_class(L"Chrome_WidgetWin_");
    wnd_class.append(base::IntToString16(j));
    HWND window = FindWindowEx(NULL, NULL, wnd_class.c_str(), NULL);
    while (window) {
      HWND tmpWnd = window;
      window = FindWindowEx(NULL, window, wnd_class.c_str(), NULL);
      if (!SendMessageTimeout(tmpWnd, WM_CLOSE, 0, 0, SMTO_BLOCK, 3000, NULL) &&
          (GetLastError() == ERROR_TIMEOUT)) {
        base::CleanupProcesses(installer_util::kChromeExe, 0,
                               ResultCodes::HUNG, NULL);
        base::CleanupProcesses(installer_util::kNaClExe, 0,
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
  base::CleanupProcesses(installer_util::kNaClExe, 15000,
                         ResultCodes::HUNG, NULL);
}

// Attempts to close the Chrome Frame helper process by sending WM_CLOSE
// messages to its window, or just killing it if that doesn't work.
void CloseChromeFrameHelperProcess() {
  HWND window = FindWindow(installer_util::kChromeFrameHelperWndClass, NULL);
  if (!::IsWindow(window))
    return;

  const DWORD kWaitMs = 3000;

  DWORD pid = 0;
  ::GetWindowThreadProcessId(window, &pid);
  DCHECK_NE(pid, 0U);
  ScopedHandle process(::OpenProcess(SYNCHRONIZE, FALSE, pid));
  PLOG_IF(INFO, !process) << "Failed to open process: " << pid;

  bool kill = true;
  if (SendMessageTimeout(window, WM_CLOSE, 0, 0, SMTO_BLOCK, kWaitMs, NULL) &&
      process) {
    VLOG(1) << "Waiting for " << installer_util::kChromeFrameHelperExe;
    DWORD wait = ::WaitForSingleObject(process, kWaitMs);
    if (wait != WAIT_OBJECT_0) {
      LOG(WARNING) << "Wait for " << installer_util::kChromeFrameHelperExe
                   << " to exit failed or timed out.";
    } else {
      kill = false;
      VLOG(1) << installer_util::kChromeFrameHelperExe << " exited normally.";
    }
  }

  if (kill) {
    VLOG(1) << installer_util::kChromeFrameHelperExe << " hung.  Killing.";
    base::CleanupProcesses(installer_util::kChromeFrameHelperExe, 0,
                           ResultCodes::HUNG, NULL);
  }
}

// This method tries to figure out if current user has registered Chrome.
// It returns true iff:
// - Software\Clients\StartMenuInternet\Chromium\"" key has a valid value.
// - The value is same as chrome.exe path for the current installation.
bool CurrentUserHasDefaultBrowser(const Product& product) {
  std::wstring reg_key(ShellUtil::kRegStartMenuInternet);
  reg_key.append(L"\\" + product.distribution()->GetApplicationName() +
                 ShellUtil::kRegShellOpen);
  RegKey key(HKEY_LOCAL_MACHINE, reg_key.c_str(), KEY_READ);
  std::wstring reg_exe;
  if (key.ReadValue(L"", &reg_exe) && reg_exe.length() > 2) {
    FilePath chrome_exe(product.package().path()
        .Append(installer_util::kChromeExe));
    // The path in the registry will always have quotes.
    reg_exe = reg_exe.substr(1, reg_exe.length() - 2);
    if (FilePath::CompareEqualIgnoreCase(reg_exe, chrome_exe.value()))
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
void DeleteChromeShortcuts(const Product& product) {
  if (product.distribution()->GetType() !=
      BrowserDistribution::CHROME_BROWSER) {
    VLOG(1) << __FUNCTION__ " called for non-CHROME distribution";
    return;
  }

  FilePath shortcut_path;
  if (product.system_level()) {
    PathService::Get(base::DIR_COMMON_START_MENU, &shortcut_path);
    if (!ShellUtil::RemoveChromeDesktopShortcut(product.distribution(),
        ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL, false)) {
      ShellUtil::RemoveChromeDesktopShortcut(product.distribution(),
          ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL, true);
    }

    ShellUtil::RemoveChromeQuickLaunchShortcut(product.distribution(),
        ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL);
  } else {
    PathService::Get(base::DIR_START_MENU, &shortcut_path);
    if (!ShellUtil::RemoveChromeDesktopShortcut(product.distribution(),
        ShellUtil::CURRENT_USER, false)) {
      ShellUtil::RemoveChromeDesktopShortcut(product.distribution(),
          ShellUtil::CURRENT_USER, true);
    }

    ShellUtil::RemoveChromeQuickLaunchShortcut(product.distribution(),
        ShellUtil::CURRENT_USER);
  }
  if (shortcut_path.empty()) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    shortcut_path = shortcut_path.Append(
        product.distribution()->GetAppShortCutName());
    VLOG(1) << "Deleting shortcut " << shortcut_path.value();
    if (!file_util::Delete(shortcut_path, true))
      LOG(ERROR) << "Failed to delete folder: " << shortcut_path.value();
  }
}

bool ScheduleParentAndGrandparentForDeletion(const FilePath& path) {
  FilePath parent_dir = path.DirName();
  bool ret = ScheduleFileSystemEntityForDeletion(parent_dir.value().c_str());
  if (!ret) {
    LOG(ERROR) << "Failed to schedule parent dir for deletion: "
               << parent_dir.value();
  } else {
    FilePath grandparent_dir(parent_dir.DirName());
    ret = ScheduleFileSystemEntityForDeletion(grandparent_dir.value().c_str());
    if (!ret) {
      LOG(ERROR) << "Failed to schedule grandparent dir for deletion: "
                 << grandparent_dir.value();
    }
  }
  return ret;
}

// Deletes empty parent & empty grandparent dir of given path.
bool DeleteEmptyParentDir(const FilePath& path) {
  bool ret = true;
  FilePath parent_dir = path.DirName();
  if (!parent_dir.empty() && file_util::IsDirectoryEmpty(parent_dir)) {
    if (!file_util::Delete(parent_dir, true)) {
      ret = false;
      LOG(ERROR) << "Failed to delete folder: " << parent_dir.value();
    }

    parent_dir = parent_dir.DirName();
    if (!parent_dir.empty() && file_util::IsDirectoryEmpty(parent_dir)) {
      if (!file_util::Delete(parent_dir, true)) {
        ret = false;
        LOG(ERROR) << "Failed to delete folder: " << parent_dir.value();
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

FilePath GetLocalStateFolder(const Product& product) {
  // chrome_frame will be true for CHROME_FRAME and CEEE.
  bool chrome_frame = (product.distribution()->GetType() !=
                       BrowserDistribution::CHROME_BROWSER);
  // Obtain the location of the user profile data. Chrome Frame needs to
  // build this path manually since it doesn't use the Chrome default dir.
  FilePath local_state_folder;
  if (chrome_frame) {
    chrome::GetChromeFrameUserDataDirectory(&local_state_folder);
  } else {
    chrome::GetDefaultUserDataDirectory(&local_state_folder);
  }

  LOG_IF(ERROR, local_state_folder.empty())
      << "Could not retrieve user's profile directory.";

  return local_state_folder;
}

// Creates a copy of the local state file and returns a path to the copy.
FilePath BackupLocalStateFile(const FilePath& local_state_folder) {
  FilePath backup;
  FilePath state_file(local_state_folder.Append(chrome::kLocalStateFilename));
  if (!file_util::CreateTemporaryFile(&backup)) {
    LOG(ERROR) << "Failed to create temporary file for Local State.";
  } else {
    file_util::CopyFile(state_file, backup);
  }
  return backup;
}

// Copies the local state to the temp folder and then deletes it.
// The path to the copy is returned via the local_state_copy parameter.
DeleteResult DeleteLocalState(const Product& product) {
  FilePath user_local_state(GetLocalStateFolder(product));
  if (user_local_state.empty())
    return DELETE_SUCCEEDED;

  DeleteResult result = DELETE_SUCCEEDED;
  VLOG(1) << "Deleting user profile" << user_local_state.value();
  if (!file_util::Delete(user_local_state, true)) {
    LOG(ERROR) << "Failed to delete user profile dir: "
               << user_local_state.value();
    if (product.distribution()->GetType() ==
        BrowserDistribution::CHROME_FRAME) {
      ScheduleDirectoryForDeletion(user_local_state.value().c_str());
      result = DELETE_REQUIRES_REBOOT;
    } else {
      result = DELETE_FAILED;
    }
  }

  if (result == DELETE_REQUIRES_REBOOT) {
    ScheduleParentAndGrandparentForDeletion(user_local_state);
  } else {
    DeleteEmptyParentDir(user_local_state);
  }

  return result;
}

bool MoveSetupOutOfInstallFolder(const Package& package,
                                 const FilePath& setup_path,
                                 const Version& installed_version) {
  bool ret = false;
  FilePath setup_exe(package.GetInstallerDirectory(installed_version)
      .Append(setup_path.BaseName()));
  FilePath temp_file;
  if (!file_util::CreateTemporaryFile(&temp_file)) {
    LOG(ERROR) << "Failed to create temporary file for setup.exe.";
  } else {
    ret = file_util::Move(setup_exe, temp_file);
  }
  return ret;
}

DeleteResult DeleteFilesAndFolders(const Package& package,
    const installer::Version& installed_version) {
  VLOG(1) << "DeleteFilesAndFolders: " << package.path().value();
  if (package.path().empty()) {
    LOG(ERROR) << "Could not get installation destination path.";
    return DELETE_FAILED;  // Nothing else we can do to uninstall, so we return.
  }

  DeleteResult result = DELETE_SUCCEEDED;

  VLOG(1) << "Deleting install path " << package.path().value();
  if (!file_util::Delete(package.path(), true)) {
    LOG(ERROR) << "Failed to delete folder (1st try): "
               << package.path().value();
    if (FindProduct(package.products(),
                    BrowserDistribution::CHROME_FRAME)) {
      // We don't try killing Chrome processes for Chrome Frame builds since
      // that is unlikely to help. Instead, schedule files for deletion and
      // return a value that will trigger a reboot prompt.
      ScheduleDirectoryForDeletion(package.path().value().c_str());
      result = DELETE_REQUIRES_REBOOT;
    } else {
      // Try closing any running chrome processes and deleting files once again.
      CloseAllChromeProcesses();
      if (!file_util::Delete(package.path(), true)) {
        LOG(ERROR) << "Failed to delete folder (2nd try): "
                   << package.path().value();
        result = DELETE_FAILED;
      }
    }
  }

  if (result == DELETE_REQUIRES_REBOOT) {
    // If we need a reboot to continue, schedule the parent directories for
    // deletion unconditionally. If they are not empty, the session manager
    // will not delete them on reboot.
    ScheduleParentAndGrandparentForDeletion(package.path());
  } else {
    // Now check and delete if the parent directories are empty
    // For example Google\Chrome or Chromium
    DeleteEmptyParentDir(package.path());
  }
  return result;
}

// This method checks if Chrome is currently running or if the user has
// cancelled the uninstall operation by clicking Cancel on the confirmation
// box that Chrome pops up.
InstallStatus IsChromeActiveOrUserCancelled(const Product& product) {
  int32 exit_code = ResultCodes::NORMAL_EXIT;
  CommandLine options(CommandLine::NO_PROGRAM);
  options.AppendSwitch(installer_util::switches::kUninstall);

  // Here we want to save user from frustration (in case of Chrome crashes)
  // and continue with the uninstallation as long as chrome.exe process exit
  // code is NOT one of the following:
  // - UNINSTALL_CHROME_ALIVE - chrome.exe is currently running
  // - UNINSTALL_USER_CANCEL - User cancelled uninstallation
  // - HUNG - chrome.exe was killed by HuntForZombieProcesses() (until we can
  //          give this method some brains and not kill chrome.exe launched
  //          by us, we will not uninstall if we get this return code).
  VLOG(1) << "Launching Chrome to do uninstall tasks.";
  if (product.LaunchChromeAndWait(options, &exit_code)) {
    VLOG(1) << "chrome.exe launched for uninstall confirmation returned: "
            << exit_code;
    if ((exit_code == ResultCodes::UNINSTALL_CHROME_ALIVE) ||
        (exit_code == ResultCodes::UNINSTALL_USER_CANCEL) ||
        (exit_code == ResultCodes::HUNG))
      return installer_util::UNINSTALL_CANCELLED;

    if (exit_code == ResultCodes::UNINSTALL_DELETE_PROFILE)
      return installer_util::UNINSTALL_DELETE_PROFILE;
  } else {
    PLOG(ERROR) << "Failed to launch chrome.exe for uninstall confirmation.";
  }

  return installer_util::UNINSTALL_CONFIRMED;
}

bool ShouldDeleteProfile(const CommandLine& cmd_line, InstallStatus status,
                         const Product& product) {
  bool should_delete = false;

  // Chrome Frame uninstallations always want to delete the profile (we have no
  // UI to prompt otherwise and the profile stores no useful data anyway)
  // unless they are managed by MSI. MSI uninstalls will explicitly include
  // the --delete-profile flag to distinguish them from MSI upgrades.
  if (product.distribution()->GetType() !=
      BrowserDistribution::CHROME_BROWSER && !product.IsMsi()) {
    should_delete = true;
  } else {
    should_delete =
        status == installer_util::UNINSTALL_DELETE_PROFILE ||
        cmd_line.HasSwitch(installer_util::switches::kDeleteProfile);
  }

  return should_delete;
}

bool DeleteChromeRegistrationKeys(BrowserDistribution* dist, HKEY root,
                                  const std::wstring& browser_entry_suffix,
                                  InstallStatus& exit_code) {
  if (!dist->CanSetAsDefault()) {
    // We should have never set those keys.
    return true;
  }

  RegKey key(root, L"", KEY_ALL_ACCESS);
  if (!key.Valid()) {
    PLOG(ERROR) << "DeleteChromeRegistrationKeys: failed to open root key";
  }

  // Delete Software\Classes\ChromeHTML,
  std::wstring html_prog_id(ShellUtil::kRegClasses);
  file_util::AppendToPath(&html_prog_id, ShellUtil::kChromeHTMLProgId);
  html_prog_id.append(browser_entry_suffix);
  InstallUtil::DeleteRegistryKey(key, html_prog_id);

  // Delete Software\Clients\StartMenuInternet\Chromium
  std::wstring set_access_key(ShellUtil::kRegStartMenuInternet);
  file_util::AppendToPath(&set_access_key, dist->GetApplicationName());
  set_access_key.append(browser_entry_suffix);
  InstallUtil::DeleteRegistryKey(key, set_access_key);

  // We have renamed the StartMenuInternet\chrome.exe to
  // StartMenuInternet\Chromium so for old users we still need to delete
  // the old key.
  std::wstring old_set_access_key(ShellUtil::kRegStartMenuInternet);
  file_util::AppendToPath(&old_set_access_key, installer_util::kChromeExe);
  InstallUtil::DeleteRegistryKey(key, old_set_access_key);

  // Delete Software\RegisteredApplications\Chromium
  InstallUtil::DeleteRegistryValue(root, ShellUtil::kRegRegisteredApplications,
      dist->GetApplicationName() + browser_entry_suffix);

  // Delete Software\Classes\Applications\chrome.exe
  std::wstring app_key(ShellUtil::kRegClasses);
  file_util::AppendToPath(&app_key, L"Applications");
  file_util::AppendToPath(&app_key, installer_util::kChromeExe);
  InstallUtil::DeleteRegistryKey(key, app_key);

  // Delete the App Paths key that lets explorer find Chrome.
  std::wstring app_path_key(ShellUtil::kAppPathsRegistryKey);
  file_util::AppendToPath(&app_path_key, installer_util::kChromeExe);
  InstallUtil::DeleteRegistryKey(key, app_path_key);

  // Cleanup OpenWithList
  for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
    std::wstring open_with_key(ShellUtil::kRegClasses);
    file_util::AppendToPath(&open_with_key, ShellUtil::kFileAssociations[i]);
    file_util::AppendToPath(&open_with_key, L"OpenWithList");
    file_util::AppendToPath(&open_with_key, installer_util::kChromeExe);
    InstallUtil::DeleteRegistryKey(key, open_with_key);
  }

  key.Close();
  exit_code = installer_util::UNINSTALL_SUCCESSFUL;
  return true;
}

void RemoveLegacyRegistryKeys(BrowserDistribution* dist) {
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
        !ShellUtil::GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
      suffix = L"";

    // Delete Software\Classes\ChromeExt,
    std::wstring ext_prog_id(ShellUtil::kRegClasses);
    file_util::AppendToPath(&ext_prog_id, kChromeExtProgId);
    ext_prog_id.append(suffix);
    InstallUtil::DeleteRegistryKey(key, ext_prog_id);

    // Delete Software\Classes\.crx,
    std::wstring ext_association(ShellUtil::kRegClasses);
    ext_association.append(L"\\");
    ext_association.append(chrome::kExtensionFileExtension);
    InstallUtil::DeleteRegistryKey(key, ext_association);
  }
}

InstallStatus UninstallChrome(const FilePath& setup_path,
                              const Product& product,
                              bool remove_all,
                              bool force_uninstall,
                              const CommandLine& cmd_line) {
  InstallStatus status = installer_util::UNINSTALL_CONFIRMED;
  std::wstring suffix;
  if (!ShellUtil::GetUserSpecificDefaultBrowserSuffix(product.distribution(),
                                                      &suffix))
    suffix = L"";

  BrowserDistribution* browser_dist = product.distribution();
  bool is_chrome = (browser_dist->GetType() ==
                    BrowserDistribution::CHROME_BROWSER);

  VLOG(1) << "UninstallChrome: " << browser_dist->GetApplicationName();

  if (force_uninstall) {
    // Since --force-uninstall command line option is used, we are going to
    // do silent uninstall. Try to close all running Chrome instances.
    // NOTE: We don't do this for Chrome Frame or CEEE.
    if (is_chrome)
      CloseAllChromeProcesses();
  } else if (is_chrome) {
    // no --force-uninstall so lets show some UI dialog boxes.
    status = IsChromeActiveOrUserCancelled(product);
    if (status != installer_util::UNINSTALL_CONFIRMED &&
        status != installer_util::UNINSTALL_DELETE_PROFILE)
      return status;

    // Check if we need admin rights to cleanup HKLM. If we do, try to launch
    // another uninstaller (silent) in elevated mode to do HKLM cleanup.
    // And continue uninstalling in the current process also to do HKCU cleanup.
    if (remove_all &&
        (!suffix.empty() || CurrentUserHasDefaultBrowser(product)) &&
        !::IsUserAnAdmin() &&
        base::win::GetVersion() >= base::win::VERSION_VISTA &&
        !cmd_line.HasSwitch(installer_util::switches::kRunAsAdmin)) {
      CommandLine new_cmd(CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer_util::switches::kRunAsAdmin);
      // Append --remove-chrome-registration to remove registry keys only.
      new_cmd.AppendSwitch(installer_util::switches::kRemoveChromeRegistration);
      if (!suffix.empty()) {
        new_cmd.AppendSwitchNative(
            installer_util::switches::kRegisterChromeBrowserSuffix, suffix);
      }
      DWORD exit_code = installer_util::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
    }
  }

  // Get the version of installed Chrome (if any)
  scoped_ptr<installer::Version>
      installed_version(InstallUtil::GetChromeVersion(browser_dist,
                                                      product.system_level()));

  // Chrome is not in use so lets uninstall Chrome by deleting various files
  // and registry entries. Here we will just make best effort and keep going
  // in case of errors.

  // First delete shortcuts from Start->Programs, Desktop & Quick Launch.
  DeleteChromeShortcuts(product);

  // Delete the registry keys (Uninstall key and Version key).
  HKEY reg_root = product.system_level() ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER;
  RegKey key(reg_root, L"", KEY_ALL_ACCESS);

  // Note that we must retrieve the distribution-specific data before deleting
  // product.GetVersionKey().
  std::wstring distribution_data(browser_dist->GetDistributionData(reg_root));

  // Remove Control Panel uninstall link and Omaha product key.
  InstallUtil::DeleteRegistryKey(key, browser_dist->GetUninstallRegPath());
  InstallUtil::DeleteRegistryKey(key, browser_dist->GetVersionKey());

  // Also try to delete the MSI value in the ClientState key (it might not be
  // there). This is due to a Google Update behaviour where an uninstall and a
  // rapid reinstall might result in stale values from the old ClientState key
  // being picked up on reinstall.
  product.SetMsiMarker(false);

  // Remove all Chrome registration keys.
  InstallStatus ret = installer_util::UNKNOWN_STATUS;
  DeleteChromeRegistrationKeys(product.distribution(), reg_root, suffix, ret);

  // For user level install also we end up creating some keys in HKLM if user
  // sets Chrome as default browser. So delete those as well (needs admin).
  if (remove_all && !product.system_level() &&
      (!suffix.empty() || CurrentUserHasDefaultBrowser(product))) {
    DeleteChromeRegistrationKeys(product.distribution(), HKEY_LOCAL_MACHINE,
                                 suffix, ret);
  }

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    if (!InstallUtil::IsChromeSxSProcess() && is_chrome) {
      // Delete media player registry key that exists only in HKLM.
      // We don't delete this key in SxS uninstall or Chrome Frame uninstall
      // as we never set the key for those products.
      RegKey hklm_key(HKEY_LOCAL_MACHINE, L"", KEY_ALL_ACCESS);
      std::wstring reg_path(installer::kMediaPlayerRegPath);
      file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
      InstallUtil::DeleteRegistryKey(hklm_key, reg_path);
      hklm_key.Close();
    }

    // Unregister any dll servers that we may have registered for Chrome Frame
    // and CEEE builds only.
    // TODO(tommi): We should only do this when the folder itself is
    // being removed and we know that the DLLs were previously registered.
    // Simplest would be to always register them.
    if (installed_version.get() && !is_chrome) {
      RegisterComDllList(product.package().path().Append(
                             installed_version->GetString()),
                         product.system_level(), false, false);
    }
  }

  // Close any Chrome Frame helper processes that may be running.
  if (InstallUtil::IsChromeFrameProcess()) {
    VLOG(1) << "Closing the Chrome Frame helper process";
    CloseChromeFrameHelperProcess();
  }

  if (!installed_version.get())
    return installer_util::UNINSTALL_SUCCESSFUL;

  // Finally delete all the files from Chrome folder after moving setup.exe
  // and the user's Local State to a temp location.
  bool delete_profile = ShouldDeleteProfile(cmd_line, status, product);
  ret = installer_util::UNINSTALL_SUCCESSFUL;

  // In order to be able to remove the folder in which we're running, we
  // need to move setup.exe out of the install folder.
  // TODO(tommi): What if the temp folder is on a different volume?
  MoveSetupOutOfInstallFolder(product.package(), setup_path,
                              *installed_version);

  FilePath backup_state_file(BackupLocalStateFile(
      GetLocalStateFolder(product)));

  // TODO(tommi): We should only do this when the last distribution is being
  // uninstalled.
  DeleteResult delete_result = DeleteFilesAndFolders(product.package(),
                                                     *installed_version);

  if (delete_profile)
    DeleteLocalState(product);

  if (delete_result == DELETE_FAILED) {
    ret = installer_util::UNINSTALL_FAILED;
  } else if (delete_result == DELETE_REQUIRES_REBOOT) {
    ret = installer_util::UNINSTALL_REQUIRES_REBOOT;
  }

  if (!force_uninstall) {
    VLOG(1) << "Uninstallation complete. Launching Uninstall survey.";
    browser_dist->DoPostUninstallOperations(*installed_version,
        backup_state_file, distribution_data);
  }

  // Try and delete the preserved local state once the post-install
  // operations are complete.
  if (!backup_state_file.empty())
    file_util::Delete(backup_state_file, false);

  return ret;
}

}  // namespace installer

