// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/gtk_util.h"

namespace {

// Called when the content view gtk widget is tabbed to. We always return true
// and grab focus if we don't have it. The call to SetInitialFocus(bool)
// forwards the tab to webkit. We leave focus via TakeFocus().
// We cast the TabContents to a TabContents because SetInitialFocus is public
// in TabContents and protected in TabContents.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  if (GTK_WIDGET_HAS_FOCUS(widget))
    return TRUE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->SetInitialFocus(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, true);
  return FALSE;
}

}  // namespace

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      vbox_(gtk_vbox_new(FALSE, 0)),
      content_view_(NULL) {
}

TabContentsViewGtk::~TabContentsViewGtk() {
  vbox_.Destroy();
}

void TabContentsViewGtk::CreateView() {
  NOTIMPLEMENTED();
}

RenderWidgetHostView* TabContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  content_view_ = view->native_view();
  g_signal_connect(content_view_, "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  g_signal_connect(view->native_view(), "button-press-event",
                   G_CALLBACK(OnMouseDown), this);
  gfx::RemoveAllChildren(vbox_.get());
  gtk_box_pack_start(GTK_BOX(vbox_.get()), content_view_, TRUE, TRUE, 0);
  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return vbox_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  return content_view_;
}

gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  // This is used for positioning the download shelf arrow animation,
  // as well as sizing some other widgets in Windows.  In GTK the size is
  // managed for us, so it appears to be only used for the download shelf
  // animation.
  out->SetRect(vbox_.get()->allocation.x, vbox_.get()->allocation.y,
               vbox_.get()->allocation.width, vbox_.get()->allocation.height);
}

void TabContentsViewGtk::OnContentsDestroy() {
  // TODO(estade): Windows uses this function cancel pending drag-n-drop drags.
  // We don't have drags yet, so do nothing for now.
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  if (content_view_ && content_view_->window)
    gdk_window_set_title(content_view_->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::Invalidate() {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::HideFindBar(bool end_session) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::ReparentFindWindow(Browser* new_browser) const {
  NOTIMPLEMENTED();
}

bool TabContentsViewGtk::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  NOTIMPLEMENTED();
  return false;
}

void TabContentsViewGtk::Focus() {
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    gtk_widget_grab_focus(content_view_);
}

void TabContentsViewGtk::StoreFocus() {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::RestoreFocus() {
  // TODO(estade): implement this function.
  NOTIMPLEMENTED() << " Need to restore the focus position on this page.";
}

void TabContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  tab_contents()->delegate()->SetFocusToLocationBar();
}

void TabContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // This may be an accelerator. Try to pass it on to our browser window
  // to handle.
  GtkWindow* window = GetTopLevelNativeWindow();
  // It's possible to not be associated with a window at the time when we're
  // handling the keyboard event (e.g., the user opened a new tab in the time).
  // What we really want to do is get whatever currently has focus and have
  // that handle the accelerator.  TODO(tc): Consider walking
  // gtk_window_list_toplevels to find what has focus and if that's a browser
  // window, forward the event.
  if (!window)
    return;

  BrowserWindowGtk* browser_window = static_cast<BrowserWindowGtk*>(
      g_object_get_data(G_OBJECT(window), "browser_window_gtk"));
  DCHECK(browser_window);
  browser_window->HandleAccelerator(event.os_event->keyval,
      static_cast<GdkModifierType>(event.os_event->state));
}

void TabContentsViewGtk::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(tab_contents(), params,
                                                   last_mouse_down_time_));
  context_menu_->Popup();
}

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();

  // Until we have d'n'd implemented, just immediately pretend we're
  // already done with the drag and drop so we don't get stuck
  // thinking we're in mid-drag.
  // TODO(port): remove me when the above NOTIMPLEMENTED is fixed.
  if (tab_contents()->render_view_host())
    tab_contents()->render_view_host()->DragSourceSystemDragEnded();
}

gboolean TabContentsViewGtk::OnMouseDown(GtkWidget* widget,
    GdkEventButton* event, TabContentsViewGtk* view) {
  view->last_mouse_down_time_ = event->time;
  return FALSE;
}
