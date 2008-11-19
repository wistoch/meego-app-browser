// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/toolbar_view.h"

#include <string>

#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/theme_helpers.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/container.h"
#include "chrome/views/button_dropdown.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/background.h"
#include "chrome/views/label.h"
#include "chrome/views/tooltip_manager.h"
#include "net/base/net_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

static const int kControlHorizOffset = 4;
static const int kControlVertOffset = 6;
static const int kControlVertOffsetLocationOnly = 4;
// The left and right margin of the toolbar in location-bar only mode.
static const int kToolbarHorizontalMargin = 1;
static const int kControlIndent = 3;
static const int kStatusBubbleWidth = 480;

// Separation between the location bar and the menus.
static const int kMenuButtonOffset = 3;

// Padding to the right of the location bar
static const int kPaddingRight = 2;

BrowserToolbarView::BrowserToolbarView(CommandController* controller,
                                       Browser* browser)
    : EncodingMenuControllerDelegate(browser, controller),
      controller_(controller),
      model_(browser->toolbar_model()),
      back_(NULL),
      forward_(NULL),
      reload_(NULL),
      home_(NULL),
      star_(NULL),
      location_bar_(NULL),
      go_(NULL),
      profile_(NULL),
      acc_focused_view_(NULL),
      browser_(browser),
      tab_(NULL) {
  back_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  forward_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));

  if (browser->type() == Browser::TYPE_NORMAL)
    display_mode_ = DISPLAYMODE_NORMAL;
  else
    display_mode_ = DISPLAYMODE_LOCATION;
}

BrowserToolbarView::~BrowserToolbarView() {
}

void BrowserToolbarView::Init(Profile* profile) {
  // Create all the individual Views in the Toolbar.
  CreateLeftSideControls();
  CreateCenterStack(profile);
  CreateRightSideControls(profile);

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void BrowserToolbarView::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  location_bar_->SetProfile(profile);
}

void BrowserToolbarView::CreateLeftSideControls() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  back_ = new views::ButtonDropDown(back_menu_model_.get());
  back_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_BACK));
  back_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_BACK_H));
  back_->SetImage(views::Button::BS_PUSHED, rb.GetBitmapNamed(IDR_BACK_P));
  back_->SetImage(views::Button::BS_DISABLED, rb.GetBitmapNamed(IDR_BACK_D));
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetID(VIEW_ID_BACK_BUTTON);
  AddChildView(back_);
  controller_->AddManagedButton(back_, IDC_BACK);

  forward_ = new views::ButtonDropDown(forward_menu_model_.get());
  forward_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_FORWARD));
  forward_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_FORWARD_H));
  forward_->SetImage(views::Button::BS_PUSHED,
                     rb.GetBitmapNamed(IDR_FORWARD_P));
  forward_->SetImage(views::Button::BS_DISABLED,
                     rb.GetBitmapNamed(IDR_FORWARD_D));
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetID(VIEW_ID_FORWARD_BUTTON);
  AddChildView(forward_);
  controller_->AddManagedButton(forward_, IDC_FORWARD);

  reload_ = new views::Button();
  reload_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::Button::BS_PUSHED, rb.GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);
  AddChildView(reload_);
  controller_->AddManagedButton(reload_, IDC_RELOAD);

  home_ = new views::Button();
  home_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_HOME));
  home_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_HOME_H));
  home_->SetImage(views::Button::BS_PUSHED, rb.GetBitmapNamed(IDR_HOME_P));
  home_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_HOME));
  AddChildView(home_);
  controller_->AddManagedButton(home_, IDC_HOME);
}

