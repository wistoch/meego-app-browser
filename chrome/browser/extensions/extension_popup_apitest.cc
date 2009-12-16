// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Flaky, http://crbug.com/27271.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_Popup) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExtensionToolstrips);

  ASSERT_TRUE(RunExtensionTest("popup")) << message_;
}
