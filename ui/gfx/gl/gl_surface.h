// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_H_
#define UI_GFX_GL_GL_SURFACE_H_
#pragma once

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
class GLSurface {
 public:
  GLSurface() {}
  virtual ~GLSurface() {}

  // Destroys the surface.
  virtual void Destroy() = 0;

  // Returns true if this surface is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual bool SwapBuffers() = 0;

  // Get the size of the surface.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific surface "handle".
  virtual void* GetHandle() = 0;

  // Returns the internal frame buffer object name if the surface is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFrameBufferObject();

#if !defined(OS_MACOSX)
  // Create a surface corresponding to a view.
  static GLSurface* CreateViewGLContext(gfx::PluginWindowHandle window);
#endif

  // Create a surface used for offscreen rendering.
  static GLSurface* CreateOffscreenGLContext(const gfx::Size& size);

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurface);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_H_