void BrowserToolbarView::CreateCenterStack(Profile *profile) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  star_ = new ToolbarStarToggle(this);
  star_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_STAR));
  star_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_STAR_H));
  star_->SetImage(views::Button::BS_PUSHED, rb.GetBitmapNamed(IDR_STAR_P));
  star_->SetImage(views::Button::BS_DISABLED, rb.GetBitmapNamed(IDR_STAR_D));
  star_->SetToggledImage(views::Button::BS_NORMAL,
                         rb.GetBitmapNamed(IDR_STARRED));
  star_->SetToggledImage(views::Button::BS_HOT,
                         rb.GetBitmapNamed(IDR_STARRED_H));
  star_->SetToggledImage(views::Button::BS_PUSHED,
                         rb.GetBitmapNamed(IDR_STARRED_P));
  star_->SetDragController(this);
  star_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_STAR));
  star_->SetToggledTooltipText(l10n_util::GetString(IDS_TOOLTIP_STARRED));
  star_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_STAR));
  star_->SetID(VIEW_ID_STAR_BUTTON);
  AddChildView(star_);
  controller_->AddManagedButton(star_, IDC_STAR);

  location_bar_ = new LocationBarView(profile, controller_, model_, this,
                                      display_mode_ == DISPLAYMODE_LOCATION);
  AddChildView(location_bar_);
  location_bar_->Init();

  // The Go button.
  go_ = new GoButton(location_bar_, controller_);
  go_->SetImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_GO));
  go_->SetImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_GO_H));
  go_->SetImage(views::Button::BS_PUSHED, rb.GetBitmapNamed(IDR_GO_P));
  go_->SetToggledImage(views::Button::BS_NORMAL, rb.GetBitmapNamed(IDR_STOP));
  go_->SetToggledImage(views::Button::BS_HOT, rb.GetBitmapNamed(IDR_STOP_H));
  go_->SetToggledImage(views::Button::BS_PUSHED,
                       rb.GetBitmapNamed(IDR_STOP_P));
  go_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_GO));
  go_->SetID(VIEW_ID_GO_BUTTON);
  AddChildView(go_);
}

void BrowserToolbarView::Update(TabContents* tab, bool should_restore_state) {
  tab_ = tab;

  if (!location_bar_)
    return;

  location_bar_->Update(should_restore_state ? tab : NULL);
}

void BrowserToolbarView::OnInputInProgress(bool in_progress) {
  // The edit should make sure we're only notified when something changes.
  DCHECK(model_->input_in_progress() != in_progress);

  model_->set_input_in_progress(in_progress);
  location_bar_->Update(NULL);
}

void BrowserToolbarView::CreateRightSideControls(Profile* profile) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  page_menu_ = new views::MenuButton(std::wstring(), this, false);

  // We use different menu button images if the locale is right-to-left.
  if (UILayoutIsRightToLeft())
    page_menu_->SetIcon(*rb.GetBitmapNamed(IDR_MENU_PAGE_RTL));
  else
    page_menu_->SetIcon(*rb.GetBitmapNamed(IDR_MENU_PAGE));

  page_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_PAGE));
  page_menu_->SetTooltipText(l10n_util::GetString(IDS_PAGEMENU_TOOLTIP));
  page_menu_->SetID(VIEW_ID_PAGE_MENU);
  AddChildView(page_menu_);

  app_menu_ = new views::MenuButton(std::wstring(), this, false);
  if (UILayoutIsRightToLeft())
    app_menu_->SetIcon(*rb.GetBitmapNamed(IDR_MENU_CHROME_RTL));
  else
    app_menu_->SetIcon(*rb.GetBitmapNamed(IDR_MENU_CHROME));

  app_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringF(IDS_APPMENU_TOOLTIP,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  app_menu_->SetID(VIEW_ID_APP_MENU);
  AddChildView(app_menu_);
}

