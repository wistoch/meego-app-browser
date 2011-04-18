// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the NativeViewGLContext and PbufferGLContext classes.

#include "ui/gfx/gl/gl_context.h"

#include <GL/osmesa.h>

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context_egl.h"
#include "ui/gfx/gl/gl_context_osmesa.h"
#include "ui/gfx/gl/gl_context_stub.h"
#include "ui/gfx/gl/gl_context_wgl.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "ui/gfx/gl/gl_surface_wgl.h"

namespace gfx {

// This OSMesa GL surface can use GDI to swap the contents of the buffer to a
// view.
class NativeViewGLSurfaceOSMesa : public GLSurfaceOSMesa {
 public:
  explicit NativeViewGLSurfaceOSMesa(gfx::PluginWindowHandle window);
  virtual ~NativeViewGLSurfaceOSMesa();

  // Initializes the GL context.
  bool Initialize();

  // Implement subset of GLSurface.
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();

 private:
  void UpdateSize();

  gfx::PluginWindowHandle window_;
  HDC device_context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceOSMesa);
};

// Helper routine that does one-off initialization like determining the
// pixel format and initializing the GL bindings.
bool GLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const GLImplementation kAllowedGLImplementations[] = {
    kGLImplementationEGLGLES2,
    kGLImplementationDesktopGL,
    kGLImplementationOSMesaGL
  };

  if (!InitializeRequestedGLBindings(
           kAllowedGLImplementations,
           kAllowedGLImplementations + arraysize(kAllowedGLImplementations),
           kGLImplementationEGLGLES2)) {
    LOG(ERROR) << "InitializeRequestedGLBindings failed.";
    return false;
  }

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceWGL::InitializeOneOff())
        return false;
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
  }

  initialized = true;
  return true;
}

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::PluginWindowHandle window)
  : window_(window),
    device_context_(NULL) {
  DCHECK(window);
}

NativeViewGLSurfaceOSMesa::~NativeViewGLSurfaceOSMesa() {
  Destroy();
}

bool NativeViewGLSurfaceOSMesa::Initialize() {
  device_context_ = GetDC(window_);
  UpdateSize();
  return true;
}

void NativeViewGLSurfaceOSMesa::Destroy() {
  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  window_ = NULL;
  device_context_ = NULL;

  GLSurfaceOSMesa::Destroy();
}

bool NativeViewGLSurfaceOSMesa::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceOSMesa::SwapBuffers() {
  DCHECK(device_context_);

  // Update the size before blitting so that the blit size is exactly the same
  // as the window.
  UpdateSize();

  gfx::Size size = GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                0, 0, size.width(), size.height(),
                0, 0, size.width(), size.height(),
                GetHandle(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return true;
}

void NativeViewGLSurfaceOSMesa::UpdateSize() {
  // Change back buffer size to that of window. If window handle is invalid, do
  // not change the back buffer size.
  RECT rect;
  if (!GetClientRect(window_, &rect))
    return;

  gfx::Size window_size = gfx::Size(
    std::max(1, static_cast<int>(rect.right - rect.left)),
    std::max(1, static_cast<int>(rect.bottom - rect.top)));
  Resize(window_size);
}

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle window,
                                          bool multisampled) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_ptr<NativeViewGLSurfaceOSMesa> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(surface.release()));
      if (!context->Initialize(OSMESA_RGBA, NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<NativeViewGLSurfaceEGL> surface(new NativeViewGLSurfaceEGL(
          window));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextEGL> context(
          new GLContextEGL(surface.release()));
      if (!context->Initialize(NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationDesktopGL: {
      scoped_ptr<NativeViewGLSurfaceWGL> surface(new NativeViewGLSurfaceWGL(
          window));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextWGL> context(
          new GLContextWGL(surface.release()));
      if (!context->Initialize(NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

GLContext* GLContext::CreateOffscreenGLContext(GLContext* shared_context) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_ptr<GLSurfaceOSMesa> surface(new GLSurfaceOSMesa());
      surface->Resize(gfx::Size(1, 1));

      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(surface.release()));
      if (!context->Initialize(OSMESA_RGBA, shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<PbufferGLSurfaceEGL> surface(new PbufferGLSurfaceEGL(
          gfx::Size(1, 1)));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextEGL> context(new GLContextEGL(surface.release()));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationDesktopGL: {
      scoped_ptr<PbufferGLSurfaceWGL> surface(new PbufferGLSurfaceWGL(
          gfx::Size(1, 1)));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextWGL> context(new GLContextWGL(surface.release()));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
