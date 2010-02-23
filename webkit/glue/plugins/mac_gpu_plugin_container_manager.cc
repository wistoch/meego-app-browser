// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/mac_gpu_plugin_container_manager.h"

#include "base/logging.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/mac_gpu_plugin_container.h"

MacGPUPluginContainerManager::MacGPUPluginContainerManager()
    : current_id_(0) {
}

gfx::PluginWindowHandle
MacGPUPluginContainerManager::AllocateFakePluginWindowHandle() {
  MacGPUPluginContainer* container = new MacGPUPluginContainer();
  gfx::PluginWindowHandle res =
      static_cast<gfx::PluginWindowHandle>(++current_id_);
  plugin_window_to_container_map_.insert(std::make_pair(res, container));
  return res;
}

void MacGPUPluginContainerManager::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  MacGPUPluginContainer* container = MapIDToContainer(id);
  if (container)
    delete container;
  plugin_window_to_container_map_.erase(id);
}

void MacGPUPluginContainerManager::SetSizeAndIOSurface(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  MacGPUPluginContainer* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndIOSurface(width, height,
                                      io_surface_identifier, this);
}

void MacGPUPluginContainerManager::SetSizeAndTransportDIB(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  MacGPUPluginContainer* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndTransportDIB(width, height,
                                      transport_dib, this);
}

void MacGPUPluginContainerManager::MovePluginContainer(
    const webkit_glue::WebPluginGeometry& move) {
  MacGPUPluginContainer* container = MapIDToContainer(move.window);
  if (container)
    container->MoveTo(move);
}

void MacGPUPluginContainerManager::Draw(CGLContextObj context) {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glTexEnvi(target, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  for (PluginWindowToContainerMap::const_iterator i =
          plugin_window_to_container_map_.begin();
       i != plugin_window_to_container_map_.end(); ++i) {
    MacGPUPluginContainer* container = i->second;
    container->Draw(context);
  }

  // Unbind any texture from the texture target to ensure that the
  // next time through we will have to re-bind the texture and thereby
  // pick up modifications from the other process.
  glBindTexture(target, 0);

  glFlush();
}

void MacGPUPluginContainerManager::EnqueueTextureForDeletion(GLuint texture) {
  if (texture) {
    textures_pending_deletion_.push_back(texture);
  }
}

MacGPUPluginContainer* MacGPUPluginContainerManager::MapIDToContainer(
    gfx::PluginWindowHandle id) {
  PluginWindowToContainerMap::const_iterator i =
      plugin_window_to_container_map_.find(id);
  if (i != plugin_window_to_container_map_.end())
    return i->second;

  LOG(ERROR) << "Request for plugin container for unknown window id " << id;

  return NULL;
}

