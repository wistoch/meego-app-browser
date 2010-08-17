// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_TEST_SUITE_H_
#define GFX_TEST_SUITE_H_
#pragma once

#include "build/build_config.h"

#include <string>

#include "gfx/gfx_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/scoped_nsautorelease_pool.h"
#include "base/test/test_suite.h"

class GfxTestSuite : public base::TestSuite {
 public:
  GfxTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

 protected:

  virtual void Initialize() {
    base::ScopedNSAutoreleasePool autorelease_pool;

    TestSuite::Initialize();

    gfx::RegisterPathProvider();

#if defined(OS_MACOSX)
    // Look in the framework bundle for resources.
    // TODO(port): make a resource bundle for non-app exes.  What's done here
    // isn't really right because this code needs to depend on chrome_dll
    // being built.  This is inappropriate in app.
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
#if defined(GOOGLE_CHROME_BUILD)
    path = path.AppendASCII("Google Chrome Framework.framework");
#elif defined(CHROMIUM_BUILD)
    path = path.AppendASCII("Chromium Framework.framework");
#else
#error Unknown branding
#endif
    mac_util::SetOverrideAppBundlePath(path);
#endif  // OS_MACOSX
  }

  virtual void Shutdown() {
#if defined(OS_MACOSX)
    mac_util::SetOverrideAppBundle(NULL);
#endif
    TestSuite::Shutdown();
  }
};

#endif  // GFX_TEST_SUITE_H_
