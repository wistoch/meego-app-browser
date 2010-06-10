// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_location_bar_view.h"

#include <gtk/gtk.h>
#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "gfx/canvas.h"
#include "gfx/point.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {
const int kDefaultLocationEntryWidth = 250;
const int kCompactLocationLeftMargin = 5;
const int kCompactLocationRightMargin = 10;
const int kEntryLeftMargin = 2;
// TODO(oshima): ToolbarView gets this from background image's height;
// Find out the right way, value for compact location bar.
const int kDefaultLocationBarHeight = 34;
const int kWidgetsSeparatorWidth = 2;

CompactLocationBarView::CompactLocationBarView(CompactLocationBarHost* host)
    : DropdownBarView(host),
      reload_(NULL),
      browser_actions_(NULL) {
  SetFocusable(true);
}

CompactLocationBarView::~CompactLocationBarView() {
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView public:

void CompactLocationBarView::SetFocusAndSelection(bool select_all) {
  location_entry_->SetFocus();
  if (select_all)
    location_entry_->SelectAll(true);
}

void CompactLocationBarView::Update(const TabContents* contents) {
  location_entry_->Update(contents);
  browser_actions_->RefreshBrowserActionViews();
}


////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView private:

Browser* CompactLocationBarView::browser() const {
  return host()->browser_view()->browser();
}

void CompactLocationBarView::Init() {
  ThemeProvider* tp = browser()->profile()->GetThemeProvider();
  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(IDR_THEME_BUTTON_BACKGROUND);

  // Reload button.
  reload_ = new views::ImageButton(this);
  reload_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                             views::ImageButton::ALIGN_MIDDLE);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);

  reload_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_RELOAD_MASK));

  AddChildView(reload_);

  // Location bar.
  location_entry_.reset(new AutocompleteEditViewGtk(
      this, browser()->toolbar_model(), browser()->profile(),
      browser()->command_updater(), false, this));

  location_entry_->Init();
  location_entry_->Update(browser()->GetSelectedTabContents());
  gtk_widget_show_all(location_entry_->GetNativeView());
  gtk_widget_hide(location_entry_->GetNativeView());

  location_entry_view_ = new views::NativeViewHost;
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->GetNativeView());

  location_entry_->Update(browser()->GetSelectedTabContents());

  // Note: we tell the BrowserActionsContainer not to save its size because
  // the main container is part of the ToolbarView, and we don't want them
  // fighting. See http://code.google.com/p/chromium/issues/detail?id=38992
  browser_actions_ = new BrowserActionsContainer(browser(), this, false);
  AddChildView(browser_actions_);
}

////////////////////////////////////////////////////////////////////////////////
// views::View overrides:

gfx::Size CompactLocationBarView::GetPreferredSize() {
  if (!reload_)
    return gfx::Size();  // Not initialized yet, do nothing.

  gfx::Size reload_size = reload_->GetPreferredSize();
  gfx::Size location_size = location_entry_view_->GetPreferredSize();
  gfx::Size ba_size = browser_actions_->GetPreferredSize();
  int width = kCompactLocationLeftMargin + reload_size.width() +
      std::max(kDefaultLocationEntryWidth,
               location_entry_view_->GetPreferredSize().width()) +
      ba_size.width() +
      kCompactLocationRightMargin;
  return gfx::Size(width, kDefaultLocationBarHeight);
}

void CompactLocationBarView::Layout() {
  if (!reload_)
    return;  // Not initialized yet, do nothing.

  int cur_x = kCompactLocationLeftMargin;

  gfx::Size reload_size = reload_->GetPreferredSize();
  int reload_y = (height() - reload_size.height()) / 2;
  reload_->SetBounds(cur_x, reload_y,
                     reload_size.width(), reload_size.height());
  cur_x += reload_size.width();

  gfx::Size ba_size = browser_actions_->GetPreferredSize();
  int ba_y = (height() - ba_size.height()) / 2;
  browser_actions_->SetBounds(
      width() - ba_size.width(), ba_y, ba_size.width(), ba_size.height());
  int location_entry_width = browser_actions_->x() - cur_x;
  if (ba_size.IsEmpty()) {
    // BrowserActionsContainer has its own margin on right.
    // Use the our margin when if the browser action is empty.
    location_entry_width -= kCompactLocationRightMargin;
  }

  // The location bar gets the rest of the space in the middle.
  location_entry_view_->SetBounds(cur_x, 0, location_entry_width, height());
}

void CompactLocationBarView::Paint(gfx::Canvas* canvas) {
  gfx::Rect lb = GetLocalBounds(true);
  ThemeProvider* tp = GetThemeProvider();
  gfx::Rect bounds;
  host()->GetThemePosition(&bounds);
  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR),
                       bounds.x(), bounds.y(), 0, 0, lb.width(), lb.height());
  View::Paint(canvas);
}

void CompactLocationBarView::ViewHierarchyChanged(bool is_add, View* parent,
                                              View* child) {
  if (is_add && child == this)
    Init();
}

void CompactLocationBarView::Focus() {
  location_entry_->SetFocus();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void CompactLocationBarView::ButtonPressed(views::Button* sender,
                                           const views::Event& event) {
  int id = sender->tag();
  int flags = sender->mouse_event_flags();
  // Shift-clicking or ctrl-clicking the reload button means we should
  // ignore any cached content.
  // TODO(avayvod): eliminate duplication of this logic in
  // CompactLocationBarView.
  if (id == IDC_RELOAD && (event.IsShiftDown() || event.IsControlDown())) {
    id = IDC_RELOAD_IGNORING_CACHE;
    // Mask off shift/ctrl so they aren't interpreted as affecting the
    // disposition below.
    flags &= ~(views::Event::EF_SHIFT_DOWN | views::Event::EF_CONTROL_DOWN);
  }
  browser()->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(flags));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditController overrides:

void CompactLocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  browser()->OpenURL(url, GURL(), disposition, transition);
  clb_host()->StartAutoHideTimer();
}

void CompactLocationBarView::OnChanged() {
  // Other one does "DoLayout" here.
}

void CompactLocationBarView::OnKillFocus() {
  host()->UnregisterAccelerators();
}

void CompactLocationBarView::OnSetFocus() {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
  host()->RegisterAccelerators();
}

void CompactLocationBarView::OnInputInProgress(bool in_progress) {
}

SkBitmap CompactLocationBarView::GetFavIcon() const {
  return SkBitmap();
}

std::wstring CompactLocationBarView::GetTitle() const {
  return std::wstring();
}

}  // namespace chromeos
