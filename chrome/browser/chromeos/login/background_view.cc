// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/background_view.h"

#include "app/x11_util.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/language_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

using views::WidgetGtk;

namespace chromeos {

BackgroundView::BackgroundView() : status_area_(NULL), did_paint_(false) {
  views::Painter* painter = chromeos::CreateWizardPainter(
      &chromeos::BorderDefinition::kWizardBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  InitStatusArea();
}

void BackgroundView::Init() {
  InitStatusArea();
  Layout();
  status_area_->SchedulePaint();
}

void BackgroundView::Teardown() {
  RemoveAllChildViews(true);
  status_area_ = NULL;
}

static void ResetXCursor() {
  // TODO(sky): nuke this once new window manager is in place.
  // This gets rid of the ugly X default cursor.
  Display* display = x11_util::GetXDisplay();
  Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
  XID root_window = x11_util::GetX11RootWindow();
  XSetWindowAttributes attr;
  attr.cursor = cursor;
  XChangeWindowAttributes(display, root_window, CWCursor, &attr);
}

// static
views::Widget* BackgroundView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    BackgroundView** view) {
  ResetXCursor();

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, bounds);
  *view = new BackgroundView();
  window->SetContentsView(*view);

  (*view)->UpdateWindowType();

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  return window;
}

void BackgroundView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);
  if (!did_paint_) {
    did_paint_ = true;
    UpdateWindowType();
  }
}

void BackgroundView::Layout() {
  int right_top_padding =
      chromeos::BorderDefinition::kWizardBorder.padding +
      chromeos::BorderDefinition::kWizardBorder.corner_radius / 2;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - right_top_padding,
      right_top_padding,
      status_area_size.width(),
      status_area_size.height());
}

void BackgroundView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

gfx::NativeWindow BackgroundView::GetNativeWindow() const {
  return
      GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

bool BackgroundView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->clock_view() ||
      button_view == status_area_->language_view() ||
      button_view == status_area_->network_view()) {
    return false;
  }
  return true;
}

void BackgroundView::OpenButtonOptions(const views::View* button_view) const {
  // TODO(avayvod): Add some dialog for options or remove them completely.
}

bool BackgroundView::IsButtonVisible(const views::View* button_view) const {
  return true;
}

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

void BackgroundView::UpdateWindowType() {
  std::vector<int> params;
  params.push_back(did_paint_ ? 1 : 0);
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      chromeos::WmIpc::WINDOW_TYPE_LOGIN_BACKGROUND,
      &params);
}

}  // namespace chromeos
