/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "tests/common/win/testing_common.h"
#include "core/cross/client.h"
#include "core/cross/client_info.h"
#include "core/cross/renderer.h"
#include "core/cross/bitmap.h"
#include "core/cross/features.h"
#include "core/cross/texture.h"
#include "core/cross/renderer_platform.h"

// Defined in testing_common.cc, for each platform.
extern o3d::DisplayWindow* g_display_window;

namespace o3d {

class RendererTest : public testing::Test {
 public:
  ServiceLocator* service_locator() {
    return service_locator_;
  }

 protected:
  virtual void SetUp() {
    service_locator_ = new ServiceLocator;
    features_ = new Features(service_locator_);
    client_info_manager_ = new ClientInfoManager(service_locator_);
  }

  virtual void TearDown() {
    delete client_info_manager_;
    delete features_;
    delete service_locator_;
  }

  ServiceLocator* service_locator_;
  Features* features_;
  ClientInfoManager* client_info_manager_;
};

// This tests that a default Renderer can be created.
TEST_F(RendererTest, CreateDefaultRenderer) {
  scoped_ptr<Renderer> renderer(
      Renderer::CreateDefaultRenderer(service_locator()));
  EXPECT_TRUE(renderer != NULL);
}



TEST_F(RendererTest, InitAndDestroyRenderer) {
// TODO(apatrick): This test will not work as is with command buffers because
//     it attempts to create a Renderer using the same ring buffer as the
//     Renderer created in main.
  scoped_ptr<Renderer> renderer(
      Renderer::CreateDefaultRenderer(service_locator()));
  EXPECT_TRUE(renderer->Init(*g_display_window, false));
#if defined(RENDERER_D3D9)
  // test that the d3d_device was correctly created
  RendererD3D9* d3d_renderer = down_cast<RendererD3D9*>(renderer.get());
  EXPECT_TRUE(d3d_renderer->d3d_device() != NULL);
#elif defined(RENDERER_GL)
  // test that the Cg Context was correctly created
  RendererGL* gl_renderer = down_cast<RendererGL*>(renderer.get());
  EXPECT_TRUE(gl_renderer->cg_context() != NULL);
#elif defined(RENDERER_GLES2)
  RendererGLES2* gles2_renderer = down_cast<RendererGLES2*>(renderer.get());
#endif
  // destroy the renderer
  renderer->Destroy();

#if defined(RENDERER_D3D9)
  // check that the renderer no longer had the D3D device.
  EXPECT_FALSE(d3d_renderer->d3d_device() != NULL);
#elif defined(RENDERER_GL)
  // check that the renderer no longer has a Cg Context.
  EXPECT_FALSE(gl_renderer->cg_context() != NULL);
#elif defined(RENDERER_GLES2)
#if defined(GLES2_BACKEND_DESKTOP_GL)
  EXPECT_FALSE(gles2_renderer->glx_context() != NULL);
#elif defined(GLES2_BACKEND_NATIVE_GLES2)
  EXPECT_FALSE(gles2_renderer->egl_context() != NULL);
#endif
#endif
}

// Offscreen is only supported on D3D currently
#if defined(RENDERER_D3D9)
// Tests that creating an off-screen renderer works correctly.
TEST_F(RendererTest, OffScreen) {
  scoped_ptr<Renderer> renderer(
      Renderer::CreateDefaultRenderer(service_locator()));
  EXPECT_TRUE(renderer->Init(*g_display_window, true));

  RendererD3D9 *d3d_renderer = down_cast<RendererD3D9*>(renderer.get());
  EXPECT_TRUE(d3d_renderer->d3d_device() != NULL);

  renderer->Destroy();

  EXPECT_FALSE(d3d_renderer->d3d_device() != NULL);
}
#endif

// Tests SetViewport
TEST_F(RendererTest, SetViewport) {
  ErrorStatus error_status(g_service_locator);

  // Test that we can call it.
  EXPECT_TRUE(error_status.GetLastError().empty());
  g_renderer->SetViewport(Float4(0.0f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_TRUE(error_status.GetLastError().empty());

  // Test zero width
  g_renderer->SetViewport(Float4(0.0f, 0.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f));
  EXPECT_TRUE(error_status.GetLastError().empty());

  // Test that it fails with invalid values
  error_status.ClearLastError();
  // width off right
  g_renderer->SetViewport(Float4(0.5f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // height off bottom
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(0.0f, 0.5f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // left off right
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(2.0f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // top off bottom
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(0.0f, 2.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // negative width
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(0.0f, 0.0f, -1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // negative height
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(0.0f, 0.0f, 1.0f, -1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // left off left
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(-0.1f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());

  // top off top
  error_status.ClearLastError();
  g_renderer->SetViewport(Float4(0.0f, -0.1f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
  EXPECT_FALSE(error_status.GetLastError().empty());
}

}  // namespace o3d

