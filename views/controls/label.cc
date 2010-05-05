// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/label.h"

#include <cmath>
#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/text_elider.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"
#include "gfx/font.h"
#include "gfx/insets.h"
#include "views/background.h"

namespace views {

// static
const char Label::kViewClassName[] = "views/Label";
SkColor Label::kEnabledColor, Label::kDisabledColor;

static const int kFocusBorderPadding = 1;

Label::Label() {
  Init(std::wstring(), GetDefaultFont());
}

Label::Label(const std::wstring& text) {
  Init(text, GetDefaultFont());
}

Label::Label(const std::wstring& text, const gfx::Font& font) {
  Init(text, font);
}

Label::~Label() {
}

gfx::Size Label::GetPreferredSize() {
  // Return a size of (0, 0) if the label is not visible and if the
  // collapse_when_hidden_ flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need ot respect the collapse_when_hidden_ flag.
  if (!IsVisible() && collapse_when_hidden_)
    return gfx::Size();

  gfx::Size prefsize(GetTextSize());
  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

int Label::GetBaseline() {
  return GetInsets().top() + font_.baseline();
}

int Label::GetHeightForWidth(int w) {
  if (!is_multi_line_)
    return View::GetHeightForWidth(w);

  w = std::max(0, w - GetInsets().width());
  int h = font_.height();
  gfx::Canvas::SizeStringInt(text_, font_, &w, &h, ComputeMultiLineFlags());
  return h + GetInsets().height();
}

void Label::DidChangeBounds(const gfx::Rect& previous,
                            const gfx::Rect& current) {
  text_size_valid_ &= !is_multi_line_;
}

void Label::Paint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
  std::wstring paint_text;
  gfx::Rect text_bounds;
  int flags = 0;
  CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  canvas->DrawStringInt(paint_text, font_, color_,
                        text_bounds.x(), text_bounds.y(),
                        text_bounds.width(), text_bounds.height(), flags);

  if (HasFocus() || paint_as_focused_) {
    text_bounds.Inset(-kFocusBorderPadding, -kFocusBorderPadding);
    canvas->DrawFocusRect(text_bounds.x(), text_bounds.y(),
                          text_bounds.width(), text_bounds.height());
  }
}

void Label::PaintBackground(gfx::Canvas* canvas) {
  const Background* bg = contains_mouse_ ? GetMouseOverBackground() : NULL;
  if (!bg)
    bg = background();
  if (bg)
    bg->Paint(canvas, this);
}

void Label::SetFont(const gfx::Font& font) {
  font_ = font;
  text_size_valid_ = false;
  SchedulePaint();
}

void Label::SetText(const std::wstring& text) {
  text_ = text;
  url_set_ = false;
  text_size_valid_ = false;
  SetAccessibleName(text);
  SchedulePaint();
}

const std::wstring Label::GetText() const {
  return url_set_ ? UTF8ToWide(url_.spec()) : text_;
}

void Label::SetURL(const GURL& url) {
  url_ = url;
  text_ = UTF8ToWide(url_.spec());
  url_set_ = true;
  text_size_valid_ = false;
  SchedulePaint();
}

const GURL Label::GetURL() const {
  return url_set_ ? url_ : GURL(WideToUTF8(text_));
}

void Label::SetHorizontalAlignment(Alignment a) {
  // If the View's UI layout is right-to-left and rtl_alignment_mode_ is
  // USE_UI_ALIGNMENT, we need to flip the alignment so that the alignment
  // settings take into account the text directionality.
  if (UILayoutIsRightToLeft() && (rtl_alignment_mode_ == USE_UI_ALIGNMENT) &&
      (a != ALIGN_CENTER))
    a = (a == ALIGN_LEFT) ? ALIGN_RIGHT : ALIGN_LEFT;
  if (horiz_alignment_ != a) {
    horiz_alignment_ = a;
    SchedulePaint();
  }
}

void Label::SetMultiLine(bool f) {
  if (f != is_multi_line_) {
    is_multi_line_ = f;
    text_size_valid_ = false;
    SchedulePaint();
  }
}

void Label::SetAllowCharacterBreak(bool f) {
  if (f != allow_character_break_) {
    allow_character_break_ = f;
    text_size_valid_ = false;
    SchedulePaint();
  }
}

void Label::SetTooltipText(const std::wstring& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool Label::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  DCHECK(tooltip);

  // If a tooltip has been explicitly set, use it.
  if (!tooltip_text_.empty()) {
    tooltip->assign(tooltip_text_);
    return true;
  }

  // Show the full text if the text does not fit.
  if (!is_multi_line_ && font_.GetStringWidth(text_) > width()) {
    *tooltip = text_;
    return true;
  }
  return false;
}

void Label::OnMouseMoved(const MouseEvent& e) {
  UpdateContainsMouse(e);
}

void Label::OnMouseEntered(const MouseEvent& event) {
  UpdateContainsMouse(event);
}

void Label::OnMouseExited(const MouseEvent& event) {
  SetContainsMouse(false);
}

void Label::SetMouseOverBackground(Background* background) {
  mouse_over_background_.reset(background);
}

const Background* Label::GetMouseOverBackground() const {
  return mouse_over_background_.get();
}

void Label::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;
  View::SetEnabled(enabled);
  SetColor(enabled ? kEnabledColor : kDisabledColor);
}

