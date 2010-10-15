// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "skia/ext/bitmap_platform_device.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

PlatformCanvas::PlatformCanvas()
    : SkCanvas(SkNEW(SkBitmapPlatformDeviceFactory)) {
}

PlatformCanvas::PlatformCanvas(SkDeviceFactory* factory) : SkCanvas(factory) {
}

SkDevice* PlatformCanvas::setBitmapDevice(const SkBitmap&) {
  SkASSERT(false);  // Should not be called.
  return NULL;
}

PlatformDevice& PlatformCanvas::getTopPlatformDevice() const {
  // All of our devices should be our special PlatformDevice.
  SkCanvas::LayerIter iter(const_cast<PlatformCanvas*>(this), false);
  return *static_cast<PlatformDevice*>(iter.device());
}

// static
size_t PlatformCanvas::StrideForWidth(unsigned width) {
  return 4 * width;
}

}  // namespace skia
