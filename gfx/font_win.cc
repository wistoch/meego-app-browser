// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/font.h"

#include <windows.h>
#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "gfx/canvas.h"

namespace gfx {

// static
Font::HFontRef* Font::base_font_ref_;

// static
Font::AdjustFontCallback Font::adjust_font_callback = NULL;
Font::GetMinimumFontSizeCallback Font::get_minimum_font_size_callback = NULL;

// If the tmWeight field of a TEXTMETRIC structure has a value >= this, the
// font is bold.
static const int kTextMetricWeightBold = 700;

// Returns either minimum font allowed for a current locale or
// lf_height + size_delta value.
static int AdjustFontSize(int lf_height, int size_delta) {
  if (lf_height < 0) {
    lf_height -= size_delta;
  } else {
    lf_height += size_delta;
  }
  int min_font_size = 0;
  if (Font::get_minimum_font_size_callback)
    min_font_size = Font::get_minimum_font_size_callback();
  // Make sure lf_height is not smaller than allowed min font size for current
  // locale.
  if (abs(lf_height) < min_font_size) {
    return lf_height < 0 ? -min_font_size : min_font_size;
  } else {
    return lf_height;
  }
}

//
// Font
//

Font::Font()
    : font_ref_(GetBaseFontRef()) {
}

int Font::height() const {
  return font_ref_->height();
}

int Font::baseline() const {
  return font_ref_->baseline();
}

int Font::ave_char_width() const {
  return font_ref_->ave_char_width();
}

int Font::GetExpectedTextWidth(int length) const {
  return length * std::min(font_ref_->dlu_base_x(), ave_char_width());
}

int Font::style() const {
  return font_ref_->style();
}

NativeFont Font::nativeFont() const {
  return hfont();
}

// static
Font Font::CreateFont(HFONT font) {
  DCHECK(font);
  LOGFONT font_info;
  GetObject(font, sizeof(LOGFONT), &font_info);
  return Font(CreateHFontRef(CreateFontIndirect(&font_info)));
}

Font Font::CreateFont(const std::wstring& font_name, int font_size) {
  HDC hdc = GetDC(NULL);
  long lf_height = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(NULL, hdc);
  HFONT hf = ::CreateFont(lf_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    font_name.c_str());
  return Font::CreateFont(hf);
}

// static
Font::HFontRef* Font::GetBaseFontRef() {
  if (base_font_ref_ == NULL) {
    NONCLIENTMETRICS metrics;
    win_util::GetNonClientMetrics(&metrics);

    if (adjust_font_callback)
      adjust_font_callback(&metrics.lfMessageFont);
    metrics.lfMessageFont.lfHeight =
        AdjustFontSize(metrics.lfMessageFont.lfHeight, 0);
    HFONT font = CreateFontIndirect(&metrics.lfMessageFont);
    DLOG_ASSERT(font);
    base_font_ref_ = Font::CreateHFontRef(font);
    // base_font_ref_ is global, up the ref count so it's never deleted.
    base_font_ref_->AddRef();
  }
  return base_font_ref_;
}

const std::wstring& Font::FontName() const {
  return font_ref_->font_name();
}

int Font::FontSize() {
  LOGFONT font_info;
  GetObject(hfont(), sizeof(LOGFONT), &font_info);
  long lf_height = font_info.lfHeight;
  HDC hdc = GetDC(NULL);
  int device_caps = GetDeviceCaps(hdc, LOGPIXELSY);
  int font_size = 0;
  if (device_caps != 0) {
    float font_size_float = -static_cast<float>(lf_height)*72/device_caps;
    font_size = static_cast<int>(::ceil(font_size_float - 0.5));
  }
  ReleaseDC(NULL, hdc);
  return font_size;
}

Font::HFontRef::HFontRef(HFONT hfont,
         int height,
         int baseline,
         int ave_char_width,
         int style,
         int dlu_base_x)
    : hfont_(hfont),
      height_(height),
      baseline_(baseline),
      ave_char_width_(ave_char_width),
      style_(style),
      dlu_base_x_(dlu_base_x) {
  DLOG_ASSERT(hfont);

  LOGFONT font_info;
  GetObject(hfont_, sizeof(LOGFONT), &font_info);
  font_name_ = std::wstring(font_info.lfFaceName);
}

Font::HFontRef::~HFontRef() {
  DeleteObject(hfont_);
}

Font Font::DeriveFont(int size_delta, int style) const {
  LOGFONT font_info;
  GetObject(hfont(), sizeof(LOGFONT), &font_info);
  font_info.lfHeight = AdjustFontSize(font_info.lfHeight, size_delta);
  font_info.lfUnderline = ((style & UNDERLINED) == UNDERLINED);
  font_info.lfItalic = ((style & ITALIC) == ITALIC);
  font_info.lfWeight = (style & BOLD) ? FW_BOLD : FW_NORMAL;

  HFONT hfont = CreateFontIndirect(&font_info);
  return Font(CreateHFontRef(hfont));
}

int Font::GetStringWidth(const std::wstring& text) const {
  int width = 0, height = 0;
  Canvas::SizeStringInt(text, *this, &width, &height, gfx::Canvas::NO_ELLIPSIS);
  return width;
}

Font::HFontRef* Font::CreateHFontRef(HFONT font) {
  TEXTMETRIC font_metrics;
  HDC screen_dc = GetDC(NULL);
  HFONT previous_font = static_cast<HFONT>(SelectObject(screen_dc, font));
  int last_map_mode = SetMapMode(screen_dc, MM_TEXT);
  GetTextMetrics(screen_dc, &font_metrics);
  // Yes, this is how Microsoft recommends calculating the dialog unit
  // conversions.
  SIZE ave_text_size;
  GetTextExtentPoint32(screen_dc,
                       L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                       52, &ave_text_size);
  const int dlu_base_x = (ave_text_size.cx / 26 + 1) / 2;
  // To avoid the DC referencing font_handle_, select the previous font.
  SelectObject(screen_dc, previous_font);
  SetMapMode(screen_dc, last_map_mode);
  ReleaseDC(NULL, screen_dc);

  const int height = std::max(1, static_cast<int>(font_metrics.tmHeight));
  const int baseline = std::max(1, static_cast<int>(font_metrics.tmAscent));
  const int ave_char_width =
      std::max(1, static_cast<int>(font_metrics.tmAveCharWidth));
  int style = 0;
  if (font_metrics.tmItalic) {
    style |= Font::ITALIC;
  }
  if (font_metrics.tmUnderlined) {
    style |= Font::UNDERLINED;
  }
  if (font_metrics.tmWeight >= kTextMetricWeightBold) {
    style |= Font::BOLD;
  }

  return new HFontRef(font, height, baseline, ave_char_width, style,
                      dlu_base_x);
}

}  // namespace gfx
