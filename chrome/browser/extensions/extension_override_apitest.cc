// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Overrides) {
  // The first pass response is the creation of a new tab.
  ASSERT_TRUE(RunExtensionTest("override1")) << message_;

  // The overridden new tab page also sends a pass response.
  WaitForPassFail();

  // There should be no additional pass/fail responses.
  EXPECT_EQ(results_.size(), 0U);

  // TODO(erikkay) load a second override and verify behavior, then unload
  // the first and verify behavior, etc.
}
