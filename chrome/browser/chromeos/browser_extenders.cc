// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "app/theme_provider.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/compact_location_bar.h"
#include "chrome/browser/chromeos/compact_navigation_bar.h"
#include "chrome/browser/chromeos/main_menu.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/chromeos/panel_controller.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/tabs/tab_strip_wrapper.h"
#include "chrome/browser/views/toolbar_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/simple_menu_model.h"
#include "views/window/window.h"

namespace {

// NormalExtender adds ChromeOS specific controls and menus to a BrowserView
// created with Browser::TYPE_NORMAL. This extender adds controls to
// the title bar as follows:
//                  ____  __ __
//      [MainMenu] /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
//
// For Browser::TYPE_POPUP type of BrowserView, see PopupExtender class below.
class NormalExtender : public BrowserExtender,
                       public views::ButtonListener,
                       public views::ContextMenuController {
 public:
  explicit NormalExtender(BrowserView* browser_view)
      : BrowserExtender(browser_view),
        main_menu_(NULL),
        status_area_(NULL),
        compact_navigation_bar_(NULL),
        // CompactNavigationBar is disabled by default.
        // TODO(oshima): Get this info from preference.
        compact_navigation_bar_enabled_(false) {
  }
  virtual ~NormalExtender() {}

 protected:
  // BrowserExtender overrides.
  virtual void Init() {
    main_menu_ = new views::ImageButton(this);
    ThemeProvider* theme_provider =
        browser_view()->frame()->GetThemeProviderForFrame();
    SkBitmap* image = theme_provider->GetBitmapNamed(IDR_MAIN_MENU_BUTTON);
    main_menu_->SetImage(views::CustomButton::BS_NORMAL, image);
    main_menu_->SetImage(views::CustomButton::BS_HOT, image);
    main_menu_->SetImage(views::CustomButton::BS_PUSHED, image);
    browser_view()->AddChildView(main_menu_);

    compact_location_bar_.reset(new chromeos::CompactLocationBar(browser_view()));
    compact_navigation_bar_ =
        new chromeos::CompactNavigationBar(browser_view()->browser());
    browser_view()->AddChildView(compact_navigation_bar_);
    compact_navigation_bar_->Init();
    status_area_ = new chromeos::StatusAreaView(
        browser_view()->browser(),
        browser_view()->GetWindow()->GetNativeWindow());
    browser_view()->AddChildView(status_area_);
    status_area_->Init();

    InitSystemMenu();
    chromeos::MainMenu::ScheduleCreation();

    // The ContextMenuController has to be set to a NonClientView but
    // not to a NonClientFrameView because a TabStrip is not a child of
    // a NonClientFrameView even though visually a TabStrip is over a
    // NonClientFrameView.
    BrowserFrameGtk* gtk_frame =
        static_cast<BrowserFrameGtk*>(browser_view()->frame());
    gtk_frame->GetNonClientView()->SetContextMenuController(this);
  }

  virtual gfx::Rect Layout(const gfx::Rect& bounds) {
    // Skip if there is no space to layout, or if the browser is in
    // fullscreen mode.
    if (bounds.IsEmpty() || browser_view()->IsFullscreen()) {
      main_menu_->SetVisible(false);
      compact_navigation_bar_->SetVisible(false);
      status_area_->SetVisible(false);
      return bounds;
    } else {
      main_menu_->SetVisible(true);
      compact_navigation_bar_->SetVisible(compact_navigation_bar_enabled_);
      status_area_->SetVisible(true);
    }

    if (compact_navigation_bar_->IsVisible()) {
      // Update the size and location of the compact location bar.
      compact_location_bar_->UpdateBounds(
          browser_view()->tabstrip()->AsTabStrip()->GetSelectedTab());
    }

    // Layout main menu before tab strip.
    gfx::Size main_menu_size = main_menu_->GetPreferredSize();
    main_menu_->SetBounds(bounds.x(), bounds.y(),
                          main_menu_size.width(), bounds.height());

    // Layout status area after tab strip.
    gfx::Size status_size = status_area_->GetPreferredSize();
    status_area_->SetBounds(bounds.x() + bounds.width() - status_size.width(),
                            bounds.y(), status_size.width(),
                            status_size.height());
    int curx = bounds.x() + main_menu_size.width();
    int width = bounds.width() - main_menu_size.width() - status_size.width();

    if (compact_navigation_bar_->IsVisible()) {
      gfx::Size cnb_bounds = compact_navigation_bar_->GetPreferredSize();
      compact_navigation_bar_->SetBounds(curx, bounds.y(),
                                         cnb_bounds.width(), bounds.height());
      curx += cnb_bounds.width();
      width -= cnb_bounds.width();
    }
    width = std::max(0, width);  // In case there is no space left.
    return gfx::Rect(curx, bounds.y(), width, bounds.height());
  }

  virtual bool NonClientHitTest(const gfx::Point& point) {
    gfx::Point point_in_main_menu_coords(point);
    views::View::ConvertPointToView(browser_view(), main_menu_,
                                    &point_in_main_menu_coords);
    if (main_menu_->HitTest(point_in_main_menu_coords))
      return true;

    gfx::Point point_in_status_area_coords(point);
    views::View::ConvertPointToView(browser_view(), status_area_,
                                    &point_in_status_area_coords);
    if (status_area_->HitTest(point_in_status_area_coords))
      return true;

    if (compact_navigation_bar_->IsVisible()) {
      gfx::Point point_in_cnb_coords(point);
      views::View::ConvertPointToView(browser_view(),
                                      compact_navigation_bar_,
                                      &point_in_cnb_coords);
      return compact_navigation_bar_->HitTest(point_in_cnb_coords);
    }
    return false;
  }

  virtual void UpdateTitleBar() {}

  virtual void Show() {
    TabOverviewTypes::instance()->SetWindowType(
        GTK_WIDGET(GetBrowserWindow()->GetNativeWindow()),
        TabOverviewTypes::WINDOW_TYPE_CHROME_TOPLEVEL,
        NULL);
  }

  virtual void Close() {}

  virtual void ActivationChanged() {}

  virtual bool ShouldForceHideToolbar() {
    return compact_navigation_bar_enabled_;
  }

  virtual void ToggleCompactNavigationBar() {
    compact_navigation_bar_enabled_ = !compact_navigation_bar_enabled_;
  }

  virtual void OnMouseEnteredToTab(Tab* tab) {
    ShowCompactLocationBarUnderSelectedTab();
  }

  virtual void OnMouseMovedOnTab(Tab* tab) {
    ShowCompactLocationBarUnderSelectedTab();
  }

  virtual void OnMouseExitedFromTab(Tab* tab) {
    compact_location_bar_->StartPopupTimer();
  }

 private:
  // Shows the compact location bar under the selected tab.
  void ShowCompactLocationBarUnderSelectedTab() {
    if (!compact_navigation_bar_enabled_)
      return;
    compact_location_bar_->Update(
        browser_view()->tabstrip()->AsTabStrip()->GetSelectedTab(),
        browser_view()->browser()->GetSelectedTabContents());
  }

  // Creates system menu.
  void InitSystemMenu() {
    system_menu_contents_.reset(new views::SimpleMenuModel(browser_view()));
    system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB,
                                               IDS_RESTORE_TAB);
    system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
    system_menu_contents_->AddSeparator();
    system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                               IDS_TASK_MANAGER);
    system_menu_menu_.reset(new views::Menu2(system_menu_contents_.get()));
  }

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    chromeos::MainMenu::Show(browser_view()->browser());
  }

  // views::ContextMenuController overrides.
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture) {
    system_menu_menu_->RunMenuAt(gfx::Point(x, y), views::Menu2::ALIGN_TOPLEFT);
  }

  // Main menu button.
  views::ImageButton* main_menu_;

  // Status Area view.
  chromeos::StatusAreaView* status_area_;

  // System menus.
  scoped_ptr<views::SimpleMenuModel> system_menu_contents_;
  scoped_ptr<views::Menu2> system_menu_menu_;

  // CompactNavigationBar view.
  chromeos::CompactNavigationBar* compact_navigation_bar_;

  // A toggle flag to show/hide the compact navigation bar.
  bool compact_navigation_bar_enabled_;

  // CompactLocationBar view.
  scoped_ptr<chromeos::CompactLocationBar> compact_location_bar_;

  DISALLOW_COPY_AND_ASSIGN(NormalExtender);
};

