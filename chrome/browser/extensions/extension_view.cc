// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#if defined(OS_WIN)
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#endif
#include "views/widget/widget.h"

ExtensionView::ExtensionView(ExtensionHost* host, Browser* browser,
                             const GURL& content_url)
    : host_(host), browser_(browser), content_url_(content_url),
      initialized_(false) {
  host_->set_view(this);
}

ExtensionView::~ExtensionView() {
  if (GetHWND())
    Detach();
}

void ExtensionView::SetVisible(bool is_visible) {
  if (is_visible != IsVisible()) {
    HWNDView::SetVisible(is_visible);

    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View heirarchy and does not know about the change
    // unless we tell it.
    if (render_view_host()->view()) {
      if (is_visible)
        render_view_host()->view()->Show();
      else
        render_view_host()->view()->Hide();
    }
  }
}

void ExtensionView::DidChangeBounds(const gfx::Rect& previous,
                                    const gfx::Rect& current) {
  // Propagate the new size to RenderWidgetHostView.
  // We can't send size zero because RenderWidget DCHECKs that.
  if (render_view_host()->view() && !current.IsEmpty())
    render_view_host()->view()->SetSize(gfx::Size(width(), height()));
  // Layout is where the HWND is properly positioned.
  // TODO(erikkay) - perhaps this should be in HWNDView
  Layout();
}

void ExtensionView::ShowIfCompletelyLoaded() {
  // We wait to show the ExtensionView until it has loaded and our parent has
  // given us a background. These can happen in different orders.
  if (!IsVisible() && host_->did_stop_loading() && render_view_host()->view() &&
      !render_view_host()->view()->background().empty()) {
    SetVisible(true);
    DidContentsPreferredWidthChange(pending_preferred_width_);
  }
}

void ExtensionView::SetBackground(const SkBitmap& background) {
  if (initialized_ && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
  ShowIfCompletelyLoaded();
}

void ExtensionView::DidContentsPreferredWidthChange(const int pref_width) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!IsVisible()) {
    pending_preferred_width_ = pref_width;
  } else if (pref_width > 0 && pref_width != GetPreferredSize().width()) {
    SetPreferredSize(gfx::Size(pref_width, height()));
  }
}

void ExtensionView::ViewHierarchyChanged(bool is_add,
                                         views::View *parent,
                                         views::View *child) {
  if (is_add && GetWidget() && !initialized_) {
    initialized_ = true;

    RenderWidgetHostView* view = RenderWidgetHostView::CreateViewForWidget(
        render_view_host());

    // TODO(mpcomplete): RWHV needs a cross-platform Init function.
#if defined(OS_WIN)
    // Create the HWND. Note:
    // RenderWidgetHostHWND supports windowed plugins, but if we ever also
    // wanted to support constrained windows with this, we would need an
    // additional HWND to parent off of because windowed plugin HWNDs cannot
    // exist in the same z-order as constrained windows.
    RenderWidgetHostViewWin* view_win =
        static_cast<RenderWidgetHostViewWin*>(view);
    HWND hwnd = view_win->Create(GetWidget()->GetNativeView());
    view_win->ShowWindow(SW_SHOW);
    Attach(hwnd);
#else
    NOTIMPLEMENTED();
#endif

    host_->CreateRenderView(content_url_, view);
    SetVisible(false);

    if (!pending_background_.empty()) {
      render_view_host()->view()->SetBackground(pending_background_);
      pending_background_.reset();
    }
  }
}