gfx::Insets Label::GetInsets() const {
  gfx::Insets insets = View::GetInsets();
  if (IsFocusable() || has_focus_border_)  {
    insets += gfx::Insets(kFocusBorderPadding, kFocusBorderPadding,
                          kFocusBorderPadding, kFocusBorderPadding);
  }
  return insets;
}

void Label::SizeToFit(int max_width) {
  DCHECK(is_multi_line_);

  std::vector<std::wstring> lines;
  SplitString(text_, L'\n', &lines);

  int label_width = 0;
  for (std::vector<std::wstring>::const_iterator iter = lines.begin();
       iter != lines.end(); ++iter)
    label_width = std::max(label_width, font_.GetStringWidth(*iter));

  label_width += GetInsets().width();

  if (max_width > 0)
    label_width = std::min(label_width, max_width);

  SetBounds(x(), y(), label_width, 0);
  SizeToPreferredSize();
}

bool Label::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_STATICTEXT;
  return true;
}

bool Label::GetAccessibleState(AccessibilityTypes::State* state) {
  DCHECK(state);

  *state = AccessibilityTypes::STATE_READONLY;
  return true;
}

void Label::SetHasFocusBorder(bool has_focus_border) {
  has_focus_border_ = has_focus_border;
  text_size_valid_ &= !is_multi_line_;
}

// static
gfx::Font Label::GetDefaultFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

void Label::Init(const std::wstring& text, const gfx::Font& font) {
  static bool initialized = false;
  if (!initialized) {
#if defined(OS_WIN)
    kEnabledColor = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    kDisabledColor = color_utils::GetSysSkColor(COLOR_GRAYTEXT);
#else
    // TODO(beng): source from theme provider.
    kEnabledColor = SK_ColorBLACK;
    kDisabledColor = SK_ColorGRAY;
#endif

    initialized = true;
  }

  contains_mouse_ = false;
  font_ = font;
  text_size_valid_ = false;
  SetText(text);
  url_set_ = false;
  color_ = kEnabledColor;
  horiz_alignment_ = ALIGN_CENTER;
  is_multi_line_ = false;
  allow_character_break_ = false;
  collapse_when_hidden_ = false;
  rtl_alignment_mode_ = USE_UI_ALIGNMENT;
  paint_as_focused_ = false;
  has_focus_border_ = false;
}

