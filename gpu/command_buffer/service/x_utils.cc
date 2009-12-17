// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class implements the XWindowWrapper class.

#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/common/logging.h"
#include "gpu/command_buffer/service/x_utils.h"

namespace gpu {

bool XWindowWrapper::Initialize() {
  XWindowAttributes attributes;
  XGetWindowAttributes(display_, window_, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo *visual_info_list = XGetVisualInfo(display_, VisualIDMask,
                                                 &visual_info_template,
                                                 &visual_info_count);
  DCHECK(visual_info_list);
  DCHECK_GT(visual_info_count, 0);
  context_ = 0;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = glXCreateContext(display_, visual_info_list + i, 0,
                                True);
    if (context_) break;
  }
  XFree(visual_info_list);
  if (!context_) {
    DLOG(ERROR) << "Couldn't create GL context.";
    return false;
  }
  return true;
}

bool XWindowWrapper::MakeCurrent() {
  if (glXMakeCurrent(display_, window_, context_) != True) {
    glXDestroyContext(display_, context_);
    context_ = 0;
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }
  return true;
}

void XWindowWrapper::Destroy() {
  Bool result = glXMakeCurrent(display_, 0, 0);
  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (context_) {
    glXDestroyContext(display_, context_);
    context_ = 0;
  }
}

void XWindowWrapper::SwapBuffers() {
  glXSwapBuffers(display_, window_);
}

}  // namespace gpu
