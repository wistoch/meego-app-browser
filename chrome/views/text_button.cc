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

#include "chrome/views/text_button.h"

#include <atlbase.h>
#include <atlapp.h>

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/event.h"
#include "chrome/views/view_menu_delegate.h"
#include "chrome/views/view_container.h"

#include "generated_resources.h"

namespace ChromeViews {

// Padding between the icon and text.
static const int kIconTextPadding = 5;

// Preferred padding between text and edge
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

static const SkColor kEnabledColor = SkColorSetRGB(6, 45, 117);
static const SkColor kHighlightColor = SkColorSetARGB(200, 255, 255, 255);
static const SkColor kDisabledColor = SkColorSetRGB(161, 161, 146);

// How long the hover fade animation should last.
static const int kHoverAnimationDurationMs = 170;

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

TextButtonBorder::TextButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  hot_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_P);
  pushed_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_P);
}

TextButtonBorder::~TextButtonBorder() {
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBackground - painting
//
////////////////////////////////////////////////////////////////////////////////

void TextButtonBorder::Paint(const View& view, ChromeCanvas* canvas) const {
  const TextButton* mb = static_cast<const TextButton*>(&view);
  int state = mb->GetState();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &hot_set_;
  if (state == TextButton::BS_PUSHED)
    set = &pushed_set_;

  if (set) {
    CRect bounds;
    view.GetBounds(&bounds);

    // Draw the top left image
    canvas->DrawBitmapInt(*set->top_left, 0, 0);

    // Tile the top image
    canvas->TileImageInt(*set->top,
                          set->top_left->width(), 0,
                          bounds.Width() - set->top_right->width() - set->top_left->width(),
                          set->top->height());

    // Draw the top right image
    canvas->DrawBitmapInt(*set->top_right,
                          bounds.Width() - set->top_right->width(), 0);

    // Tile the left image
    canvas->TileImageInt(*set->left,
                         0, set->top_left->height(),
                         set->top_left->width(),
                         bounds.Height() - set->top->height() - set->bottom_left->height());

    // Tile the center image
    canvas->TileImageInt(*set->center,
                         set->left->width(), set->top->height(),
                         bounds.Width() - set->right->width() - set->left->width(),
                         bounds.Height() - set->bottom->height() - set->top->height());

    // Tile the right image
    canvas->TileImageInt(*set->right,
                         bounds.Width() - set->right->width(), set->top_right->height(),
                         bounds.Width(), bounds.Height() - set->bottom_right->height() - set->top_right->height());

    // Draw the bottom left image
    canvas->DrawBitmapInt(*set->bottom_left, 0, bounds.Height() - set->bottom_left->height());

    // Tile the bottom image
    canvas->TileImageInt(*set->bottom,
                          set->bottom_left->width(), bounds.Height() - set->bottom->height(),
                          bounds.Width() - set->bottom_right->width() - set->bottom_left->width(),
                          set->bottom->height());

    // Draw the bottom right image
    canvas->DrawBitmapInt(*set->bottom_right,
                          bounds.Width() - set->bottom_right->width(),
                          bounds.Height() -  set->bottom_right->height());
  } else {
    // Do nothing
  }
}

void TextButtonBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
              kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButton - Implementation
//
////////////////////////////////////////////////////////////////////////////////

TextButton::TextButton(const std::wstring& text)
    : max_text_size_(CSize(0, 0)),
      font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      color_(kEnabledColor),
      BaseButton(),
      max_width_(0),
      alignment_(ALIGN_LEFT) {
  SetText(text);
  SetBorder(new TextButtonBorder);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButton::~TextButton() {
}

void TextButton::GetPreferredSize(CSize *result) {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  result->cx = max_text_size_.cx + icon_.width() + insets.width();
  result->cy = std::max(static_cast<int>(max_text_size_.cy), icon_.height()) +
               insets.height();

  if (icon_.width() > 0 && !text_.empty())
    result->cx += kIconTextPadding;

  if (max_width_ > 0)
    result->cx = std::min(max_width_, static_cast<int>(result->cx));
}

void TextButton::GetMinimumSize(CSize *result) {
  result->cx = max_text_size_.cx;
  result->cy = max_text_size_.cy;
}

bool TextButton::OnMousePressed(const ChromeViews::MouseEvent& e) {
  return true;
}

void TextButton::SetText(const std::wstring& text) {
  text_ = text;
  // Update our new current and max text size
  text_size_.cx = font_.GetStringWidth(text_);
  text_size_.cy = font_.height();
  max_text_size_.cx = std::max(max_text_size_.cx, text_size_.cx);
  max_text_size_.cy = std::max(max_text_size_.cy, text_size_.cy);
}

void TextButton::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
}

void TextButton::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButton::Paint(ChromeCanvas* canvas) {
  Paint(canvas, false);
}

void TextButton::Paint(ChromeCanvas* canvas, bool for_drag) {
  if (!for_drag) {
    PaintBackground(canvas);

    if (hover_animation_->IsAnimating()) {
      // Draw the hover bitmap into an offscreen buffer, then blend it
      // back into the current canvas.
      canvas->saveLayerAlpha(NULL,
          static_cast<int>(hover_animation_->GetCurrentValue() * 255),
          SkCanvas::kARGB_NoClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkPorterDuff::kClear_Mode);
      PaintBorder(canvas);
      canvas->restore();
    } else if (state_ == BS_HOT || state_ == BS_PUSHED) {
      PaintBorder(canvas);
    }

    PaintFocusBorder(canvas);
  }

  gfx::Insets insets = GetInsets();
  int available_width = GetWidth() - insets.width();
  int available_height = GetHeight() - insets.height();
  // Use the actual text (not max) size to properly center the text.
  int content_width = text_size_.cx;
  if (icon_.width() > 0) {
    content_width += icon_.width();
    if (!text_.empty())
      content_width += kIconTextPadding;
  }
  // Place the icon along the left edge.
  int icon_x;
  if (alignment_ == ALIGN_LEFT) {
    icon_x = insets.left();
  } else if (alignment_ == ALIGN_RIGHT) {
    icon_x = available_width - content_width;
  } else {
    icon_x =
        std::max(0, (available_width - content_width) / 2) + insets.left();
  }
  int text_x = icon_x;
  if (icon_.width() > 0)
    text_x += icon_.width() + kIconTextPadding;
  const int text_width = std::min(static_cast<int>(text_size_.cx),
                                  GetWidth() - insets.right() - text_x);
  int text_y = (available_height - text_size_.cy) / 2 + insets.top();

  if (text_width > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction insentisive bitmaps and such bitmaps should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    gfx::Rect text_bounds(text_x, text_y, text_width, text_size_.cy);
    text_bounds.set_x(MirroredLeftPointForRect(text_bounds));

    // Draw bevel highlight
    canvas->DrawStringInt(text_,
                          font_,
                          kHighlightColor,
                          text_bounds.x() + 1,
                          text_bounds.y() + 1,
                          text_bounds.width(),
                          text_bounds.height());

    canvas->DrawStringInt(text_,
                          font_,
                          color_,
                          text_bounds.x(),
                          text_bounds.y(),
                          text_bounds.width(),
                          text_bounds.height());
  }

  if (icon_.width() > 0) {
    int icon_y = (available_height - icon_.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon_.width(), icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }
}

void TextButton::SetEnabled(bool enabled) {
  if (enabled == IsEnabled())
    return;
  BaseButton::SetEnabled(enabled);
  color_ = enabled ? kEnabledColor : kDisabledColor;
  SchedulePaint();
}

}  // namespace ChromeViews
