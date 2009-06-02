// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/platform_thread.h"
#include "base/win_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome_mini_installer.h"

namespace {
class MiniInstallTest : public testing::Test {
   protected:
    void CleanTheSystem() {
      ChromeMiniInstaller userinstall(mini_installer_constants::kUserInstall,
          mini_installer_constants::kDevChannelBuild);
      userinstall.UnInstall();
      if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
        ChromeMiniInstaller systeminstall(
            mini_installer_constants::kSystemInstall,
            mini_installer_constants::kDevChannelBuild);
        systeminstall.UnInstall();
      }
    }
    virtual void SetUp() {
      CleanTheSystem();
    }

    virtual void TearDown() {
      PlatformThread::Sleep(2000);
      CleanTheSystem();
    }
  };
};

TEST_F(MiniInstallTest, FullInstallerTestOnDev) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  installer.InstallFullInstaller(false);
}

TEST_F(MiniInstallTest, FullInstallerTestOnStable) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kStableChannelBuild);
  installer.InstallFullInstaller(false);
}

TEST_F(MiniInstallTest, FullInstallerSystemLevelTestOnDev) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall,
                                  mini_installer_constants::kDevChannelBuild);
    installer.InstallFullInstaller(false);
  }
}

// --system-level argument dosen't work with 1.0 builds.
TEST_F(MiniInstallTest, DISABLED_FullInstallerSystemLevelTestOnStable) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall,
        mini_installer_constants::kStableChannelBuild);
    installer.InstallFullInstaller(false);
  }
}

TEST_F(MiniInstallTest, FullInstallerOverPreviousFullInstallerTestOnDev) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall);
}

TEST_F(MiniInstallTest, FullInstallerOverPreviousFullInstallerTestOnStable) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kStableChannelBuild);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall);
}

TEST_F(MiniInstallTest, DiffInstallerOverPreviousFullInstallerTestOnStable) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kStableChannelBuild);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall);
}

TEST_F(MiniInstallTest, DiffInstallerOverPreviousFullInstallerTestOnDev) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall);
}

TEST_F(MiniInstallTest, StandaloneInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kStableChannelBuild);
  installer.InstallStandaloneInstaller();
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  installer.OverInstall();
}

TEST_F(MiniInstallTest, DISABLED_MiniInstallerSystemInstallTest) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall,
                                  mini_installer_constants::kDevChannelBuild);
    installer.Install();
  }
}

TEST_F(MiniInstallTest, DISABLED_MiniInstallerUserInstallTest) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                  mini_installer_constants::kDevChannelBuild);
    installer.Install();
  }
}

TEST_F(MiniInstallTest, StableChromeInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kStableChannelBuild);
  installer.OverInstall();
}

TEST_F(MiniInstallTest, DevChromeInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  installer.OverInstall();
}

TEST(InstallUtilTests, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}