void BrowserToolbarView::Layout() {
  gfx::Size sz;

  // If we have not been initialized yet just do nothing.
  if (back_ == NULL)
    return;

  int location_bar_y = kControlVertOffset;
  int location_bar_height = 0;
  // The width of all of the controls to the left of the location bar.
  int left_side_width = 0;
  // The width of all of the controls to the right of the location bar.
  int right_side_width = 0;
  if (IsDisplayModeNormal()) {
    sz = back_->GetPreferredSize();
    back_->SetBounds(kControlIndent, kControlVertOffset, sz.width(),
                     sz.height());

    sz = forward_->GetPreferredSize();
    forward_->SetBounds(back_->x() + back_->width(), kControlVertOffset,
                        sz.width(), sz.height());

    sz = reload_->GetPreferredSize();
    reload_->SetBounds(forward_->x() + forward_->width() +
                           kControlHorizOffset,
                       kControlVertOffset, sz.width(), sz.height());

    int offset = 0;
    if (show_home_button_.GetValue()) {
      sz = home_->GetPreferredSize();
      offset = kControlHorizOffset;
      home_->SetVisible(true);
    } else {
      sz = gfx::Size();
      home_->SetVisible(false);
    }
    home_->SetBounds(reload_->x() + reload_->width() + offset,
                     kControlVertOffset, sz.width(), sz.height());

    sz = star_->GetPreferredSize();
    star_->SetBounds(home_->x() + home_->width() + kControlHorizOffset,
                     kControlVertOffset, sz.width(), sz.height());

    sz = page_menu_->GetPreferredSize();
    right_side_width = sz.width() + kMenuButtonOffset;

    sz = app_menu_->GetPreferredSize();
    right_side_width += sz.width() + kPaddingRight;

    sz = go_->GetPreferredSize();
    location_bar_height = sz.height();
    right_side_width += sz.width();

    left_side_width = star_->x() + star_->width();
  } else {
    gfx::Size temp = location_bar_->GetPreferredSize();
    location_bar_height = temp.height();
    left_side_width = kToolbarHorizontalMargin;
    right_side_width = kToolbarHorizontalMargin;
    location_bar_y = kControlVertOffsetLocationOnly;
  }

  location_bar_->SetBounds(left_side_width, location_bar_y,
                           width() - left_side_width - right_side_width,
                           location_bar_height);

  if (IsDisplayModeNormal()) {
    go_->SetBounds(location_bar_->x() + location_bar_->width(),
                   kControlVertOffset, sz.width(), sz.height());

    // Make sure the Page menu never overlaps the location bar.
    int page_x = go_->x() + go_->width() + kMenuButtonOffset;
    sz = page_menu_->GetPreferredSize();
    page_menu_->SetBounds(page_x, kControlVertOffset, sz.width(),
                          go_->height());
    sz = app_menu_->GetPreferredSize();
    app_menu_->SetBounds(page_menu_->x() + page_menu_->width(),
                         page_menu_->y(), sz.width(), go_->height());
  }
}

void BrowserToolbarView::DidGainFocus() {
  // Find first accessible child (-1 for start search at parent).
  int first_acc_child = GetNextAccessibleViewIndex(-1, false);

  // No buttons enabled or visible.
  if (first_acc_child == -1)
    return;

  acc_focused_view_ = GetChildViewAt(first_acc_child);

  // Default focus is on the toolbar.
  int view_index = VIEW_ID_TOOLBAR;

  // Set hot-tracking for child, and update focused_view for MSAA focus event.
  if (acc_focused_view_) {
    acc_focused_view_->SetHotTracked(true);

    // Show the tooltip for the view that got the focus.
    if (GetContainer()->GetTooltipManager()) {
      GetContainer()->GetTooltipManager()->
          ShowKeyboardTooltip(acc_focused_view_);
    }

    // Update focused_view with MSAA-adjusted child id.
    view_index = acc_focused_view_->GetID();
  }

  HWND hwnd = GetContainer()->GetHWND();

  // Notify Access Technology that there was a change in keyboard focus.
  ::NotifyWinEvent(EVENT_OBJECT_FOCUS, hwnd, OBJID_CLIENT,
                   static_cast<LONG>(view_index));
}

void BrowserToolbarView::WillLoseFocus() {
  // Resetting focus state.
  acc_focused_view_->SetHotTracked(false);
  // Any tooltips that are active should be hidden when toolbar loses focus.
  if (GetContainer() && GetContainer()->GetTooltipManager())
    GetContainer()->GetTooltipManager()->HideKeyboardTooltip();
  acc_focused_view_ = NULL;
}

