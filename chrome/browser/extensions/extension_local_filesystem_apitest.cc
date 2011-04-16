// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_CHROMEOS)

// TODO(zelidrag): Remove disable prefix on this test once API changes land.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_LocalFileSystem) {
  ASSERT_TRUE(RunComponentExtensionTest("local_filesystem")) << message_;
}

#endif
