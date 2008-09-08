// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/window.h"

class AeroGlassNonClientView;
class BrowserView2;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame
//
//  AeroGlassFrame is a Window subclass that provides the window frame on
//  Windows Vista with DWM desktop compositing enabled. The window's non-client
//  areas are drawn by the system.
//
class AeroGlassFrame : public BrowserFrame,
                       public ChromeViews::Window {
 public:
  explicit AeroGlassFrame(BrowserView2* browser_view);
  virtual ~AeroGlassFrame();

  void Init(const gfx::Rect& bounds);

  // Determine the distance of the left edge of the minimize button from the
  // right edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Overridden from BrowserFrame:
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds);
  virtual void SizeToContents(const gfx::Rect& contents_bounds) {}
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const;
  virtual ChromeViews::Window* GetWindow();

  // Overridden from ChromeViews::Window:
  virtual void UpdateWindowIcon();

  // Overridden from ChromeViews::HWNDViewContainer:
  virtual bool AcceleratorPressed(ChromeViews::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id,
                              ChromeViews::Accelerator* accelerator);

 protected:
  // Overridden from ChromeViews::HWNDViewContainer:
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual void OnExitMenuLoop(bool is_track_popup_menu);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& pt);

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Return a pointer to the concrete type of our non-client view.
  AeroGlassNonClientView* GetAeroGlassNonClientView() const;

  // The BrowserView2 is our ClientView. This is a pointer to it.
  BrowserView2* browser_view_;

  bool frame_initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_

