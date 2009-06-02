// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "views/window/custom_frame_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace views {

WindowGtk::~WindowGtk() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  WindowGtk* window = new WindowGtk(window_delegate);
  window->GetNonClientView()->SetFrameView(window->CreateFrameViewForWindow());
  window->Init(bounds);
  return window;
}

// static
void Window::CloseAllSecondaryWindows() {
  NOTIMPLEMENTED();
}

gfx::Rect WindowGtk::GetBounds() const {
  gfx::Rect bounds;
  WidgetGtk::GetBounds(&bounds, true);
  return bounds;
}

gfx::Rect WindowGtk::GetNormalBounds() const {
  NOTIMPLEMENTED();
  return GetBounds();
}

void WindowGtk::SetBounds(const gfx::Rect& bounds,
                          gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  WidgetGtk::SetBounds(bounds);
}

void WindowGtk::Show() {
  gtk_widget_show_all(GetNativeView());
}

void WindowGtk::HideWindow() {
  NOTIMPLEMENTED();
}

void WindowGtk::PushForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::PopForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::Activate() {
  NOTIMPLEMENTED();
}

void WindowGtk::Close() {
  if (window_closed_) {
    // Don't do anything if we've already been closed.
    return;
  }

  if (non_client_view_->CanClose()) {
    SaveWindowPosition();
    WidgetGtk::Close();
    window_closed_ = true;
  }
}

void WindowGtk::Maximize() {
  gtk_window_maximize(GetNativeWindow());
}

void WindowGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

void WindowGtk::Restore() {
  NOTIMPLEMENTED();
}

bool WindowGtk::IsActive() const {
  return gtk_window_is_active(GetNativeWindow());
}

bool WindowGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView());
}

bool WindowGtk::IsMaximized() const {
  return window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
}

bool WindowGtk::IsMinimized() const {
  return window_state_ & GDK_WINDOW_STATE_ICONIFIED;
}

void WindowGtk::SetFullscreen(bool fullscreen) {
  if (fullscreen)
    gtk_window_fullscreen(GetNativeWindow());
  else
    gtk_window_unfullscreen(GetNativeWindow());
}

bool WindowGtk::IsFullscreen() const {
  return window_state_ & GDK_WINDOW_STATE_FULLSCREEN;
}

void WindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void WindowGtk::DisableInactiveRendering() {
  NOTIMPLEMENTED();
}

void WindowGtk::UpdateWindowTitle() {
  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);

  gtk_window_set_title(GetNativeWindow(), WideToUTF8(window_title).c_str());
}

void WindowGtk::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

void WindowGtk::SetIsAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

NonClientFrameView* WindowGtk::CreateFrameViewForWindow() {
  // TODO(erg): Always use a custom frame view? Are there cases where we let
  // the window manager deal with the X11 equivalent of the "non-client" area?
  return new CustomFrameView(this);
}

void WindowGtk::UpdateFrameAfterFrameChange() {
  NOTIMPLEMENTED();
}

WindowDelegate* WindowGtk::GetDelegate() const {
  return window_delegate_;
}

NonClientView* WindowGtk::GetNonClientView() const {
  return non_client_view_;
}

ClientView* WindowGtk::GetClientView() const {
  return non_client_view_->client_view();
}

gfx::NativeWindow WindowGtk::GetNativeWindow() const {
  return GTK_WINDOW(GetNativeView());
}

bool WindowGtk::ShouldUseNativeFrame() const {
  return false;
}

void WindowGtk::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

WindowGtk::WindowGtk(WindowDelegate* window_delegate)
    : WidgetGtk(TYPE_WINDOW),
      is_modal_(false),
      window_delegate_(window_delegate),
      non_client_view_(new NonClientView(this)),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      window_closed_(false) {
  is_window_ = true;
  window_delegate_->window_.reset(this);
}

void WindowGtk::Init(const gfx::Rect& bounds) {
  // We call this after initializing our members since our implementations of
  // assorted WidgetWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_) {
    // TODO(erg): Fix once modality works.
    // BecomeModal();
  }

  WidgetGtk::Init(NULL, bounds, true);

  // Create the ClientView, add it to the NonClientView and add the
  // NonClientView to the RootView. This will cause everything to be parented.
  non_client_view_->set_client_view(window_delegate_->CreateClientView(this));
  WidgetGtk::SetContentsView(non_client_view_);

  UpdateWindowTitle();

  GtkWindow* gtk_window = GetNativeWindow();
  g_signal_connect(G_OBJECT(gtk_window),
                   "window-state-event",
                   G_CALLBACK(CallWindowStateEvent),
                   NULL);

  SetInitialBounds(bounds);

  // if (!IsAppWindow()) {
  //   notification_registrar_.Add(
  //       this,
  //       NotificationType::ALL_APPWINDOWS_CLOSED,
  //       NotificationService::AllSources());
  // }

  // ResetWindowRegion(false);
}

void WindowGtk::SetInitialBounds(const gfx::Rect& create_bounds) {
  gfx::Rect saved_bounds(create_bounds.ToGdkRectangle());
  if (window_delegate_->GetSavedWindowBounds(&saved_bounds)) {
    WidgetGtk::SetBounds(saved_bounds);
  } else {
    if (create_bounds.IsEmpty()) {
      SizeWindowToDefault();
    } else {
      SetBounds(create_bounds, NULL);
    }
  }
}

void WindowGtk::SizeWindowToDefault() {
  gfx::Size size = non_client_view_->GetPreferredSize();
  gfx::Rect bounds(size.width(), size.height());
  SetBounds(bounds, NULL);
}

void WindowGtk::SaveWindowPosition() {
  // The delegate may have gone away on us.
  if (!window_delegate_)
    return;

  NOTIMPLEMENTED();
}

// static
void WindowGtk::CallWindowStateEvent(GtkWidget* widget,
                                     GdkEventWindowState* window_state) {
  WindowGtk* window_gtk = GetWindowForNative(widget);
  window_gtk->window_state_ = window_state->new_window_state;
}

}  // namespace views
