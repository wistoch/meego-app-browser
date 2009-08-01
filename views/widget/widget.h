// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_H_
#define VIEWS_WIDGET_WIDGET_H_

#include "base/gfx/native_widget_types.h"

class ThemeProvider;

namespace gfx {
class Path;
class Point;
class Rect;
}

namespace views {

class Accelerator;
class FocusManager;
class RootView;
class TooltipManager;
class View;
class Window;

////////////////////////////////////////////////////////////////////////////////
//
// Widget interface
//
//   Widget is an abstract class that defines the API that should be implemented
//   by a native window in order to host a view hierarchy.
//
//   Widget wraps a hierarchy of View objects (see view.h) that implement
//   painting and flexible layout within the bounds of the Widget's window.
//
//   The Widget is responsible for handling various system events and forwarding
//   them to the appropriate view.
//
/////////////////////////////////////////////////////////////////////////////

class Widget {
 public:
  virtual ~Widget() { }

  // Creates a transparent popup widget specific to the current platform useful
  // for transient status notifications.
  static Widget* CreateTransparentPopupWidget(bool delete_on_destroy);

  // Initialize the Widget with a parent and an initial desired size.
  // |contents_view| is the view that will be the single child of RootView
  // within this Widget. As contents_view is inserted into RootView's tree,
  // RootView assumes ownership of this view and cleaning it up. If you remove
  // this view, you are responsible for its destruction. If this value is NULL,
  // the caller is responsible for populating the RootView, and sizing its
  // contents as the window is sized.
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds) = 0;

  // Sets the specified view as the contents of this Widget. There can only
  // be one contnets view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  virtual void SetContentsView(View* view) = 0;

  // Returns the bounds of this Widget in the screen coordinate system.
  // If the receiving Widget is a frame which is larger than its client area,
  // this method returns the client area if including_frame is false and the
  // frame bounds otherwise. If the receiving Widget is not a frame,
  // including_frame is ignored.
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const = 0;

  // Sizes and/or places the widget to the specified bounds, size or position.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Sets a shape on the widget.
  virtual void SetShape(const gfx::Path& shape) = 0;

  // Hides the widget then closes it after a return to the message loop.
  virtual void Close() = 0;

  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  virtual void CloseNow() = 0;

  // Shows or hides the widget, without changing activation state.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Returns the gfx::NativeView associated with this Widget.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Forces a paint of a specified rectangle immediately.
  virtual void PaintNow(const gfx::Rect& update_rect) = 0;

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system. Note that the caller must then schedule a
  // repaint to allow this change to take effect.
  virtual void SetOpacity(unsigned char opacity) = 0;

  // Returns the RootView contained by this Widget.
  virtual RootView* GetRootView() = 0;

  // Returns the Widget associated with the root ancestor.
  virtual Widget* GetRootWidget() const = 0;

  // Returns whether the Widget is visible to the user.
  virtual bool IsVisible() const = 0;

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() const = 0;

  // Returns the TooltipManager for this Widget. If this Widget does not support
  // tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager() {
    return NULL;
  }

  // Starts a drag operation for the specified view. |point| is a position in
  // |view| coordinates that the drag was initiated from.
  virtual void GenerateMousePressedForView(View* view,
                                           const gfx::Point& point) = 0;

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id,
                              Accelerator* accelerator) = 0;

  // Returns the Window containing this Widget, or NULL if not contained in a
  // window.
  virtual Window* GetWindow() { return NULL; }
  virtual const Window* GetWindow() const { return NULL; }

  // Gets the theme provider.
  virtual ThemeProvider* GetThemeProvider() const { return NULL; }

  // Gets the default theme provider; this is necessary for when a widget has
  // no profile (and ThemeProvider) associated with it. The default theme
  // provider provides a default set of bitmaps that such widgets can use.
  virtual ThemeProvider* GetDefaultThemeProvider() { return NULL; }

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  virtual FocusManager* GetFocusManager() { return NULL; }
};

}  // namespace views

#endif // VIEWS_WIDGET_WIDGET_H_
