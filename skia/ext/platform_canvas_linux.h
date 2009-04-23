// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_LINUX_H_
#define SKIA_EXT_PLATFORM_CANVAS_LINUX_H_

#include <unistd.h>

#include "skia/ext/platform_device_linux.h"

#include <cairo/cairo.h>
#include <gdk/gdk.h>

namespace skia {

// This class is a specialization of the regular SkCanvas that is designed to
// work with a gfx::PlatformDevice to manage platform-specific drawing. It
// allows using both Skia operations and platform-specific operations.
class PlatformCanvasLinux : public SkCanvas {
 public:
  // Set is_opaque if you are going to erase the bitmap and not use
  // tranparency: this will enable some optimizations.  The shared_section
  // parameter is passed to gfx::PlatformDevice::create.  See it for details.
  //
  // If you use the version with no arguments, you MUST call initialize()
  PlatformCanvasLinux();
  PlatformCanvasLinux(int width, int height, bool is_opaque);
  // Construct a canvas from the given memory region. The memory is not cleared
  // first. @data must be, at least, @height * StrideForWidth(@width) bytes.
  PlatformCanvasLinux(int width, int height, bool is_opaque, uint8_t* data);
  virtual ~PlatformCanvasLinux();

  // For two-part init, call if you use the no-argument constructor above
  bool initialize(int width, int height, bool is_opaque);
  bool initialize(int width, int height, bool is_opaque, uint8_t* data);

  // These calls should surround calls to platform-specific drawing routines.
  // The cairo_surface_t* returned by beginPlatformPaint represents the
  // memory that can be used to draw into.
  // endPlatformPaint is a no-op; it is used for symmetry with Windows.
  cairo_surface_t* beginPlatformPaint();
  void endPlatformPaint() {}

  // Returns the platform device pointer of the topmost rect with a non-empty
  // clip. Both the windows and mac versions have an equivalent of this method;
  // a Linux version is added for compatibility.
  PlatformDeviceLinux& getTopPlatformDevice() const;

  // Return the stride (length of a line in bytes) for the given width. Because
  // we use 32-bits per pixel, this will be roughly 4*width. However, for
  // alignment reasons we may wish to increase that.
  static size_t StrideForWidth(unsigned width);

 protected:
  // Creates a device store for use by the canvas. We override this so that
  // the device is always our own so we know that we can use GDI operations
  // on it. Simply calls into createPlatformDevice().
  virtual SkDevice* createDevice(SkBitmap::Config, int width, int height,
                                 bool is_opaque, bool isForLayer);

  // Creates a device store for use by the canvas. By default, it creates a
  // BitmapPlatformDevice object. Can be overridden to change the object type.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque);

  // Disallow copy and assign.
  PlatformCanvasLinux(const PlatformCanvasLinux&);
  PlatformCanvasLinux& operator=(const PlatformCanvasLinux&);
};

// A class designed to translate skia painting into a region in a
// GdkWindow. This class has been adapted from the class with the same name in
// platform_canvas_win.h. On construction, it will set up a context for
// painting into, and on destruction, it will commit it to the GdkWindow.
template <class T>
class CanvasPaintT : public T {
 public:
  explicit CanvasPaintT(GdkEventExpose* event)
      : surface_(NULL),
        window_(event->window),
        rectangle_(event->area) {
    init(true);
  }

  CanvasPaintT(GdkEventExpose* event, bool opaque)
      : surface_(NULL),
        window_(event->window),
        rectangle_(event->area) {
    init(opaque);
  }

  virtual ~CanvasPaintT() {
    if (!isEmpty()) {
      T::restoreToCount(1);

      // Blit the dirty rect to the window.
      cairo_t* cr = gdk_cairo_create(window_);
      cairo_set_source_surface(cr, surface_, 0.0, 0.0);
      cairo_rectangle(cr, rectangle_.x, rectangle_.y,
                      rectangle_.width, rectangle_.height);
      cairo_fill(cr);
      cairo_destroy(cr);
    }
  }

  // Returns true if the invalid region is empty. The caller should call this
  // function to determine if anything needs painting.
  bool isEmpty() const {
    return rectangle_.width == 0 || rectangle_.height == 0;
  }

  const GdkRectangle& rectangle() const {
    return rectangle_;
  }

 private:
  void init(bool opaque) {
    // In order to be most optimal, we could allocate just the damaged rect and
    // set a translation so it's at the origin.  However, since that would be
    // ignored when we draw on the cairo surface, this currently won't work.
    // Allocate the minimal bitmap from the origin to damage rect.
    if (!T::initialize(rectangle_.x + rectangle_.width,
                       rectangle_.y + rectangle_.height, opaque, NULL)) {
      // Cause a deliberate crash;
      *(char*) 0 = 0;
    }

    surface_ = T::getTopPlatformDevice().beginPlatformPaint();
  }

  cairo_surface_t* surface_;
  GdkWindow* window_;
  GdkRectangle rectangle_;

  // Disallow copy and assign.
  CanvasPaintT(const CanvasPaintT&);
  CanvasPaintT& operator=(const CanvasPaintT&);
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_LINUX_H_
