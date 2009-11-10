// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// TODO(jcampan): http://crbug.com/27216 disabled because failing.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}
