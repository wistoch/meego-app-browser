// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

class DeviceOrientationEnableSwitchTest : public InProcessBrowserTest {
 public:
  GURL testUrl(const FilePath::CharType* filename) {
    const FilePath kTestDir(FILE_PATH_LITERAL("device_orientation"));
    return ui_test_utils::GetTestUrl(kTestDir, FilePath(filename));
  }
};

IN_PROC_BROWSER_TEST_F(DeviceOrientationEnableSwitchTest, UnavailabilityTest) {
  // Test that device orientation is not available to a web page if
  // the runtime switch is disabled.

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool has_switch = command_line.HasSwitch(switches::kEnableDeviceOrientation);
  ASSERT_FALSE(has_switch) << "This test does not make sense if "
                           << "--enable-device-orientation is set.";

  GURL test_url = testUrl(FILE_PATH_LITERAL("enable_switch_test.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);
  std::string status = browser()->GetSelectedTabContents()->GetURL().ref();
  EXPECT_EQ("pass", status) << "Page detected device orientation properties.";
}