bool BrowserToolbarView::OnKeyPressed(const views::KeyEvent& e) {
  // Paranoia check, button should be initialized upon toolbar gaining focus.
  if (!acc_focused_view_)
    return false;

  int focused_view = GetChildIndex(acc_focused_view_);
  int next_view = focused_view;

  switch (e.GetCharacter()) {
    case VK_LEFT:
      next_view = GetNextAccessibleViewIndex(focused_view, true);
      break;
    case VK_RIGHT:
      next_view = GetNextAccessibleViewIndex(focused_view, false);
      break;
    case VK_DOWN:
    case VK_RETURN:
      // VK_SPACE is already handled by the default case.
      if (acc_focused_view_->GetID() == VIEW_ID_PAGE_MENU ||
          acc_focused_view_->GetID() == VIEW_ID_APP_MENU) {
        // If a menu button in toolbar is activated and its menu is displayed,
        // then active tooltip should be hidden.
        if (GetContainer()->GetTooltipManager())
          GetContainer()->GetTooltipManager()->HideKeyboardTooltip();
        // Safe to cast, given to above view id check.
        static_cast<views::MenuButton*>(acc_focused_view_)->Activate();
        if (!acc_focused_view_) {
          // Activate triggered a focus change, don't try to change focus.
          return true;
        }
        // Re-enable hot-tracking, as Activate() will disable it.
        acc_focused_view_->SetHotTracked(true);
        break;
      }
    default:
      // If key is not handled explicitly, pass it on to view.
      return acc_focused_view_->OnKeyPressed(e);
  }

  // No buttons enabled or visible.
  if (next_view == -1)
    return false;

  // Only send an event if focus moved.
  if (next_view != focused_view) {
    // Remove hot-tracking from old focused button.
    acc_focused_view_->SetHotTracked(false);

    // All is well, update the focused child member variable.
    acc_focused_view_ = GetChildViewAt(next_view);

    // Hot-track new focused button.
    acc_focused_view_->SetHotTracked(true);

    // Retrieve information to generate an MSAA focus event.
    int view_id = acc_focused_view_->GetID();
    HWND hwnd = GetContainer()->GetHWND();

    // Show the tooltip for the view that got the focus.
    if (GetContainer()->GetTooltipManager()) {
      GetContainer()->GetTooltipManager()->
          ShowKeyboardTooltip(GetChildViewAt(next_view));
    }
    // Notify Access Technology that there was a change in keyboard focus.
    ::NotifyWinEvent(EVENT_OBJECT_FOCUS, hwnd, OBJID_CLIENT,
                     static_cast<LONG>(view_id));
    return true;
  }
  return false;
}

bool BrowserToolbarView::OnKeyReleased(const views::KeyEvent& e) {
  // Paranoia check, button should be initialized upon toolbar gaining focus.
  if (!acc_focused_view_)
    return false;

  // Have keys be handled by the views themselves.
  return acc_focused_view_->OnKeyReleased(e);
}

gfx::Size BrowserToolbarView::GetPreferredSize() {
  if (IsDisplayModeNormal()) {
    static SkBitmap normal_background;
    if (normal_background.isNull()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      normal_background = *rb.GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
    }
    return gfx::Size(0, normal_background.height());
  }

  int locbar_height = location_bar_->GetPreferredSize().height();
  return gfx::Size(0, locbar_height + 2 * kControlVertOffsetLocationOnly);
}

