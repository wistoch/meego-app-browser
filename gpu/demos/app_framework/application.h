// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for gles2 applications using command buffer.

#ifndef GPU_DEMOS_APP_FRAMEWORK_APPLICATION_H_
#define GPU_DEMOS_APP_FRAMEWORK_APPLICATION_H_

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/time.h"

#include "gpu/demos/app_framework/platform.h"

namespace gpu_demos {

// Acts as a base class for GLES2 applications using command buffer.
// The derived calls needs to call InitRenderContext() to create a render
// surface and initialize a rendering context. Currently it only creates
// an on-screen window. It will be extended to support pepper/nacl plugin
// when pepper 3D api is in place.
class Application {
 public:
  Application();
  virtual ~Application();

  // Enters the event processing loop.
  void MainLoop();
  void OnPaint();

 protected:
  // Returns the width of rendering surface.
  inline int width() const { return width_; }
  // Returns the height of rendering surface.
  inline int height() const { return height_; }

  bool InitRenderContext();

  // The framework calls this function for the derived classes to do custom
  // rendering. There is no default implementation. It must be defined by the
  // derived classes. The elapsed_sec param represents the time elapsed
  // (in seconds) after Draw was called the last time. It can be used to
  // make the application frame-rate independent. It is 0.0f for the
  // first draw call.
  virtual void Draw(float elapsed_sec) = 0;

 private:
  // Creates a native on-screen window.
  NativeWindowHandle CreateNativeWindow();

  int width_;
  int height_;
  NativeWindowHandle window_handle_;

  // Time at which draw was called last.
  base::Time last_draw_time_;

  // The following two variables are just needed to satisfy
  // the assumption that we are running inside a browser.
  base::AtExitManager at_exit_manager_;
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(Application);
};

}  // namespace gpu_demos
#endif  // GPU_DEMOS_APP_FRAMEWORK_APPLICATION_H_
