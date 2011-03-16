// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
#pragma once

#include "views/controls/menu/menu_host.h"
#include "views/widget/widget_gtk.h"

namespace views {

class SubmenuView;

// MenuHost implementation for Gtk.
class MenuHostGtk : public WidgetGtk, public MenuHost {
 public:
  explicit MenuHostGtk(SubmenuView* submenu);
  virtual ~MenuHostGtk();

  // MenuHost overrides.
  virtual void InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) OVERRIDE;
  virtual bool IsMenuHostVisible() OVERRIDE;
  virtual void ShowMenuHost(bool do_capture) OVERRIDE;
  virtual void HideMenuHost() OVERRIDE;
  virtual void DestroyMenuHost() OVERRIDE;
  virtual void SetMenuHostBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void ReleaseMenuHostCapture() OVERRIDE;
  virtual gfx::NativeWindow GetMenuHostWindow() OVERRIDE;

 protected:
  virtual RootView* CreateRootView() OVERRIDE;

  // WidgetGtk overrides:
  virtual bool ReleaseCaptureOnMouseReleased() OVERRIDE;
  virtual void ReleaseNativeCapture() OVERRIDE;
  virtual void OnDestroy(GtkWidget* object) OVERRIDE;
  virtual void HandleGtkGrabBroke() OVERRIDE;
  virtual void HandleXGrabBroke() OVERRIDE;

 private:
  void DoCapture();

  // Cancell all menus unless drag is in progress.
  void CancelAllIfNoDrag();

  // If true, DestroyMenuHost has been invoked.
  bool destroying_;

  // The view we contain.
  SubmenuView* submenu_;

  // Have we done input grab?
  bool did_input_grab_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
