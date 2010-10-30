// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/shutdown_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"

namespace {

// Bottom/Right Padding for Shutdown button.
const int kBottomPadding = 12;
const int kRightPadding = 12;

class HoverBackground : public views::Background {
 public:
  HoverBackground(views::Background* normal, views::Background* hover)
      : normal_(normal), hover_(hover) {
  }

  // views::Background implementation.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const {
    views::TextButton* button = static_cast<views::TextButton*>(view);
    if (button->state() == views::CustomButton::BS_HOT) {
      hover_->Paint(canvas, view);
    } else {
      normal_->Paint(canvas, view);
    }
  }

 private:
  views::Background* normal_;
  views::Background* hover_;

  DISALLOW_COPY_AND_ASSIGN(HoverBackground);
};

}  // namespace

namespace chromeos {

ShutdownButton::ShutdownButton()
    : ALLOW_THIS_IN_INITIALIZER_LIST(TextButton(this, std::wstring())) {
}

void ShutdownButton::Init() {
  SkColor kButtonColor = 0xFF242A35;
  SkColor kHoverColor = 0xFF353E4E;
  int kVerticalPadding = 13;
  int kIconTextPadding = 10;
  int kHorizontalPadding = 13;
  int kCornerRadius = 4;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetBitmapNamed(IDR_SHUTDOWN_ICON));
  set_icon_text_spacing(kIconTextPadding);
  SetFocusable(true);
  // Set label colors.
  SetEnabledColor(SK_ColorWHITE);
  SetDisabledColor(SK_ColorWHITE);
  SetHighlightColor(SK_ColorWHITE);
  SetHoverColor(SK_ColorWHITE);
  // Disable throbbing and make border always visible.
  SetAnimationDuration(0);
  SetNormalHasBorder(true);
  // Setup round shapes.
  set_background(
      new HoverBackground(
          CreateRoundedBackground(
              kCornerRadius, 0, kButtonColor, 0),
          CreateRoundedBackground(
              kCornerRadius, 0, kHoverColor, 0)));
  set_border(
      views::Border::CreateEmptyBorder(kVerticalPadding,
                                       kHorizontalPadding,
                                       kVerticalPadding,
                                       kHorizontalPadding));
  OnLocaleChanged();  // set text
}

void ShutdownButton::LayoutIn(views::View* parent) {
  // No RTL for now. RTL will be handled in new
  // domui based Login/Locker.
  gfx::Size button_size = GetPreferredSize();
  SetBounds(
      parent->width() - button_size.width()- kRightPadding,
      parent->height() - button_size.height() - kBottomPadding,
      button_size.width(),
      button_size.height());
}

void ShutdownButton::OnLocaleChanged() {
  SetText(UTF8ToWide(l10n_util::GetStringUTF8(IDS_SHUTDOWN_BUTTON)));
  if (GetParent()) {
    GetParent()->Layout();
    GetParent()->SchedulePaint();
  }
}

gfx::NativeCursor ShutdownButton::GetCursorForPoint(
    views::Event::EventType event_type,
    const gfx::Point& p) {
  if (!IsEnabled()) {
    return NULL;
  }
  return gdk_cursor_new(GDK_HAND2);
}

void ShutdownButton::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  CrosLibrary::Get()->GetPowerLibrary()->RequestShutdown();
}

}  // namespace chromeos
