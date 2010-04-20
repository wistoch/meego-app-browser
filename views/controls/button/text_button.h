// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_

#include "gfx/font.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/border.h"
#include "views/controls/button/custom_button.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder
//
//  A Border subclass that paints a TextButton's background layer -
//  basically the button frame in the hot/pushed states.
//
// Note that this type of button is not focusable by default and will not be
// part of the focus chain.  Call SetFocusable(true) to make it part of the
// focus chain.
//
////////////////////////////////////////////////////////////////////////////////
class TextButtonBorder : public Border {
 public:
  TextButtonBorder();
  virtual ~TextButtonBorder();

  // Render the background for the provided view
  virtual void Paint(const View& view, gfx::Canvas* canvas) const;

  // Returns the insets for the border.
  virtual void GetInsets(gfx::Insets* insets) const;

 protected:
  // Images
  struct MBBImageSet {
    SkBitmap* top_left;
    SkBitmap* top;
    SkBitmap* top_right;
    SkBitmap* left;
    SkBitmap* center;
    SkBitmap* right;
    SkBitmap* bottom_left;
    SkBitmap* bottom;
    SkBitmap* bottom_right;
  };
  MBBImageSet hot_set_;
  MBBImageSet pushed_set_;

  virtual void Paint(const View& view, gfx::Canvas* canvas,
      const MBBImageSet& set) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextButtonBorder);
};


////////////////////////////////////////////////////////////////////////////////
//
// TextButton
//
//  A button which displays text and/or and icon that can be changed in
//  response to actions. TextButton reserves space for the largest string
//  passed to SetText. To reset the cached max size invoke ClearMaxTextSize.
//
////////////////////////////////////////////////////////////////////////////////
class TextButton : public CustomButton {
 public:
  TextButton(ButtonListener* listener, const std::wstring& text);
  virtual ~TextButton();

  // Call SetText once per string in your set of possible values at button
  // creation time, so that it can contain the largest of them and avoid
  // resizing the button when the text changes.
  virtual void SetText(const std::wstring& text);
  std::wstring text() const { return text_; }

  enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  void set_alignment(TextAlignment alignment) { alignment_ = alignment; }

  // Sets the icon.
  void SetIcon(const SkBitmap& icon);
  SkBitmap icon() const { return icon_; }
  void SetHoverIcon(const SkBitmap& icon);
  SkBitmap icon_hover() const { return icon_hover_; }

  // Meanings are reversed for right-to-left layouts.
  enum IconPlacement {
    ICON_ON_LEFT,
    ICON_ON_RIGHT
  };

  IconPlacement icon_placement() { return icon_placement_; }
  void set_icon_placement(IconPlacement icon_placement) {
    icon_placement_ = icon_placement;
  }

  // TextButton remembers the maximum display size of the text passed to
  // SetText. This method resets the cached maximum display size to the
  // current size.
  void ClearMaxTextSize();

  void set_max_width(int max_width) { max_width_ = max_width; }
  void SetFont(const gfx::Font& font);
  void SetEnabledColor(SkColor color);
  void SetDisabledColor(SkColor color);
  void SetHighlightColor(SkColor color);
  void SetHoverColor(SkColor color);
  void SetNormalHasBorder(bool normal_has_border);
  // Sets whether or not to show the highlighed (i.e. hot) state. Default true.
  void SetShowHighlighted(bool show_highlighted);

  // Paint the button into the specified canvas. If |for_drag| is true, the
  // function paints a drag image representation into the canvas.
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual gfx::Size GetMinimumSize();
  virtual void SetEnabled(bool enabled);

  // Text colors.
  static const SkColor kEnabledColor;
  static const SkColor kHighlightColor;
  static const SkColor kDisabledColor;
  static const SkColor kHoverColor;

 protected:
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual void Paint(gfx::Canvas* canvas);

  // Called when enabled or disabled state changes, or the colors for those
  // states change.
  virtual void UpdateColor();

 private:
  // The text string that is displayed in the button.
  std::wstring text_;

  // The size of the text string.
  gfx::Size text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  gfx::Size max_text_size_;

  // The alignment of the text string within the button.
  TextAlignment alignment_;

  // The position of the icon.
  IconPlacement icon_placement_;

  // The font used to paint the text.
  gfx::Font font_;

  // Text color.
  SkColor color_;

  // State colors.
  SkColor color_enabled_;
  SkColor color_disabled_;
  SkColor color_highlight_;
  SkColor color_hover_;

  // An icon displayed with the text.
  SkBitmap icon_;

  // An optional different version of the icon for hover state.
  SkBitmap icon_hover_;
  bool has_hover_icon_;

  // The width of the button will never be larger than this value. A value <= 0
  // indicates the width is not constrained.
  int max_width_;

  // This is true if normal state has a border frame; default is false.
  bool normal_has_border_;

  // Whether or not to show the highlighted (i.e. hot) state.
  bool show_highlighted_;

  DISALLOW_COPY_AND_ASSIGN(TextButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