void BrowserToolbarView::RunPageMenu(const CPoint& pt, HWND hwnd) {
  Menu::AnchorPoint anchor = Menu::TOPRIGHT;
  if (UILayoutIsRightToLeft())
    anchor = Menu::TOPLEFT;

  Menu menu(this, anchor, hwnd);
  // The install menu may be dynamically generated with a contextual label.
  // See browser_commands.cc.
  menu.AppendMenuItemWithLabel(IDC_CREATE_SHORTCUT,
      l10n_util::GetString(IDS_DEFAULT_INSTALL_SITE_LABEL));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_CUT, l10n_util::GetString(IDS_CUT));
  menu.AppendMenuItemWithLabel(IDC_COPY, l10n_util::GetString(IDS_COPY));
  menu.AppendMenuItemWithLabel(IDC_PASTE, l10n_util::GetString(IDS_PASTE));
  menu.AppendSeparator();

  menu.AppendMenuItemWithLabel(IDC_FIND,
                               l10n_util::GetString(IDS_FIND_IN_PAGE));
  menu.AppendMenuItemWithLabel(IDC_SAVEPAGE,
                               l10n_util::GetString(IDS_SAVEPAGEAS));
  menu.AppendMenuItemWithLabel(IDC_PRINT, l10n_util::GetString(IDS_PRINT));
  menu.AppendSeparator();

  Menu* zoom_menu = menu.AppendSubMenu(IDC_ZOOM,
                                       l10n_util::GetString(IDS_ZOOM));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_PLUS,
                                     l10n_util::GetString(IDS_ZOOM_PLUS));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_NORMAL,
                                     l10n_util::GetString(IDS_ZOOM_NORMAL));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_MINUS,
                                     l10n_util::GetString(IDS_ZOOM_MINUS));

  // Create encoding menu.
  Menu* encoding_menu = menu.AppendSubMenu(IDC_ENCODING,
                                           l10n_util::GetString(IDS_ENCODING));

  EncodingMenuControllerDelegate::BuildEncodingMenu(profile_, encoding_menu);

  struct MenuCreateMaterial {
    unsigned int menu_id;
    unsigned int menu_label_id;
  };
  struct MenuCreateMaterial developer_menu_materials[] = {
    { IDC_VIEWSOURCE, IDS_VIEWPAGESOURCE },
    { IDC_DEBUGGER, IDS_DEBUGGER },
    { IDC_SHOW_JS_CONSOLE, IDS_VIEWJSCONSOLE },
    { IDC_TASKMANAGER, IDS_TASKMANAGER }
  };
  // Append developer menu.
  menu.AppendSeparator();
  Menu* developer_menu =
    menu.AppendSubMenu(IDC_DEVELOPER, l10n_util::GetString(IDS_DEVELOPER));
  for (int i = 0; i < arraysize(developer_menu_materials); ++i) {
    if (developer_menu_materials[i].menu_id) {
      developer_menu->AppendMenuItemWithLabel(
          developer_menu_materials[i].menu_id,
          l10n_util::GetString(developer_menu_materials[i].menu_label_id));
    } else {
      developer_menu->AppendSeparator();
    }
  }

  menu.AppendSeparator();

  menu.AppendMenuItemWithLabel(IDS_COMMANDS_REPORTBUG,
                               l10n_util::GetString(IDS_COMMANDS_REPORTBUG));
  menu.RunMenuAt(pt.x, pt.y);
}

void BrowserToolbarView::RunAppMenu(const CPoint& pt, HWND hwnd) {
  Menu::AnchorPoint anchor = Menu::TOPRIGHT;
  if (UILayoutIsRightToLeft())
    anchor = Menu::TOPLEFT;

  Menu menu(this, anchor, hwnd);
  menu.AppendMenuItemWithLabel(IDC_NEWTAB, l10n_util::GetString(IDS_NEWTAB));
  menu.AppendMenuItemWithLabel(IDC_NEWWINDOW,
                               l10n_util::GetString(IDS_NEWWINDOW));
  menu.AppendMenuItemWithLabel(IDC_GOOFFTHERECORD,
                               l10n_util::GetString(IDS_GOOFFTHERECORD));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_SHOW_BOOKMARKS_BAR,
                               l10n_util::GetString(IDS_SHOW_BOOKMARK_BAR));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_SHOW_HISTORY,
                               l10n_util::GetString(IDS_SHOW_HISTORY));
  menu.AppendMenuItemWithLabel(IDC_SHOW_BOOKMARK_MANAGER,
                               l10n_util::GetString(IDS_BOOKMARK_MANAGER));
  menu.AppendMenuItemWithLabel(IDC_SHOW_DOWNLOADS,
                               l10n_util::GetString(IDS_SHOW_DOWNLOADS));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_CLEAR_BROWSING_DATA,
                               l10n_util::GetString(IDS_CLEAR_BROWSING_DATA));
  menu.AppendMenuItemWithLabel(IDC_IMPORT_SETTINGS,
                               l10n_util::GetString(IDS_IMPORT_SETTINGS));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_OPTIONS,
      l10n_util::GetStringF(IDS_OPTIONS,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  menu.AppendMenuItemWithLabel(IDC_ABOUT,
      l10n_util::GetStringF(IDS_ABOUT,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  menu.AppendMenuItemWithLabel(IDC_HELPMENU, l10n_util::GetString(IDS_HELP));
  menu.AppendSeparator();
  menu.AppendMenuItemWithLabel(IDC_EXIT, l10n_util::GetString(IDS_EXIT));

  menu.RunMenuAt(pt.x, pt.y);
}

bool BrowserToolbarView::IsItemChecked(int id) const {
  if (!profile_)
    return false;
  if (id == IDC_SHOW_BOOKMARKS_BAR)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  else
    return EncodingMenuControllerDelegate::IsItemChecked(id);
}

void BrowserToolbarView::RunMenu(views::View* source, const CPoint& pt,
                                 HWND hwnd) {
  switch (source->GetID()) {
    case VIEW_ID_PAGE_MENU:
      RunPageMenu(pt, hwnd);
      break;
    case VIEW_ID_APP_MENU:
      RunAppMenu(pt, hwnd);
      break;
    default:
      NOTREACHED() << "Invalid source menu.";
  }
}

bool BrowserToolbarView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_TOOLBAR;
  return true;
}

bool BrowserToolbarView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    (*name).assign(accessible_name_);
    return true;
  }
  return false;
}

