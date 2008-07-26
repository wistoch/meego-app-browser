// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/views/sad_tab_view.h"

#include "base/gfx/size.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "generated_resources.h"
#include "skia/include/SkGradientShader.h"

static const int kSadTabOffset = -64;
static const int kIconTitleSpacing = 20;
static const int kTitleMessageSpacing = 15;
static const int kMessageBottomMargin = 20;
static const float kMessageSize = 0.65f;
static const SkColor kTitleColor = SK_ColorWHITE;
static const SkColor kMessageColor = SK_ColorWHITE;
static const SkColor kBackgroundColor = SkColorSetRGB(35, 48, 64);
static const SkColor kBackgroundEndColor = SkColorSetRGB(35, 48, 64);

// static
SkBitmap* SadTabView::sad_tab_bitmap_ = NULL;
ChromeFont SadTabView::title_font_;
ChromeFont SadTabView::message_font_;
std::wstring SadTabView::title_;
std::wstring SadTabView::message_;
int SadTabView::title_width_;

SadTabView::SadTabView() {
  InitClass();
}

static SkShader* CreateGradientShader(int end_point) {
  SkColor grad_colors[2] = { kBackgroundColor, kBackgroundEndColor };
  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
  grad_points[1].set(SkIntToScalar(0), SkIntToScalar(end_point));
  return SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode);
}

void SadTabView::Paint(ChromeCanvas* canvas) {
  SkShader* background_shader = CreateGradientShader(GetHeight());
  SkPaint paint;
  paint.setShader(background_shader);
  background_shader->unref();
  paint.setStyle(SkPaint::kFill_Style);
  canvas->drawRectCoords(0, 0,
                         SkIntToScalar(GetWidth()), SkIntToScalar(GetHeight()),
                         paint);

  canvas->DrawBitmapInt(*sad_tab_bitmap_, icon_bounds_.x(), icon_bounds_.y());

  canvas->DrawStringInt(title_, title_font_, kTitleColor, title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height(),
                        ChromeCanvas::TEXT_ALIGN_CENTER);

  canvas->DrawStringInt(message_, message_font_, kMessageColor,
                        message_bounds_.x(), message_bounds_.y(),
                        message_bounds_.width(), message_bounds_.height(),
                        ChromeCanvas::MULTI_LINE);
}

void SadTabView::Layout() {
  int icon_width = sad_tab_bitmap_->width();
  int icon_height = sad_tab_bitmap_->height();
  int icon_x = (GetWidth() - icon_width) / 2;
  int icon_y = ((GetHeight() - icon_height) / 2) + kSadTabOffset;
  icon_bounds_.SetRect(icon_x, icon_y, icon_width, icon_height);

  int title_x = (GetWidth() - title_width_) / 2;
  int title_y = icon_bounds_.bottom() + kIconTitleSpacing;
  int title_height = title_font_.height();
  title_bounds_.SetRect(title_x, title_y, title_width_, title_height);

  ChromeCanvas cc(0, 0, true);
  int message_width = static_cast<int>(GetWidth() * kMessageSize);
  int message_height = 0;
  cc.SizeStringInt(message_, message_font_, &message_width, &message_height,
                   ChromeCanvas::MULTI_LINE);
  int message_x = (GetWidth() - message_width) / 2;
  int message_y = title_bounds_.bottom() + kTitleMessageSpacing;
  message_bounds_.SetRect(message_x, message_y, message_width, message_height);
}

void SadTabView::DidChangeBounds(const CRect&, const CRect&) {
  Layout();
}

// static
void SadTabView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    title_font_ = rb.GetFont(ResourceBundle::BaseFont).
        DeriveFont(2, ChromeFont::BOLD);
    message_font_ = rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1);
    sad_tab_bitmap_ = rb.GetBitmapNamed(IDR_SAD_TAB);

    title_ = l10n_util::GetString(IDS_SAD_TAB_TITLE);
    title_width_ = title_font_.GetStringWidth(title_);
    message_ = l10n_util::GetString(IDS_SAD_TAB_MESSAGE);

    initialized = true;
  }
}