void Label::CalculateDrawStringParams(std::wstring* paint_text,
                                      gfx::Rect* text_bounds,
                                      int* flags) const {
  DCHECK(paint_text && text_bounds && flags);

  if (url_set_) {
    // TODO(jungshik) : Figure out how to get 'intl.accept_languages'
    // preference and use it when calling ElideUrl.
    *paint_text = gfx::ElideUrl(url_, font_, width(), std::wstring());

    // An URLs is always treated as an LTR text and therefore we should
    // explicitly mark it as such if the locale is RTL so that URLs containing
    // Hebrew or Arabic characters are displayed correctly.
    //
    // Note that we don't check the View's UI layout setting in order to
    // determine whether or not to insert the special Unicode formatting
    // characters. We use the locale settings because an URL is always treated
    // as an LTR string, even if its containing view does not use an RTL UI
    // layout.
    base::i18n::GetDisplayStringInLTRDirectionality(paint_text);
  } else {
    *paint_text = text_;
  }

  *text_bounds = GetTextBounds();
  *flags = ComputeMultiLineFlags();
}

void Label::UpdateContainsMouse(const MouseEvent& event) {
  SetContainsMouse(GetTextBounds().Contains(event.x(), event.y()));
}

void Label::SetContainsMouse(bool contains_mouse) {
  if (contains_mouse_ == contains_mouse)
    return;
  contains_mouse_ = contains_mouse;
  if (GetMouseOverBackground())
    SchedulePaint();
}

gfx::Rect Label::GetTextBounds() const {
  gfx::Rect available_rect(GetAvailableRect());
  gfx::Size text_size(GetTextSize());
  text_size.set_width(std::min(available_rect.width(), text_size.width()));

  gfx::Insets insets = GetInsets();
  gfx::Point text_origin(insets.left(), insets.top());
  switch (horiz_alignment_) {
    case ALIGN_LEFT:
      break;
    case ALIGN_CENTER:
      // We put any extra margin pixel on the left rather than the right.  We
      // used to do this because measurement on Windows used
      // GetTextExtentPoint32(), which could report a value one too large on the
      // right; we now use DrawText(), and who knows if it can also do this.
      text_origin.Offset((available_rect.width() + 1 - text_size.width()) / 2,
                         0);
      break;
    case ALIGN_RIGHT:
      text_origin.set_x(available_rect.right() - text_size.width());
      break;
    default:
      NOTREACHED();
      break;
  }
  text_origin.Offset(0,
      std::max(0, (available_rect.height() - text_size.height())) / 2);
  return gfx::Rect(text_origin, text_size);
}

gfx::Size Label::GetTextSize() const {
  if (!text_size_valid_) {
    // For single-line strings, we supply the largest possible width, because
    // while adding NO_ELLIPSIS to the flags works on Windows for forcing
    // SizeStringInt() to calculate the desired width, it doesn't seem to work
    // on Linux.
    int w = is_multi_line_ ?
        GetAvailableRect().width() : std::numeric_limits<int>::max();
    int h = font_.height();
    // For single-line strings, ignore the available width and calculate how
    // wide the text wants to be.
    int flags = ComputeMultiLineFlags();
    if (!is_multi_line_)
      flags |= gfx::Canvas::NO_ELLIPSIS;
    gfx::Canvas::SizeStringInt(text_, font_, &w, &h, flags);
    text_size_.SetSize(w, h);
    text_size_valid_ = true;
  }

  return text_size_;
}

int Label::ComputeMultiLineFlags() const {
  if (!is_multi_line_)
    return 0;

  int flags = gfx::Canvas::MULTI_LINE;
#if !defined(OS_WIN)
    // Don't elide multiline labels on Linux.
    // Todo(davemoore): Do we depend on eliding multiline text?
    // Pango insists on limiting the number of lines to one if text is
    // elided. You can get around this if you can pass a maximum height
    // but we don't currently have that data when we call the pango code.
    flags |= gfx::Canvas::NO_ELLIPSIS;
#endif
  if (allow_character_break_)
    flags |= gfx::Canvas::CHARACTER_BREAK;
  switch (horiz_alignment_) {
    case ALIGN_LEFT:
      flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
      break;
    case ALIGN_CENTER:
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
      break;
    case ALIGN_RIGHT:
      flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
      break;
  }
  return flags;
}

gfx::Rect Label::GetAvailableRect() const {
  gfx::Rect bounds(gfx::Point(), size());
  gfx::Insets insets(GetInsets());
  bounds.Inset(insets.left(), insets.top(), insets.right(), insets.bottom());
  return bounds;
}

}  // namespace views
