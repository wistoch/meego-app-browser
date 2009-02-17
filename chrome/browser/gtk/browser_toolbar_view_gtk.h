// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_TOOLBAR_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_TOOLBAR_VIEW_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/pref_member.h"

class Browser;
class Profile;
class ToolbarModel;

// View class that displays the GTK version of the toolbar and routes gtk
// events back to the Browser.
class BrowserToolbarGtk : public CommandUpdater::CommandObserver,
                          public MenuGtk::Delegate {
 public:
  // Height of the toolbar, in pixels.
  static const int kToolbarHeight;

  explicit BrowserToolbarGtk(Browser* browser);
  virtual ~BrowserToolbarGtk();

  // Create the contents of the toolbar
  void Init(Profile* profile);

  // Adds this GTK toolbar into a sizing box.
  void AddToolbarToBox(GtkWidget* box);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from MenuGtk::Delegate:
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int command_id);

  void SetProfile(Profile* profile);

 private:
  class CustomDrawButton;  // Defined in the .cc file.

  // Builds a toolbar button with all the properties set.
  CustomDrawButton* BuildToolbarButton(int normal_id,
                                       int active_id,
                                       int highlight_id,
                                       int depressed_id,
                                       const std::wstring& localized_tooltip,
                                       bool menu_button);

  // Gtk callback for the "clicked" signal.
  static void OnButtonClick(GtkWidget* button, BrowserToolbarGtk* toolbar);

  // Gtk callback to intercept mouse clicks to the menu buttons.
  static gint OnMenuButtonPressEvent(GtkWidget* button,
                                      GdkEvent *event,
                                      BrowserToolbarGtk* toolbar);

  // Displays the page menu.
  void RunPageMenu(GdkEvent* button_press_event);

  // Displays the app menu.
  void RunAppMenu(GdkEvent* button_press_event);

  // Gtk widgets. The toolbar is an hbox with each of the other pieces of the
  // toolbar placed side by side.
  GtkWidget* toolbar_;

  // Tooltip container for all GTK widgets in this class.
  GtkTooltips* toolbar_tooltips_;

  // All the buttons in the toolbar.
  scoped_ptr<CustomDrawButton> back_, forward_;
  scoped_ptr<CustomDrawButton> reload_, home_;
  scoped_ptr<CustomDrawButton> star_, go_;
  scoped_ptr<CustomDrawButton> page_menu_button_, app_menu_button_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  scoped_ptr<MenuGtk> page_menu_;
  scoped_ptr<MenuGtk> app_menu_;

  // TODO(port): Port BackForwardMenuModel
  // scoped_ptr<BackForwardMenuModel> back_menu_model_;
  // scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  Browser* browser_;
  Profile* profile_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;
};

#endif  // CHROME_BROWSER_GTK_BROWSER_TOOLBAR_VIEW_GTK_H_
