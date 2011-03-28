// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragged_tab_view.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/ui/views/tabs/native_view_photobooth.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/canvas_skia.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

static const int kTransparentAlpha = 200;
static const int kOpaqueAlpha = 255;
static const int kDragFrameBorderSize = 2;
static const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;
static const float kScalingFactor = 0.5;
static const SkColor kDraggedTabBorderColor = SkColorSetRGB(103, 129, 162);

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, public:

DraggedTabView::DraggedTabView(const std::vector<views::View*>& renderers,
                               const gfx::Point& mouse_tab_offset,
                               const gfx::Size& contents_size,
                               const gfx::Size& min_size)
    : renderers_(renderers),
      show_contents_on_drag_(true),
      mouse_tab_offset_(mouse_tab_offset),
      tab_size_(min_size),
      photobooth_(NULL),
      contents_size_(contents_size) {
  set_parent_owned(false);

  views::Widget::CreateParams params(views::Widget::CreateParams::TYPE_POPUP);
  params.transparent = true;
  params.keep_on_top = true;
  params.delete_on_destroy = false;
  container_.reset(views::Widget::CreateWidget(params));
#if defined(OS_WIN)
  static_cast<views::WidgetWin*>(container_.get())->
      set_can_update_layered_window(false);

  BOOL drag;
  if ((::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0, &drag, 0) != 0) &&
      (drag == FALSE)) {
    show_contents_on_drag_ = false;
  }
#endif
  container_->Init(NULL, gfx::Rect(0, 0, 0, 0));
  container_->SetContentsView(this);
}

DraggedTabView::~DraggedTabView() {
  parent()->RemoveChildView(this);
  container_->CloseNow();
  STLDeleteElements(&renderers_);
}

void DraggedTabView::MoveTo(const gfx::Point& screen_point) {
  int x;
  if (base::i18n::IsRTL()) {
    // On RTL locales, a dragged tab (when it is not attached to a tab strip)
    // is rendered using a right-to-left orientation so we should calculate the
    // window position differently.
    gfx::Size ps = GetPreferredSize();
    x = screen_point.x() - ScaleValue(ps.width()) + mouse_tab_offset_.x() +
        ScaleValue(renderers_[0]->GetMirroredXInView(mouse_tab_offset_.x()));
  } else {
    x = screen_point.x() + mouse_tab_offset_.x() -
        ScaleValue(mouse_tab_offset_.x());
  }
  int y = screen_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());

#if defined(OS_WIN)
  // TODO(beng): make this cross-platform
  int show_flags = container_->IsVisible() ? SWP_NOZORDER : SWP_SHOWWINDOW;
  SetWindowPos(container_->GetNativeView(), HWND_TOP, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | show_flags);
#else
  gfx::Rect bounds = container_->GetWindowScreenBounds();
  container_->SetBounds(gfx::Rect(x, y, bounds.width(), bounds.height()));
  if (!container_->IsVisible())
    container_->Show();
#endif
}

void DraggedTabView::SetTabWidthAndUpdate(int width,
                                          NativeViewPhotobooth* photobooth) {
  tab_size_.set_width(width);
  photobooth_ = photobooth;
#if defined(OS_WIN)
  container_->SetOpacity(kTransparentAlpha);
#endif
  ResizeContainer();
  Update();
}

void DraggedTabView::Update() {
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, views::View overrides:

void DraggedTabView::OnPaint(gfx::Canvas* canvas) {
  if (show_contents_on_drag_)
    PaintDetachedView(canvas);
  else
    PaintFocusRect(canvas);
}

void DraggedTabView::Layout() {
  int left = 0;
  if (base::i18n::IsRTL())
    left = GetPreferredSize().width() - tab_size_.width();
  // The renderer_'s width should be tab_size_.width() in both LTR and RTL
  // locales. Wrong width will cause the wrong positioning of the tab view in
  // dragging. Please refer to http://crbug.com/6223 for details.
  renderers_[0]->SetBounds(left, 0, tab_size_.width(), tab_size_.height());
}

gfx::Size DraggedTabView::GetPreferredSize() {
  int width = std::max(tab_size_.width(), contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = tab_size_.height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, private:

void DraggedTabView::PaintDetachedView(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  gfx::CanvasSkia scale_canvas(ps.width(), ps.height(), false);
  SkBitmap& bitmap_device = const_cast<SkBitmap&>(
      scale_canvas.getTopPlatformDevice().accessBitmap(true));
  bitmap_device.eraseARGB(0, 0, 0, 0);

  scale_canvas.FillRectInt(kDraggedTabBorderColor, 0,
      tab_size_.height() - kDragFrameBorderSize,
      ps.width(), ps.height() - tab_size_.height());
  int image_x = kDragFrameBorderSize;
  int image_y = tab_size_.height();
  int image_w = ps.width() - kTwiceDragFrameBorderSize;
  int image_h = contents_size_.height();
  scale_canvas.FillRectInt(SK_ColorBLACK, image_x, image_y, image_w, image_h);
  photobooth_->PaintScreenshotIntoCanvas(
      &scale_canvas,
      gfx::Rect(image_x, image_y, image_w, image_h));
  renderers_[0]->Paint(&scale_canvas);

  SkIRect subset;
  subset.set(0, 0, ps.width(), ps.height());
  SkBitmap mipmap = scale_canvas.ExtractBitmap();
  mipmap.buildMipMap(true);

  SkShader* bitmap_shader =
      SkShader::CreateBitmapShader(mipmap, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode);

  SkMatrix shader_scale;
  shader_scale.setScale(kScalingFactor, kScalingFactor);
  bitmap_shader->setLocalMatrix(shader_scale);

  SkPaint paint;
  paint.setShader(bitmap_shader);
  paint.setAntiAlias(true);
  bitmap_shader->unref();

  SkRect rc;
  rc.fLeft = 0;
  rc.fTop = 0;
  rc.fRight = SkIntToScalar(ps.width());
  rc.fBottom = SkIntToScalar(ps.height());
  canvas->AsCanvasSkia()->drawRect(rc, paint);
}

void DraggedTabView::PaintFocusRect(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  canvas->DrawFocusRect(0, 0,
                        static_cast<int>(ps.width() * kScalingFactor),
                        static_cast<int>(ps.height() * kScalingFactor));
}

void DraggedTabView::ResizeContainer() {
  gfx::Size ps = GetPreferredSize();
  int w = ScaleValue(ps.width());
  int h = ScaleValue(ps.height());
#if defined(OS_WIN)
  SetWindowPos(container_->GetNativeView(), HWND_TOPMOST, 0, 0, w, h,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
  gfx::Rect bounds = container_->GetWindowScreenBounds();
  container_->SetBounds(gfx::Rect(bounds.x(), bounds.y(), w, h));
#endif
}

int DraggedTabView::ScaleValue(int value) {
  return static_cast<int>(value * kScalingFactor);
}
