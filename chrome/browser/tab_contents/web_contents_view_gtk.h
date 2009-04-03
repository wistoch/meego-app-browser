// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_GTK_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/common/owned_widget_gtk.h"

class RenderViewContextMenuGtk;

class WebContentsViewGtk : public WebContentsView {
 public:
  // The corresponding WebContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit WebContentsViewGtk(WebContents* web_contents);
  virtual ~WebContentsViewGtk();

  // WebContentsView implementation --------------------------------------------

  virtual void CreateView();
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);

  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void OnContentsDestroy();
  virtual void SetPageTitle(const std::wstring& title);
  virtual void Invalidate();
  virtual void SizeContents(const gfx::Size& size);
  virtual void FindInPage(const Browser& browser,
                          bool find_next, bool forward_direction);
  virtual void HideFindBar(bool end_session);
  virtual void ReparentFindWindow(Browser* new_browser) const;
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) const;
  virtual void SetInitialFocus();
  virtual void StoreFocus();
  virtual void RestoreFocus();

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);
 private:
  // We keep track of the timestamp of the latest mousedown event.
  static gboolean OnMouseDown(GtkWidget* widget,
                              GdkEventButton* event, WebContentsViewGtk* view);

  // The native widget for the tab.
  OwnedWidgetGtk vbox_;

  // The native widget for the contents of the tab. We do not own this widget.
  GtkWidget* content_view_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuGtk> context_menu_;

  // The event time for the last mouse down we handled. We need this to properly
  // show context menus.
  guint32 last_mouse_down_time_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGtk);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_GTK_H_