// PopupExtender class creates dedicated title window for popup window.
// The size and location of the created title window is controlled by
// by window manager.
class PopupExtender : public BrowserExtender {
 public:
  explicit PopupExtender(BrowserView* browser_view)
      : BrowserExtender(browser_view) {
  }
  virtual ~PopupExtender() {}

 private:
  // BrowserExtender overrides.
  virtual void Init() {
    // The visibility of toolbar is controlled in
    // the BrowserView::IsToolbarVisible method.

    views::Window* window = GetBrowserWindow();
    gfx::NativeWindow native_window = window->GetNativeWindow();
    // The window manager needs the min size for popups.
    gfx::Rect bounds = window->GetBounds();
    gtk_widget_set_size_request(
        GTK_WIDGET(native_window), bounds.width(), bounds.height());
    // If we don't explicitly resize here there is a race condition between
    // the X Server and the window manager. Windows will appear with a default
    // size of 200x200 if this happens.
    gtk_window_resize(native_window, bounds.width(), bounds.height());
  }

  virtual gfx::Rect Layout(const gfx::Rect& bounds) {
    return bounds;
  }

  virtual bool NonClientHitTest(const gfx::Point& point) {
    return false;
  }

  virtual void Show() {
    panel_controller_.reset(new chromeos::PanelController(browser_view()));
  }

  virtual void Close() {
    if (panel_controller_.get())
      panel_controller_->Close();
  }

  virtual void UpdateTitleBar() {
    if (panel_controller_.get())
      panel_controller_->UpdateTitleBar();
  }

  virtual void ActivationChanged() {
    if (panel_controller_.get()) {
      if (GetBrowserWindow()->IsActive())
        panel_controller_->OnFocusIn();
      else
        panel_controller_->OnFocusOut();
    }
  }

  virtual bool ShouldForceHideToolbar() {
    // Always hide toolbar for popups.
    return true;
  }

  virtual void ToggleCompactNavigationBar() {}

  virtual void OnMouseEnteredToTab(Tab* tab) {}

  virtual void OnMouseMovedOnTab(Tab* tab) {}

  virtual void OnMouseExitedFromTab(Tab* tab) {}

  // Controls interactions with the window manager for popup panels.
  scoped_ptr<chromeos::PanelController> panel_controller_;

  DISALLOW_COPY_AND_ASSIGN(PopupExtender);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

// static
BrowserExtender* BrowserExtender::Create(BrowserView* browser_view) {
  BrowserExtender* extender;
  if (browser_view->browser()->type() & Browser::TYPE_POPUP)
    extender = new PopupExtender(browser_view);
  else
    extender = new NormalExtender(browser_view);
  extender->Init();
  return extender;
}
