// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/gl/gl_switches.h"

namespace gfx {

unsigned int GLContext::GetBackingFrameBufferObject() {
  return 0;
}

std::string GLContext::GetExtensions() {
  DCHECK(IsCurrent());
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  return std::string(ext ? ext : "");
}

bool GLContext::HasExtension(const char* name) {
  std::string extensions = GetExtensions();
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

bool GLContext::InitializeCommon() {
  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    return false;
  }

  if (!IsOffscreen()) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
      SetSwapInterval(0);
    else
      SetSwapInterval(1);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "glClear failed.";
    return false;
  }

  return true;
}

}  // namespace gfx
