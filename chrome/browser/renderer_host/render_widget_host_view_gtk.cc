// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "third_party/WebKit/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "webkit/glue/webcursor_gtk_data.h"

using WebKit::WebInputEventFactory;

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_drawing_area_new();
    gtk_widget_set_double_buffered(widget, FALSE);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "configure-event",
                     G_CALLBACK(ConfigureEvent), host_view);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(OnFocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(OnFocusOut), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    g_signal_connect(widget, "scroll-event",
                     G_CALLBACK(MouseScrollEvent), host_view);

    GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_text_targets(target_list, 0);
    gint num_targets = 0;
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list,
                                                            &num_targets);
    gtk_selection_clear_targets(widget, GDK_SELECTION_PRIMARY);
    gtk_selection_add_targets(widget, GDK_SELECTION_PRIMARY, targets,
                              num_targets);
    gtk_target_list_unref(target_list);
    gtk_target_table_free(targets, num_targets);

    // When X requests the contents of the clipboard, GTK will emit the
    // selection_request_event signal. The default handler would then
    // synchronously emit the selection_get signal. However, we want to
    // respond to the selection_request_event asynchronously, so we intercept
    // the signal in OnSelectionRequest, request the selection text from the
    // render view, and return TRUE so the default handler won't be called. Then
    // when we get the selection text back from the renderer in
    // SetSelectionText() we will call manually the selection_request_event
    // default handler.
    g_signal_connect(widget, "selection_request_event",
                     G_CALLBACK(OnSelectionRequest), host_view);
    g_signal_connect(widget, "selection_get",
                     G_CALLBACK(OnSelectionGet), host_view);

    // In OnSelectionGet, we need to access |host_view| to get the selection
    // text.
    g_object_set_data(G_OBJECT(widget), "render-widget-host-view-gtk",
                      host_view);
    return widget;
  }

 private:
  static gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                                 RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->WasResized();
    return FALSE;
  }

  static gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                              RenderWidgetHostViewGtk* host_view) {
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       RenderWidgetHostViewGtk* host_view) {
    NativeWebKeyboardEvent wke(event);
    host_view->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);
    // We return TRUE because we did handle the event. If it turns out webkit
    // can't handle the event, we'll deal with it in
    // RenderView::UnhandledKeyboardEvent().
    return TRUE;
  }

  static gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* focus,
                          RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->Focus();
    return FALSE;
  }

  static gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* focus,
                           RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    host_view->GetRenderWidgetHost()->Blur();
    return FALSE;
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));
    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->ForwardWheelEvent(
        WebInputEventFactory::mouseWheelEvent(event));
    return FALSE;
  }


  static gboolean OnSelectionRequest(GtkWidget* widget,
                                     GdkEventSelection* event) {
    RenderWidgetHostViewGtk* host_view =
        reinterpret_cast<RenderWidgetHostViewGtk*>(
        g_object_get_data(G_OBJECT(widget), "render-widget-host-view-gtk"));

    // If we already know the selection text, return FALSE to let the default
    // handler run. Also, don't try to handle two events simultaneously,
    // because we might end up sending the wrong |event_selection_| back to GTK.
    if (!host_view->selection_text_.empty() ||
         host_view->event_selection_active_)
      return FALSE;

    host_view->event_selection_ = *event;
    host_view->event_selection_active_ = true;
    if (host_view->selection_text_.empty())
      host_view->RequestSelectionText();

    return TRUE;
  }

  static void OnSelectionGet(GtkWidget* widget,
                             GtkSelectionData* data,
                             guint info, guint time,
                             RenderWidgetHostViewGtk* host_view) {
    DCHECK(!host_view->selection_text_.empty() ||
           host_view->event_selection_active_);

    gtk_selection_data_set(data, data->target, 8,
        reinterpret_cast<const guchar*>(host_view->selection_text_.c_str()),
        host_view->selection_text_.length());
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

static gboolean OnPopupParentFocusOut(GtkWidget* parent, GdkEventFocus* focus,
                                      RenderWidgetHost* host) {
  host->Shutdown();
  return FALSE;
}

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host),
      parent_host_view_(NULL),
      parent_(NULL),
      popup_signal_id_(0),
      activatable_(true),
      is_loading_(false),
      event_selection_active_(false) {
  host_->set_view(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
}

void RenderWidgetHostViewGtk::InitAsChild() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  parent_host_view_ = parent_host_view;
  parent_ = parent_host_view->GetPluginNativeView();
  GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  gtk_container_add(GTK_CONTAINER(popup), view_.get());

  // If we are not activatable, we don't want to grab keyboard input,
  // and webkit will manage our destruction.
  if (activatable_) {
    // Grab all input for the app. If a click lands outside the bounds of the
    // popup, WebKit will notice and destroy us.
    gtk_grab_add(view_.get());
    // We also destroy ourselves if our parent loses focus.
    popup_signal_id_ = g_signal_connect(parent_, "focus-out-event",
        G_CALLBACK(OnPopupParentFocusOut), host_);
    // Our parent widget actually keeps GTK focus within its window, but we have
    // to make the webkit selection box disappear to maintain appearances.
    parent_host_view->Blur();
  }

  gtk_window_set_default_size(GTK_WINDOW(popup),
                              pos.width(), pos.height());
  gtk_widget_show_all(popup);
  gtk_window_move(GTK_WINDOW(popup), pos.x(), pos.y());
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::WasHidden() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  // We rely on our parent GTK container to size us.
}

