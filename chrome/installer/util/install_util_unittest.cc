// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product_unittest.h"

using base::win::RegKey;
using installer::InstallationState;
using installer::InstallerState;
using installer::MasterPreferences;

class InstallUtilTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

TEST_F(InstallUtilTest, InstallerResult) {
  const bool system_level = true;
  bool multi_install = false;
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring launch_cmd = L"hey diddle diddle";
  std::wstring value;

  // check results for a fresh install of single Chrome
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    CommandLine cmd_line = CommandLine::FromString(L"setup.exe --system-level");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    InstallUtil::WriteInstallerResult(system_level, state.state_key(),
        installer::FIRST_INSTALL_SUCCESS, 0, &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
  }
  TempRegKeyOverride::DeleteAllTempKeys();

  // check results for a fresh install of multi Chrome
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    CommandLine cmd_line = CommandLine::FromString(
        L"setup.exe --system-level --multi-install --chrome");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    InstallUtil::WriteInstallerResult(system_level, state.state_key(),
        installer::FIRST_INSTALL_SUCCESS, 0, &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
    key.Close();
  }
  TempRegKeyOverride::DeleteAllTempKeys();
}

TEST_F(InstallUtilTest, MakeUninstallCommand) {
  CommandLine command_line(CommandLine::NO_PROGRAM);

  std::pair<std::wstring, std::wstring> params[] = {
    std::make_pair(std::wstring(L""), std::wstring(L"")),
    std::make_pair(std::wstring(L""), std::wstring(L"--do-something --silly")),
    std::make_pair(std::wstring(L"spam.exe"), std::wstring(L"")),
    std::make_pair(std::wstring(L"spam.exe"),
                   std::wstring(L"--do-something --silly")),
  };
  for (int i = 0; i < arraysize(params); ++i) {
    std::pair<std::wstring, std::wstring>& param = params[i];
    InstallUtil::MakeUninstallCommand(param.first, param.second, &command_line);
    EXPECT_EQ(param.first, command_line.GetProgram().value());
    if (param.second.empty()) {
      EXPECT_EQ(0U, command_line.GetSwitchCount());
    } else {
      EXPECT_EQ(2U, command_line.GetSwitchCount());
      EXPECT_TRUE(command_line.HasSwitch("do-something"));
      EXPECT_TRUE(command_line.HasSwitch("silly"));
    }
  }
}
