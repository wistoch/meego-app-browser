// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
#pragma once

#include <vector>

#include "app/menus/accelerator.h"
#include "app/slide_animation.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/views/accessible_toolbar_view.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "chrome/browser/views/reload_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/menu_wrapper.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class BrowserActionsContainer;
class Browser;
class Profile;
class WrenchMenu;

// The Browser Window's toolbar.
class ToolbarView : public AccessibleToolbarView,
                    public views::ViewMenuDelegate,
                    public menus::AcceleratorProvider,
                    public LocationBarView::Delegate,
                    public AnimationDelegate,
                    public NotificationObserver,
                    public CommandUpdater::CommandObserver,
                    public views::ButtonListener {
 public:
  explicit ToolbarView(Browser* browser);
  virtual ~ToolbarView();

  // Create the contents of the Browser Toolbar
  void Init(Profile* profile);

  // Sets the profile which is active on the currently-active tab.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  // Updates the toolbar (and transitively the location bar) with the states of
  // the specified |tab|.  If |should_restore_state| is true, we're switching
  // (back?) to this tab and should restore any previous location bar state
  // (such as user editing) as well.
  void Update(TabContents* tab, bool should_restore_state);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the location bar. Focus will be restored
  // to the ViewStorage with id |view_storage_id| if the user escapes.
  void SetToolbarFocusAndFocusLocationBar(int view_storage_id);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the ViewStorage with id |view_storage_id| if the user escapes.
  void SetToolbarFocusAndFocusAppMenu(int view_storage_id);

  // Add a listener to receive a callback when the menu opens.
  void AddMenuListener(views::MenuListener* listener);

  // Remove a menu listener.
  void RemoveMenuListener(views::MenuListener* listener);

  // Accessors...
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  views::MenuButton* app_menu() const { return app_menu_; }

  // Overridden from AccessibleToolbarView
  virtual bool SetToolbarFocus(int view_storage_id, View* initial_focus);

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, menus::Accelerator* accel);

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from LocationBarView::Delegate:
  virtual TabContents* GetTabContents();
  virtual MatchPreview* GetMatchPreview();
  virtual void OnInputInProgress(bool in_progress);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from menus::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void OnThemeChanged();

  // The apparent horizontal space between most items, and the vertical padding
  // above and below them.
  static const int kStandardSpacing;
  // The top of the toolbar has an edge we have to skip over in addition to the
  // standard spacing.
  static const int kVertSpacing;

 protected:

  // Overridden from AccessibleToolbarView
  virtual views::View* GetDefaultFocusableChild();
  virtual void RemoveToolbarFocus();

 private:
  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

  // Loads the images for all the child views.
  void LoadImages();

  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };
  bool IsDisplayModeNormal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // Starts the recurring timer that periodically asks the upgrade notifier
  // to pulsate.
  void ShowUpgradeReminder();

  // Show the reminder, tempting the user to upgrade by pulsating.
  void PulsateUpgradeNotifier();

  // Gets a canvas with the icon for the app menu. It will possibly contain
  // an overlaid badge if an update is recommended.
  SkBitmap GetAppMenuIcon(views::CustomButton::ButtonState state);

  scoped_ptr<BackForwardMenuModel> back_menu_model_;
  scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  ReloadButton* reload_;
  views::ImageButton* home_;
  LocationBarView* location_bar_;
  BrowserActionsContainer* browser_actions_;
  views::MenuButton* app_menu_;
  Profile* profile_;
  Browser* browser_;

  // Contents of the profiles menu to populate with profile names.
  scoped_ptr<menus::SimpleMenuModel> profiles_menu_contents_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;

  // The contents of the wrench menu.
  scoped_ptr<menus::SimpleMenuModel> wrench_menu_model_;

  // Wrench menu.
  scoped_ptr<WrenchMenu> wrench_menu_;

  // Vector of listeners to receive callbacks when the menu opens.
  std::vector<views::MenuListener*> menu_listeners_;

  // The animation that makes the update reminder pulse.
  scoped_ptr<SlideAnimation> update_reminder_animation_;

  // We periodically restart the animation after it has been showed
  // once, to create a pulsating effect.
  base::RepeatingTimer<ToolbarView> upgrade_reminder_pulse_timer_;

  // Used to post tasks to switch to the next/previous menu.
  ScopedRunnableMethodFactory<ToolbarView> method_factory_;

  NotificationRegistrar registrar_;

  // If non-null the destructor sets this to true. This is set to a non-null
  // while the menu is showing and used to detect if the menu was deleted while
  // running.
  bool* destroyed_flag_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
