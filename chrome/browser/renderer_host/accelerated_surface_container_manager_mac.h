// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MANAGER_MAC_H_
#define CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MANAGER_MAC_H_
#pragma once

#include <OpenGL/OpenGL.h>
#include <map>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "gfx/native_widget_types.h"

namespace webkit_glue {
struct WebPluginGeometry;
}

class AcceleratedSurfaceContainerMac;

// Helper class that manages the backing store and on-screen rendering
// of instances of the GPU plugin on the Mac.
class AcceleratedSurfaceContainerManagerMac {
 public:
  AcceleratedSurfaceContainerManagerMac();

  // Allocates a new "fake" PluginWindowHandle, which is used as the
  // key for the other operations.
  gfx::PluginWindowHandle AllocateFakePluginWindowHandle(bool opaque,
                                                         bool root);

  // Destroys a fake PluginWindowHandle and associated storage.
  void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle id);

  // Indicates whether the given PluginWindowHandle is "root", which
  // means that we are using accelerated compositing and that this one
  // contains the compositor's output.
  bool IsRootContainer(gfx::PluginWindowHandle id);

  // Sets the size and backing store of the plugin instance.  There are two
  // versions: the IOSurface version is used on systems where the IOSurface
  // API is supported (Mac OS X 10.6 and later); the TransportDIB is used on
  // Mac OS X 10.5 and earlier.
  void SetSizeAndIOSurface(gfx::PluginWindowHandle id,
                           int32 width,
                           int32 height,
                           uint64 io_surface_identifier);
  void SetSizeAndTransportDIB(gfx::PluginWindowHandle id,
                              int32 width,
                              int32 height,
                              TransportDIB::Handle transport_dib);

  // Takes an update from WebKit about a plugin's position and size and moves
  // the plugin accordingly.
  void SetPluginContainerGeometry(const webkit_glue::WebPluginGeometry& move);

  // Draws the plugin container associated with the given id into the given
  // OpenGL context, which must already be current.
  void Draw(CGLContextObj context,
            gfx::PluginWindowHandle id,
            bool draw_root_container);

  // Causes the next Draw call on each container to trigger a texture upload.
  // Should be called any time the drawing context has changed.
  void ForceTextureReload();

 private:
  uint32 current_id_;

  // Maps a "fake" plugin window handle to the corresponding container.
  AcceleratedSurfaceContainerMac* MapIDToContainer(gfx::PluginWindowHandle id);

  // A map that associates plugin window handles with their containers.
  typedef std::map<gfx::PluginWindowHandle, AcceleratedSurfaceContainerMac*>
      PluginWindowToContainerMap;
  PluginWindowToContainerMap plugin_window_to_container_map_;

  // The "root" container, which is only used to draw the output of
  // the accelerated compositor if it is active. Currently,
  // accelerated plugins (Core Animation and Pepper 3D) are drawn on
  // top of the page's contents rather than transformed and composited
  // with the rest of the page. At some point we would like them to be
  // treated uniformly with other page elements; when this is done,
  // the separate treatment of the root container can go away because
  // there will only be one container active when the accelerated
  // compositor is active.
  AcceleratedSurfaceContainerMac* root_container_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerManagerMac);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MANAGER_MAC_H_

