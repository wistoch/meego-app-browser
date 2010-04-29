// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_CONTEXT_OSMESA_H_
#define APP_GFX_GL_GL_CONTEXT_OSMESA_H_

#include "base/scoped_ptr.h"
#include "gfx/size.h"
#include "app/gfx/gl/gl_context.h"

typedef struct osmesa_context *OSMesaContext;

namespace gfx {

// Encapsulates an OSMesa OpenGL context that uses software rendering.
class OSMesaGLContext : public GLContext {
 public:
  OSMesaGLContext();
  virtual ~OSMesaGLContext();

  // Initialize an OSMesa GL context with the default 1 x 1 initial size.
  bool Initialize(void* shared_handle);

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

  // Resize the back buffer, preserving the old content. Does nothing if the
  // size is unchanged.
  void Resize(const gfx::Size& new_size);

  const void* buffer() const {
    return buffer_.get();
  }

 private:
#if !defined(UNIT_TEST)
  gfx::Size size_;
  scoped_array<int32> buffer_;
  OSMesaContext context_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OSMesaGLContext);
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_OSMESA_H_
