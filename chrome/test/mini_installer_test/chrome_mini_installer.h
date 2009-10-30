// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "base/string_util.h"

// This class has methods to install and uninstall Chrome mini installer.
class ChromeMiniInstaller {
 public:
  explicit ChromeMiniInstaller(const std::wstring& install_type);

  ~ChromeMiniInstaller() {}

  enum RepairChrome {
    REGISTRY,
    VERSION_FOLDER
  };

  // This method returns path to either program files
  // or documents and setting based on the install type.
  std::wstring GetChromeInstallDirectoryLocation();

  // Installs the latest full installer.
  void InstallFullInstaller(bool over_install);

  // Installs chrome.
  void Install();

  // This method will first install the full installer and
  // then over installs with diff installer.
  void OverInstallOnFullInstaller(const std::wstring& install_type);

  // Installs Google Chrome through meta installer.
  void InstallMetaInstaller();

  // Installs Chrome Mini Installer.
  void InstallMiniInstaller(bool over_install = false,
                            const std::wstring& path = L"");

  // This will test the standalone installer.
  void InstallStandaloneInstaller();

  // Repairs Chrome based on the passed argument.
  void Repair(RepairChrome repair_type);

  // Uninstalls Chrome.
  void UnInstall();

  // This method will perform a over install
  void OverInstall();

  void SetBuildUnderTest(const std::wstring& build);

 private:
  // This variable holds the install type.
  // Install type can be either system or user level.
  std::wstring install_type_;

  bool standalone_installer;

  // Name of the browser (Chrome or Chromium) and install type (sys or user)
  std::wstring installer_name_;

  // The full path to the various installers.
  std::wstring full_installer_, diff_installer_, prev_installer_;

  // Whether the path to the associated installer could be found.
  // This is because we do not want to assert that these paths exist
  // except in the tests that use them.
  bool has_full_installer_, has_diff_installer_, has_prev_installer_;

  // The version string of the current and previous builds.
  std::wstring curr_version_, prev_version_;

  // Will clean up the machine if Chrome install is messed up.
  void CleanChromeInstall();

  // Closes First Run UI dialog.
  void CloseFirstRunUIDialog(bool over_install);

  // Closes Chrome uninstall confirm dialog window.
  bool CloseUninstallWindow();

  // Closes Chrome browser.
  bool CloseChromeBrowser();

  // Checks for registry key.
  bool CheckRegistryKey(const std::wstring& key_path);

  // Checks for registry key on uninstall.
  bool CheckRegistryKeyOnUninstall(const std::wstring& key_path);

  // Deletes specified folder after getting the install path.
  void DeleteFolder(const wchar_t* folder_name);

  // Will delete user data profile.
  void DeleteUserDataFolder();

  // This will delete pv key from client registry.
  void DeletePvRegistryKey();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Returns Chrome pv registry key value
  bool GetChromeVersionFromRegistry(std::wstring *return_reg_key_value);

  // This method gets the shortcut path from start menu based on install type.
  std::wstring GetStartMenuShortcutPath();

  // Get path for uninstall.
  std::wstring GetUninstallPath();

  // Gets the path to launch Chrome.
  bool GetChromeLaunchPath(std::wstring* launch_path);

  // This method will get Chrome.exe path and launch it.
  void VerifyChromeLaunch(bool expected_status);

  // Launches the chrome installer and waits for it to end.
  void LaunchInstaller(const std::wstring& install_path,
                       const wchar_t* process_name);

  // Verifies if Chrome launches after install.
  void LaunchAndCloseChrome(bool over_install);

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(const std::wstring& reg_key_value_before_overinstall,
                         const std::wstring& reg_key_value_after_overinstall);

  // This method will verify if the installed build is correct.
  bool VerifyStandaloneInstall();

  DISALLOW_COPY_AND_ASSIGN(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_

