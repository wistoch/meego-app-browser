// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_WINDOW_GTK_H_
#define CHROME_VIEWS_WINDOW_WINDOW_GTK_H_

#include "base/basictypes.h"
#include "chrome/views/widget/widget_gtk.h"
#include "chrome/views/window/window.h"

namespace gfx {
class Point;
class Size;
};

namespace views {

class Client;
class WindowDelegate;

// Window implementation for GTK.
class WindowGtk : public WidgetGtk, public Window {
 public:
  virtual ~WindowGtk();

  // Window overrides:
  virtual gfx::Rect GetBounds() const;
  virtual gfx::Rect GetNormalBounds() const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window);
  virtual void Show();
  virtual void Activate();
  virtual void Close();
  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();
  virtual bool IsActive() const;
  virtual bool IsVisible() const;
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual void EnableClose(bool enable);
  virtual void DisableInactiveRendering();
  virtual void UpdateWindowTitle();
  virtual void UpdateWindowIcon();
  virtual NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual WindowDelegate* GetDelegate() const;
  virtual NonClientView* GetNonClientView() const;
  virtual ClientView* GetClientView() const;
  virtual gfx::NativeWindow GetNativeWindow() const;

  virtual Window* AsWindow() { return this; }
  virtual const Window* AsWindow() const { return this; }

 protected:
  // For  the constructor.
  friend class Window;

  // Constructs the WindowGtk. |window_delegate| cannot be NULL.
  explicit WindowGtk(WindowDelegate* window_delegate);

  // Initializes the window to the passed in bounds.
  void Init(const gfx::Rect& bounds);

 private:
  // Whether or not the window is modal. This comes from the delegate and is
  // cached at Init time to avoid calling back to the delegate from the
  // destructor.
  bool is_modal_;

  // Whether the window is currently always on top.
  bool is_always_on_top_;

  // Our window delegate.
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  DISALLOW_COPY_AND_ASSIGN(WindowGtk);
};

}  // namespace views

#endif  // CHROME_VIEWS_WINDOW_WINDOW_GTK_H_