void BrowserToolbarView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

int BrowserToolbarView::GetNextAccessibleViewIndex(int view_index,
                                                   bool nav_left) {
  int modifier = 1;

  if (nav_left)
    modifier = -1;

  int current_view_index = view_index + modifier;

  while ((current_view_index >= 0) &&
         (current_view_index < GetChildViewCount())) {
    // Skip the location bar, as it has its own keyboard navigation. Also skip
    // any views that cannot be interacted with.
    if (current_view_index == GetChildIndex(location_bar_) ||
        !GetChildViewAt(current_view_index)->IsEnabled() ||
        !GetChildViewAt(current_view_index)->IsVisible()) {
      current_view_index += modifier;
      continue;
    }
    // Update view_index with the available button index found.
    view_index = current_view_index;
    break;
  }
  // Returns the next available button index, or if no button is available in
  // the specified direction, remains where it was.
  return view_index;
}

void BrowserToolbarView::ShowContextMenu(int x, int y, bool is_mouse_gesture) {
  if (GetAccFocusedChildView())
    GetAccFocusedChildView()->ShowContextMenu(x, y, is_mouse_gesture);
}

int BrowserToolbarView::GetDragOperations(views::View* sender, int x, int y) {
  DCHECK(sender == star_);
  if (model_->input_in_progress() || !tab_ || !tab_->ShouldDisplayURL() ||
      !tab_->GetURL().is_valid()) {
    return DragDropTypes::DRAG_NONE;
  }
  if (profile_ && profile_->GetBookmarkModel() &&
      profile_->GetBookmarkModel()->IsBookmarked(tab_->GetURL())) {
    return DragDropTypes::DRAG_MOVE | DragDropTypes::DRAG_COPY |
           DragDropTypes::DRAG_LINK;
  }
  return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
}

void BrowserToolbarView::WriteDragData(views::View* sender,
                                       int press_x,
                                       int press_y,
                                       OSExchangeData* data) {
  DCHECK(
      GetDragOperations(sender, press_x, press_y) != DragDropTypes::DRAG_NONE);

  UserMetrics::RecordAction(L"Toolbar_DragStar", profile_);

  // If there is a bookmark for the URL, add the bookmark drag data for it. We
  // do this to ensure the bookmark is moved, rather than creating an new
  // bookmark.
  if (profile_ && profile_->GetBookmarkModel()) {
    BookmarkNode* node = profile_->GetBookmarkModel()->
        GetMostRecentlyAddedNodeForURL(tab_->GetURL());
    if (node) {
      BookmarkDragData bookmark_data(node);
      bookmark_data.Write(profile_, data);
    }
  }

  drag_utils::SetURLAndDragImage(tab_->GetURL(), tab_->GetTitle(),
                                 tab_->GetFavIcon(), data);
}

TabContents* BrowserToolbarView::GetTabContents() {
  return tab_;
}

void BrowserToolbarView::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NOTIFY_PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      Layout();
      SchedulePaint();
    }
  }
}

bool BrowserToolbarView::GetAcceleratorInfo(int id,
                                            views::Accelerator* accel) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (id) {
    case IDC_CUT:
      *accel = views::Accelerator(L'X', false, true, false);
      return true;
    case IDC_COPY:
      *accel = views::Accelerator(L'C', false, true, false);
      return true;
    case IDC_PASTE:
      *accel = views::Accelerator(L'V', false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetContainer()->GetAccelerator(id, accel);
}

