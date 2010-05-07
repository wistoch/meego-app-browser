// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_

#include "third_party/ppapi/c/pp_stdint.h"

namespace skia {
class PlatformCanvas;
}

namespace pepper {

// Virtual interface that the browser implements to implement features for
// Pepper plugins.
class PluginDelegate {
 public:
  // Represents an image. This is to allow the browser layer to supply a correct
  // image representation. In Chrome, this will be a TransportDIB.
  class PlatformImage2D {
   public:
    virtual ~PlatformImage2D() {}

    // Caller will own the returned pointer, returns NULL on failure.
    virtual skia::PlatformCanvas* Map() = 0;

    // Returns the platform-specific shared memory handle of the data backing
    // this image. This is used by NativeClient to send the image to the
    // out-of-process plugin. Returns 0 on failure.
    virtual intptr_t GetSharedMemoryHandle() const = 0;
  };

  // The caller will own the pointer returned from this.
  virtual PlatformImage2D* CreateImage2D(int width, int height) = 0;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_
