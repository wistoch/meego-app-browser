// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/controls/button/button.h"

class TabStripModel;

namespace menus {
class SimpleMenuModel;
}  // namespace menus

namespace views {
class ImageButton;
class Menu2;
}  // namespace views

class Profile;

namespace chromeos {

class BrowserStatusAreaView;
class CompactLocationBar;
class CompactLocationBarHost;
class CompactNavigationBar;
class StatusAreaButton;

// chromeos::BrowserView adds ChromeOS specific controls and menus to a
// BrowserView created with Browser::TYPE_NORMAL. This extender adds controls
// to the title bar as follows:
//                     ____  __ __
//      [AppLauncher] /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
class BrowserView : public ::BrowserView,
                    public views::ButtonListener,
                    public views::ContextMenuController,
                    public StatusAreaHost {
 public:
  // There are 3 ui styles, standard, compact and sidebar.
  // Standard uses the same layout as chromium/chrome browser.
  // Compact mode hides the omnibox/toolbar to save the vertical real estate,
  // and uses QSB (compact nav bar) to launch/switch url. In sidebar mode,
  // the tabstrip is moved to the side and the omnibox is moved on top of
  // the tabstrip.
  enum UIStyle {
    StandardStyle = 0,
    CompactStyle,
    SidebarStyle,
  };

  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  // BrowserView overrides.
  virtual void Init();
  virtual void Show();
  virtual bool IsToolbarVisible() const;
  virtual void SetFocusToLocationBar();
  virtual void ToggleCompactNavigationBar();
  virtual views::LayoutManager* CreateLayoutManager() const;
  virtual BaseTabStrip* CreateTabStrip(TabStripModel* tab_strip_model);
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void SetStarredState(bool is_starred);
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::ContextMenuController overrides.
  virtual void ShowContextMenu(views::View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // StatusAreaHost overrides.
  virtual Profile* GetProfile() const;
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;

  // Shows the compact location bar under the selected tab.
  void ShowCompactLocationBarUnderSelectedTab();

  // Returns true if the ui style is in Compact mode.
  bool is_compact_style() const {
    return ui_style_ == CompactStyle;
  }

 private:
  friend class CompactLocationBarHostTest;

  CompactLocationBarHost* compact_location_bar_host() {
    return compact_location_bar_host_.get();
  }

  void InitSystemMenu();

  // AppLauncher button.
  views::ImageButton* main_menu_button_;

  // Status Area view.
  BrowserStatusAreaView* status_area_;

  // System menus.
  scoped_ptr<menus::SimpleMenuModel> system_menu_contents_;
  scoped_ptr<views::Menu2> system_menu_menu_;

  // CompactNavigationBar view.
  CompactNavigationBar* compact_navigation_bar_;

  // The current UI style of the browser.
  UIStyle ui_style_;

  // CompactLocationBarHost.
  scoped_ptr<CompactLocationBarHost> compact_location_bar_host_;

  // A flag to specify if the browser window should be maximized.
  bool force_maximized_window_;

  // A spacer under the tap strip used when the compact navigation bar
  // is active.
  views::View* spacer_;

  // Off the record icon.
  views::ImageView* otr_avatar_icon_;

  // Menu button shown in status area when browser is in compact mode.
  StatusAreaButton* menu_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