gfx::NativeView RenderWidgetHostViewGtk::GetPluginNativeView() {
  // TODO(port): We need to pass some widget pointer out here because the
  // renderer echos it back to us when it asks for GetScreenInfo. However, we
  // should probably be passing the top-level window or some such instead.
  return view_.get();
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  if (plugin_window_moves.empty())
    return;

  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Focus() {
  host_->Focus();
}

void RenderWidgetHostViewGtk::Blur() {
  host_->Blur();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGtk::Show() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Hide() {
  NOTIMPLEMENTED();
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  GtkAllocation* alloc = &view_.get()->allocation;
  return gfx::Rect(alloc->x, alloc->y, alloc->width, alloc->height);
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType())
    return;

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::RenderViewGone() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Destroy() {
  // If |parent_| is non-null, we are a popup and we must disconnect from our
  // parent and destroy the popup window.
  if (parent_) {
    if (activatable_) {
      g_signal_handler_disconnect(parent_, popup_signal_id_);
      parent_host_view_->Focus();
    }
    gtk_widget_destroy(gtk_widget_get_parent(view_.get()));
  }

  // We need to disconnect ourselves from our parent widget at this time; this
  // does the right thing, automatically removing ourselves from our parent
  // container.
  view_.Destroy();
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(), WideToUTF8(tooltip_text).c_str());
  }
}

void RenderWidgetHostViewGtk::SelectionChanged() {
  selection_text_.clear();

  guint32 timestamp = gdk_x11_get_server_time(view_.get()->window);
  gtk_selection_owner_set(view_.get(), GDK_SELECTION_PRIMARY, timestamp);
}

void RenderWidgetHostViewGtk::SetSelectionText(const std::string& text) {
  selection_text_ = text;
  DCHECK(event_selection_active_);
  event_selection_active_ = false;
  // Resume normal handling of the active selection_request_event.
  GtkWidgetClass* klass = GTK_WIDGET_CLASS(gtk_type_class(GTK_TYPE_WIDGET));
  klass->selection_request_event(view_.get(), &event_selection_);
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  Display* display = x11_util::GetXDisplay();
  void* visual = x11_util::GetVisualFromGtkWidget(view_.get());
  XID root_window = x11_util::GetX11RootWindow();
  bool use_render = x11_util::QueryRenderSupport(display);
  bool use_shared_memory = x11_util::QuerySharedMemorySupport(display);
  int depth = gtk_widget_get_visual(view_.get())->depth;

  return new BackingStore(size, display, depth, visual, root_window,
                          use_render, use_shared_memory);
}

void RenderWidgetHostViewGtk::PasteFromSelectionClipboard() {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_request_text(x_clipboard, ReceivedSelectionText, this);
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  BackingStore* backing_store = host_->GetBackingStore();

  GdkWindow* window = view_.get()->window;
  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      backing_store->ShowRect(
          damage_rect, x11_util::GetX11WindowFromGtkWidget(view_.get()));
    }
  } else {
    if (window)
      gdk_window_clear(window);
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHostViewGtk::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!view_.get()->window)
    return;

  GdkCursor* gdk_cursor;
  switch(current_cursor_.GetCursorType()) {
    case GDK_CURSOR_IS_PIXMAP:
      // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
      // that calling gdk_window_set_cursor repeatedly is expensive.  We should
      // avoid it here where possible.
      gdk_cursor = current_cursor_.GetCustomCursor();
      break;

    case GDK_LAST_CURSOR:
      if (is_loading_) {
        // Use MOZ_CURSOR_SPINNING if we are showing the default cursor and
        // the page is loading.
        static const GdkColor fg = { 0, 0, 0, 0 };
        static const GdkColor bg = { 65535, 65535, 65535, 65535 };
        GdkPixmap* source =
            gdk_bitmap_create_from_data(NULL, moz_spinning_bits, 32, 32);
        GdkPixmap* mask =
            gdk_bitmap_create_from_data(NULL, moz_spinning_mask_bits, 32, 32);
        gdk_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 2, 2);
        g_object_unref(source);
        g_object_unref(mask);
      } else {
        gdk_cursor = NULL;
      }
      break;

    default:
      gdk_cursor = gdk_cursor_new(current_cursor_.GetCursorType());
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
  // The window now owns the cursor.
  if (gdk_cursor)
    gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::RequestSelectionText() {
  host_->Send(new ViewMsg_RequestSelectionText(host_->routing_id()));
}

void RenderWidgetHostViewGtk::ReceivedSelectionText(GtkClipboard* clipboard,
    const gchar* text, gpointer userdata) {
  RenderWidgetHostViewGtk* host_view =
      reinterpret_cast<RenderWidgetHostViewGtk*>(userdata);
  host_view->host_->Send(new ViewMsg_InsertText(host_view->host_->routing_id(),
                                                UTF8ToUTF16(text)));
}
