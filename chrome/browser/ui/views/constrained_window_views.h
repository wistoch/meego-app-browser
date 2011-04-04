// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#pragma once

#include "content/browser/tab_contents/constrained_window.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/gfx/rect.h"
#include "views/window/window_win.h"

class ConstrainedTabContentsWindowDelegate;
class ConstrainedWindowAnimation;
class ConstrainedWindowFrameView;
namespace views {
class WindowDelegate;
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowViews
//
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame.
//
class ConstrainedWindowViews : public ConstrainedWindow,
                               public views::WindowWin {
 public:
  virtual ~ConstrainedWindowViews();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  // Overridden from views::Window:
  virtual views::NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow() OVERRIDE;
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual void FocusConstrainedWindow() OVERRIDE;

  virtual std::wstring GetWindowTitle() const;
  virtual const gfx::Rect& GetCurrentBounds() const;

 protected:
  // Windows message handlers:
  virtual void OnFinalMessage(HWND window) OVERRIDE;
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE;

 private:
  friend class ConstrainedWindow;

  // Use the static factory methods on ConstrainedWindow to construct a
  // ConstrainedWindow.
  ConstrainedWindowViews(TabContents* owner,
                         views::WindowDelegate* window_delegate);

  // Moves this window to the front of the Z-order and registers us with the
  // focus manager.
  void ActivateConstrainedWindow();

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // Current "anchor point", the lower right point at which we render
  // the constrained title bar.
  gfx::Point anchor_point_;

  // Current display rectangle (relative to owner_'s visible area).
  gfx::Rect current_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
