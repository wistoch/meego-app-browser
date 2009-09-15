// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// TODO(asargent): http://crbug.com/20828
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Tabs) {
  ASSERT_TRUE(RunExtensionTest("tabs")) << message_;
}
