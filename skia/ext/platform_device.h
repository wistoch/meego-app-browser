// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's device class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(WIN32)
#include "PlatformDeviceWin.h"
#elif defined(__APPLE__)
#include "PlatformDeviceMac.h"
#elif defined(__linux__)
#include "PlatformDeviceLinux.h"
#endif

namespace gfx {

#if defined(WIN32)
typedef PlatformDeviceWin PlatformDevice;
#elif defined(__APPLE__)
typedef PlatformDeviceMac PlatformDevice;
#elif defined(__linux__)
typedef PlatformDeviceLinux PlatformDevice;
#endif

}  // namespace gfx
