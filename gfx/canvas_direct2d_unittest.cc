// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "base/command_line.h"
#include "gfx/canvas_direct2d.h"
#include "gfx/canvas_skia.h"
#include "gfx/native_theme_win.h"
#include "gfx/window_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestWindow : public gfx::WindowImpl {
 public:
  static const int kWindowSize = 500;
  static const int kWindowPosition = 10;

  static const wchar_t* kVisibleModeFlag;

  TestWindow() {
    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      Sleep(1000);

    // Create the window.
    Init(NULL,
         gfx::Rect(kWindowPosition, kWindowPosition, kWindowSize, kWindowSize));

    // Initialize the RenderTarget for the window.
    rt_ = MakeHWNDRenderTarget();

    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      ShowWindow(hwnd(), SW_SHOW);
  }
  virtual ~TestWindow() {
    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      Sleep(1000);
    DestroyWindow(hwnd());
  }

  ID2D1RenderTarget* rt() const { return rt_.get(); }

  BEGIN_MSG_MAP_EX(TestWindow)
  END_MSG_MAP()

 private:
  ID2D1RenderTarget* MakeHWNDRenderTarget() {
    D2D1_RENDER_TARGET_PROPERTIES rt_properties =
        D2D1::RenderTargetProperties();
    rt_properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    ID2D1HwndRenderTarget* rt = NULL;
    gfx::CanvasDirect2D::GetD2D1Factory()->CreateHwndRenderTarget(
        rt_properties,
        D2D1::HwndRenderTargetProperties(hwnd(), D2D1::SizeU(500, 500)),
        &rt);
    return rt;
  }

  ScopedComPtr<ID2D1RenderTarget> rt_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

// static
const wchar_t* TestWindow::kVisibleModeFlag = L"d2d-canvas-visible";

}  // namespace

TEST(CanvasDirect2D, CreateCanvas) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());
}

TEST(CanvasDirect2D, SaveRestoreNesting) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  // Simple.
  canvas.Save();
  canvas.Restore();

  // Nested.
  canvas.Save();
  canvas.Save();
  canvas.Restore();
  canvas.Restore();

  // Simple alpha.
  canvas.SaveLayerAlpha(127);
  canvas.Restore();

  // Alpha with sub-rect.
  canvas.SaveLayerAlpha(127, gfx::Rect(20, 20, 100, 100));
  canvas.Restore();

  // Nested alpha.
  canvas.Save();
  canvas.SaveLayerAlpha(127);
  canvas.Save();
  canvas.Restore();
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, SaveLayerAlpha) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.FillRectInt(SK_ColorBLUE, 20, 20, 100, 100);
  canvas.SaveLayerAlpha(127);
  canvas.FillRectInt(SK_ColorRED, 60, 60, 100, 100);
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, SaveLayerAlphaWithBounds) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.FillRectInt(SK_ColorBLUE, 20, 20, 100, 100);
  canvas.SaveLayerAlpha(127, gfx::Rect(60, 60, 50, 50));
  canvas.FillRectInt(SK_ColorRED, 60, 60, 100, 100);
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, FillRect) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.FillRectInt(SK_ColorRED, 20, 20, 100, 100);
}

TEST(CanvasDirect2D, PlatformPainting) {
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  gfx::NativeDrawingContext dc = canvas.BeginPlatformPaint();

  // Use the system theme engine to draw a native button. This only works on a
  // GDI device context.
  RECT r = { 20, 20, 220, 80 };
  gfx::NativeTheme::instance()->PaintButton(
      dc, BP_PUSHBUTTON, PBS_NORMAL, DFCS_BUTTONPUSH, &r);

  canvas.EndPlatformPaint();
}

