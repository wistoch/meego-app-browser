// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_overview_snapshot.h"

#include <vector>

#include "app/x11_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "views/border.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"

using views::ColumnSet;
using views::GridLayout;
using std::vector;

#if !defined(OS_CHROMEOS)
#error This file is only meant to be compiled for ChromeOS
#endif

namespace chromeos {

WmOverviewSnapshot::WmOverviewSnapshot()
  : WidgetGtk(TYPE_WINDOW),
    snapshot_view_(NULL),
    index_(-1),
    configured_snapshot_(false) {
}

void WmOverviewSnapshot::Init(const gfx::Size& size,
                              Browser* browser,
                              int index) {
  snapshot_view_ = new views::ImageView();
  MakeTransparent();

  snapshot_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  snapshot_view_->set_border(
      views::Border::CreateSolidBorder(1, SkColorSetRGB(176, 176, 176)));

  WidgetGtk::Init(NULL, gfx::Rect(gfx::Point(0,0), size));

  SetContentsView(snapshot_view_);

  UpdateIndex(browser, index);
}


void WmOverviewSnapshot::UpdateIndex(Browser* browser, int index) {
  vector<int> params;
  params.push_back(x11_util::GetX11WindowFromGtkWidget(
      GTK_WIDGET(browser->window()->GetNativeHandle())));
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      GetNativeView(),
      WmIpc::WINDOW_TYPE_CHROME_TAB_SNAPSHOT,
      &params);
  index_ = index;
}

void WmOverviewSnapshot::SetImage(const SkBitmap& image) {
  CHECK(snapshot_view_) << "Init not called before setting image.";
  snapshot_view_->SetImage(image);

  // Reset the bounds to the size of the image.
  gfx::Rect bounds;
  GetBounds(&bounds, false);
  bounds.set_width(image.width());
  bounds.set_height(image.height());
  SetBounds(bounds);

  configured_snapshot_ = true;
}

}  // namespace chromeos
