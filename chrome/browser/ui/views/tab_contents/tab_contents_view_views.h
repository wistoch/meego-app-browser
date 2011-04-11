// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/gfx/size.h"
#include "views/widget/widget_win.h"

class RenderViewContextMenuViews;
class SadTabView;
class SkBitmap;
class TabContentsDragWin;
struct WebDropData;
class WebDragSource;
class WebDropTarget;
namespace gfx {
class Point;
}

// Windows-specific implementation of the TabContentsView. It is a HWND that
// contains all of the contents of the tab and associated child views.
class TabContentsViewViews : public TabContentsView,
                             public views::WidgetWin {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewViews(TabContents* tab_contents);
  virtual ~TabContentsViewViews();

  // Reset the native parent of this view to NULL.  Unparented windows should
  // not receive any messages.
  virtual void Unparent();

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void SetPageTitle(const std::wstring& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  // WidgetWin overridde.
  virtual views::FocusManager* GetFocusManager() OVERRIDE;

  void EndDragging();

  WebDropTarget* drop_target() const { return drop_target_.get(); }

 private:
  // A helper method for closing the tab.
  void CloseTab();

  // Windows events ------------------------------------------------------------

  // Overrides from WidgetWin.
  virtual void OnDestroy() OVERRIDE;
  virtual void OnHScroll(int scroll_type,
                         short position,
                         HWND scrollbar) OVERRIDE;
  virtual LRESULT OnMouseRange(UINT msg,
                               WPARAM w_param,
                               LPARAM l_param) OVERRIDE;
  virtual void OnPaint(HDC junk_dc) OVERRIDE;
  virtual LRESULT OnReflectedMessage(UINT msg,
                                     WPARAM w_param,
                                     LPARAM l_param) OVERRIDE;
  virtual void OnVScroll(int scroll_type,
                         short position,
                         HWND scrollbar) OVERRIDE;
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) OVERRIDE;
  virtual void OnSize(UINT param, const WTL::CSize& size) OVERRIDE;
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param) OVERRIDE;
  virtual void OnNCPaint(HRGN rgn) OVERRIDE;

  // Backend for all scroll messages, the |message| parameter indicates which
  // one it is.
  void ScrollCommon(UINT message, int scroll_type, short position,
                    HWND scrollbar);

  // Handles notifying the TabContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // TODO(brettw) comment these. They're confusing.
  bool ScrollZoom(int scroll_type);
  void WheelZoom(int distance);

  // ---------------------------------------------------------------------------

  // A drop target object that handles drags over this TabContents.
  scoped_refptr<WebDropTarget> drop_target_;

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  SadTabView* sad_tab_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // The FocusManager associated with this tab.  Stored as it is not directly
  // accessible when unparented.
  views::FocusManager* focus_manager_;

  // Set to true if we want to close the tab after the system drag operation
  // has finished.
  bool close_tab_after_drag_ends_;

  // Used to close the tab after the stack has unwound.
  base::OneShotTimer<TabContentsViewViews> close_tab_timer_;

  // Used to handle the drag-and-drop.
  scoped_refptr<TabContentsDragWin> drag_handler_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
