// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
#define VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_

#include <gtk/gtk.h>

#include "views/controls/menu/menu_wrapper.h"

namespace menus {
class MenuModel;
}

namespace views {

// A Gtk implementation of MenuWrapper.
// TODO(beng): rename to MenuGtk once the old class is dead.
class NativeMenuGtk : public MenuWrapper {
 public:
  explicit NativeMenuGtk(menus::MenuModel* model);
  virtual ~NativeMenuGtk();

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment);
  virtual void CancelMenu();
  virtual void Rebuild();
  virtual void UpdateStates();
  virtual gfx::NativeMenu GetNativeMenu() const;

 private:
  static void OnMenuHidden(GtkWidget* widget, NativeMenuGtk* menu);

  void AddSeparatorAt(int index);
  GtkWidget* AddMenuItemAt(int index, GtkRadioMenuItem* radio_group);

  void ResetMenu();

  // Updates the menu item's state.
  void UpdateMenuItemState(GtkWidget* menu_item);

  static void UpdateStateCallback(GtkWidget* menu_item, gpointer data);

  // Callback for gtk_menu_popup to position the menu.
  static void MenuPositionFunc(GtkMenu* menu, int* x, int* y, gboolean* push_in,
                               void* data);

  // Event handlers:
  void OnActivate(GtkMenuItem* menu_item);

  // Gtk signal handlers.
  static void CallActivate(GtkMenuItem* menu_item, NativeMenuGtk* native_menu);

  menus::MenuModel* model_;

  GtkWidget* menu_;

  bool menu_shown_;

  // A flag used to avoid misfiring ActivateAt call on the menu model.
  // This is necessary as GTK menu fires an activate signal even when the
  // state is changed by |UpdateStates()| API.
  bool suppress_activate_signal_;

  DISALLOW_COPY_AND_ASSIGN(NativeMenuGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
