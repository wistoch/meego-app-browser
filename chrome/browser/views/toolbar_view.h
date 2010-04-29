// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_

#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/page_menu_model.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/views/accessible_toolbar_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/menu_wrapper.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"

class BrowserActionsContainer;
class Browser;
class Profile;

namespace views {
class Menu2;
}

// The Browser Window's toolbar.
class ToolbarView : public AccessibleToolbarView,
                    public views::ViewMenuDelegate,
                    public views::FocusChangeListener,
                    public menus::SimpleMenuModel::Delegate,
                    public LocationBarView::Delegate,
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

  // Sets the app menu model.
  void SetAppMenuModel(AppMenuModel* model);

  // Focuses the page menu and enters a special mode where the page
  // and app menus are focusable and allow for keyboard navigation just
  // like a normal menu bar. As soon as focus leaves one of the menus,
  // the special mode is exited.
  //
  // Pass it the storage id of the view where focus should be returned
  // if the user escapes, and the menu button to focus initially. If
  // |menu_to_focus| is NULL, it will focus the page menu by default.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  void EnterMenuBarEmulationMode(int last_focused_view_storage_id,
                                 views::MenuButton* menu_to_focus);

  // Add a listener to receive a callback when the menu opens.
  void AddMenuListener(views::MenuListener* listener);

  // Remove a menu listener.
  void RemoveMenuListener(views::MenuListener* listener);

  // Accessors...
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  GoButton* go_button() const { return go_; }
  LocationBarView* location_bar() const { return location_bar_; }
  views::MenuButton* page_menu() const { return page_menu_; }
  views::MenuButton* app_menu() const { return app_menu_; }

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // Overridden from AccessibleToolbarView:
  virtual bool IsAccessibleViewTraversable(views::View* view);

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, menus::Accelerator* accel);

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from LocationBarView::Delegate:
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ThemeChanged();

 private:
  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

  // Set up the various Views in the toolbar
  void CreateLeftSideControls();
  void CreateCenterStack(Profile* profile);
  void CreateRightSideControls(Profile* profile);
  void LoadLeftSideControlsImages();
  void LoadCenterStackImages();
  void LoadRightSideControlsImages();

  // Runs various menus.
  void RunPageMenu(const gfx::Point& pt);
  void RunAppMenu(const gfx::Point& pt);

  // Check if the menu exited with a code indicating the user wants to
  // switch to the other menu, and then switch to that other menu.
  void SwitchToOtherMenuIfNeeded(views::Menu2* previous_menu,
                                 views::MenuButton* next_menu_button);

  void ActivateMenuButton(views::MenuButton* menu_button);

  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };
  bool IsDisplayModeNormal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // Take the menus out of the focus traversal, unregister accelerators,
  // and stop listening to focus change events.
  void ExitMenuBarEmulationMode();

  // Restore the view that was focused before EnterMenuBarEmulationMode
  // was called.
  void RestoreLastFocusedView();

  scoped_ptr<BackForwardMenuModel> back_menu_model_;
  scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  views::ImageButton* home_;
  views::ImageButton* reload_;
  LocationBarView* location_bar_;
  GoButton* go_;
  BrowserActionsContainer* browser_actions_;
  views::MenuButton* page_menu_;
  views::MenuButton* app_menu_;
  // The bookmark menu button. This may be null.
  views::MenuButton* bookmark_menu_;
  Profile* profile_;
  Browser* browser_;

  // Contents of the profiles menu to populate with profile names.
  scoped_ptr<menus::SimpleMenuModel> profiles_menu_contents_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;

  // The contents of the various menus.
  scoped_ptr<PageMenuModel> page_menu_model_;
  scoped_ptr<AppMenuModel> app_menu_model_;

  // TODO(beng): build these into MenuButton.
  scoped_ptr<views::Menu2> page_menu_menu_;
  scoped_ptr<views::Menu2> app_menu_menu_;

  // Save the focus manager rather than calling GetFocusManager(),
  // so that we can remove focus listeners in the destructor.
  views::FocusManager* focus_manager_;

  // Storage id for the last view that was focused before focus
  // was given to one of the toolbar views.
  int last_focused_view_storage_id_;

  // Vector of listeners to receive callbacks when the menu opens.
  std::vector<views::MenuListener*> menu_listeners_;

  // Are we in the menu bar emulation mode, where the app and page menu
  // are temporarily focusable?
  bool menu_bar_emulation_mode_;

  // Used to post tasks to switch to the next/previous menu.
  ScopedRunnableMethodFactory<ToolbarView> method_factory_;

  // If non-null the destuctor sets this to true. This is set to a non-null
  // while the menu is showing and used to detect if the menu was deleted while
  // running.
  bool* destroyed_flag_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
