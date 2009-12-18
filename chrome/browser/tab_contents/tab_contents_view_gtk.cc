// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "app/gtk_dnd_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/gtk_expanded_container.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/sad_tab_gtk.h"
#include "chrome/browser/gtk/tab_contents_drag_source.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

namespace {

// TODO(erg): I have no idea how to programatically figure out how wide the
// vertical scrollbar is. Hack it with a hardcoded value for now.
const int kScrollbarWidthHack = 25;

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, gfx::Point(event->x_root, event->y_root), false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, gfx::Point(event->x_root, event->y_root), true);
  return FALSE;
}

// See tab_contents_view_win.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      tab_contents->delegate()->ContentsZoomChange(false);
      return TRUE;
    } else if (event->direction == GDK_SCROLL_UP) {
      tab_contents->delegate()->ContentsZoomChange(true);
      return TRUE;
    }
  }

  return FALSE;
}

}  // namespace

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      floating_(gtk_floating_container_new()),
      expanded_(gtk_expanded_container_new()),
      popup_view_(NULL) {
  gtk_widget_set_name(expanded_, "chrome-tab-contents-view");
  g_signal_connect(expanded_, "size-allocate",
                   G_CALLBACK(OnSizeAllocate), this);
  g_signal_connect(expanded_, "child-size-request",
                   G_CALLBACK(OnChildSizeRequest), this);
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPosition), this);

  gtk_container_add(GTK_CONTAINER(floating_.get()), expanded_);
  gtk_widget_show(expanded_);
  gtk_widget_show(floating_.get());
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));
  drag_source_.reset(new TabContentsDragSource(this));
}

TabContentsViewGtk::~TabContentsViewGtk() {
  floating_.Destroy();
}

void TabContentsViewGtk::AttachBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == NULL);
  popup_view_ = popup_view;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      popup_view->widget());
}

void TabContentsViewGtk::RemoveBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == popup_view);
  gtk_container_remove(GTK_CONTAINER(floating_.get()), popup_view->widget());
  popup_view_ = NULL;
}

void TabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void TabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());

  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
  constrained_windows_.erase(item);
}

void TabContentsViewGtk::CreateView(const gfx::Size& initial_size) {
  requested_size_ = initial_size;
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
  gfx::NativeView content_view = view->native_view();
  g_signal_connect(content_view, "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(content_view, "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
  g_signal_connect(content_view, "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(content_view, "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(content_view, GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  g_signal_connect(content_view, "button-press-event",
                   G_CALLBACK(OnMouseDown), this);
  InsertIntoContentArea(content_view);

  // Renderer target DnD.
  drag_dest_.reset(new WebDragDestGtk(tab_contents(), content_view));

  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return floating_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
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
  int x = 0;
  int y = 0;
  if (expanded_->window)
    gdk_window_get_origin(expanded_->window, &x, &y);
  out->SetRect(x + expanded_->allocation.x, y + expanded_->allocation.y,
               requested_size_.width(), requested_size_.height());
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed() {
  if (tab_contents() != NULL && !sad_tab_.get()) {
    sad_tab_.reset(new SadTabGtk(tab_contents()));
    InsertIntoContentArea(sad_tab_->widget());
    gtk_widget_show(sad_tab_->widget());
  }
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // We don't need to manually set the size of of widgets in GTK+, but we do
  // need to pass the sizing information on to the RWHV which will pass the
  // sizing information on to the renderer.
  requested_size_ = size;
  if (tab_contents()->render_widget_host_view())
    tab_contents()->render_widget_host_view()->SetSize(size);
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->showing_interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
  } else {
    GtkWidget* widget = GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  focus_store_.Store(GetNativeView());
}

void TabContentsViewGtk::RestoreFocus() {
  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    SetInitialFocus();
}

void TabContentsViewGtk::UpdateDragCursor(WebDragOperation operation) {
  drag_dest_->UpdateDragStatus(operation);
}

void TabContentsViewGtk::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  if (!tab_contents()->delegate()->TakeFocus(reverse)) {
    gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
        reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
  }
}

void TabContentsViewGtk::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      // No need to remove the SadTabGtk's widget from the container since
      // the new RenderWidgetHostViewGtk instance already removed all the
      // vbox's children.
      sad_tab_.reset();
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
      break;
  }
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(tab_contents(), params,
                                                   last_mouse_down_.time));
  context_menu_->Init();

  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  gfx::Point point = bounds.origin();
  point.Offset(params.x, params.y);
  context_menu_->Popup(point);
}

// Render view DnD -------------------------------------------------------------

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops) {
  DCHECK(GetContentNativeView());

  drag_source_->StartDragging(drop_data, &last_mouse_down_);
  // TODO(snej): Make use of the WebDragOperationsMask somehow
}

// -----------------------------------------------------------------------------

void TabContentsViewGtk::InsertIntoContentArea(GtkWidget* widget) {
  gtk_container_add(GTK_CONTAINER(expanded_), widget);
}

gboolean TabContentsViewGtk::OnMouseDown(GtkWidget* widget,
    GdkEventButton* event, TabContentsViewGtk* view) {
  view->last_mouse_down_ = *event;
  return FALSE;
}

void TabContentsViewGtk::OnChildSizeRequest(GtkWidget* widget,
                                            GtkWidget* child,
                                            GtkRequisition* requisition,
                                            TabContentsViewGtk* view) {
  if (view->tab_contents()->delegate()) {
    requisition->height +=
        view->tab_contents()->delegate()->GetExtraRenderViewHeight();
  }
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation,
                                        TabContentsViewGtk* view) {
  int width = allocation->width;
  int height = allocation->height;
  // |delegate()| can be NULL here during browser teardown.
  if (view->tab_contents()->delegate())
    height += view->tab_contents()->delegate()->GetExtraRenderViewHeight();
  gfx::Size size(width, height);
  view->requested_size_ = size;

  // We manually tell our RWHV to resize the renderer content.  This avoids
  // spurious resizes from GTK+.
  if (view->tab_contents()->render_widget_host_view())
    view->tab_contents()->render_widget_host_view()->SetSize(size);
  if (view->tab_contents()->interstitial_page())
    view->tab_contents()->interstitial_page()->SetSize(size);
}

// static
void TabContentsViewGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    TabContentsViewGtk* tab_contents_view) {
  if (tab_contents_view->popup_view_) {
    GtkWidget* widget = tab_contents_view->popup_view_->widget();

    // Look at the size request of the status bubble and tell the
    // GtkFloatingContainer where we want it positioned.
    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(
        allocation->x + allocation->width - requisition.width -
        kScrollbarWidthHack, 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(
        allocation->y + allocation->height - requisition.height, 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }

  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = std::max((allocation->x + allocation->width) / 2, 0);
  int half_view_height = std::max((allocation->y + allocation->height) / 2, 0);
  std::vector<ConstrainedWindowGtk*>::iterator it =
      tab_contents_view->constrained_windows_.begin();
  std::vector<ConstrainedWindowGtk*>::iterator end =
      tab_contents_view->constrained_windows_.end();
  for (; it != end; ++it) {
    GtkWidget* widget = (*it)->widget();
    DCHECK(widget->parent == tab_contents_view->floating_.get());

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(half_view_height - (requisition.height / 2), 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }
}
