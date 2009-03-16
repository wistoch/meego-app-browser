// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"

#include "webkit/glue/webinputevent.h"

namespace {

// Called when the content view gtk widget is tabbed to. We always return true
// and grab focus if we don't have it. The call to SetInitialFocus() forwards
// the tab to webkit. We leave focus via TakeFocus().
// We cast the WebContents to a TabContents because SetInitialFocus is public
// in TabContents and protected in WebContents.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  if (GTK_WIDGET_HAS_FOCUS(widget))
    return TRUE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->SetInitialFocus(reverse);
  return TRUE;
}

// Whenever we lose focus, set the cursor back to that of our parent window,
// which should be the default arrow.
gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* event, void*) {
  gdk_window_set_cursor(widget->window, NULL);
  return FALSE;
}

// Callback used in WebContentsViewGtk::CreateViewForWidget().
void RemoveWidget(GtkWidget* widget, gpointer container) {
  gtk_container_remove(GTK_CONTAINER(container), widget);
}

}  // namespace

// static
WebContentsView* WebContentsView::Create(WebContents* web_contents) {
  return new WebContentsViewGtk(web_contents);
}

WebContentsViewGtk::WebContentsViewGtk(WebContents* web_contents)
    : web_contents_(web_contents),
      vbox_(gtk_vbox_new(FALSE, 0)) {
}

WebContentsViewGtk::~WebContentsViewGtk() {
  vbox_.Destroy();
}

WebContents* WebContentsViewGtk::GetWebContents() {
  return web_contents_;
}

void WebContentsViewGtk::CreateView() {
  NOTIMPLEMENTED();
}

RenderWidgetHostView* WebContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  gtk_widget_show(view->native_view());
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), web_contents_);
  g_signal_connect(view->native_view(), "focus-out-event",
                   G_CALLBACK(OnFocusOut), NULL);
  gtk_container_foreach(GTK_CONTAINER(vbox_.get()), RemoveWidget, vbox_.get());
  gtk_box_pack_start(GTK_BOX(vbox_.get()), view->native_view(), TRUE, TRUE, 0);
  return view;
}

gfx::NativeView WebContentsViewGtk::GetNativeView() const {
  return vbox_.get();
}

gfx::NativeView WebContentsViewGtk::GetContentNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeWindow WebContentsViewGtk::GetTopLevelNativeView() const {
  return GTK_WINDOW(gtk_widget_get_toplevel(vbox_.get()));
}

void WebContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::OnContentsDestroy() {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::SetPageTitle(const std::wstring& title) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::Invalidate() {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::SizeContents(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::HideFindBar(bool end_session) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ReparentFindWindow(Browser* new_browser) const {
  NOTIMPLEMENTED();
}

bool WebContentsViewGtk::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewGtk::SetInitialFocus() {
  // TODO(port)
}

void WebContentsViewGtk::StoreFocus() {
  // TODO(port)
}

void WebContentsViewGtk::RestoreFocus() {
  // TODO(port)
}

void WebContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewGtk::TakeFocus(bool reverse) {
  web_contents_->delegate()->SetFocusToLocationBar();
}

void WebContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // This may be an accelerator. Pass it on to GTK.
  GtkWindow* window = GetTopLevelNativeView();
  gtk_accel_groups_activate(G_OBJECT(window), event.os_event->keyval,
                            GdkModifierType(event.os_event->state));
}

void WebContentsViewGtk::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(web_contents_, params));
  context_menu_->Popup();
}

void WebContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();
}

WebContents* WebContentsViewGtk::CreateNewWindowInternal(
    int route_id,
    base::WaitableEvent* modal_dialog_event) {
  NOTIMPLEMENTED();
  return NULL;
}

RenderWidgetHostView* WebContentsViewGtk::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGtk::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}
