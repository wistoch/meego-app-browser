// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HACK: we need this #define in place before npapi.h is included for
// plugins to work. However, all sorts of headers include npapi.h, so
// the only way to be certain the define is in place is to put it
// here.  You might ask, "Why not set it in npapi.h directly, or in
// this directory's SConscript, then?"  but it turns out this define
// makes npapi.h include Xlib.h, which in turn defines a ton of symbols
// like None and Status, causing conflicts with the aforementioned
// many headers that include npapi.h.  Ugh.
// See also plugin_host.cc.
#define MOZ_X11 1

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
// #include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

WebPluginDelegate* WebPluginDelegate::Create(
    const FilePath& filename,
    const std::string& mime_type,
    gfx::NativeView containing_view) {
  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin.get() == NULL)
    return NULL;

  NPError err = plugin->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin->CreateInstance(mime_type);
  return new WebPluginDelegateImpl(containing_view, instance.get());
}

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::NativeView containing_view,
    NPAPI::PluginInstance *instance)
    :
      windowed_handle_(0),
      windowed_did_set_window_(false),
      windowless_(false),
      plugin_(NULL),
      instance_(instance),
      pixmap_(NULL),
      parent_(containing_view),
      quirks_(0)
 {
  memset(&window_, 0, sizeof(window_));

}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();

  if (!windowless_)
    WindowedDestroyWindow();

  if (window_.ws_info) {
    // We only ever use ws_info as an NPSetWindowCallbackStruct.
    delete static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  }

  if (pixmap_) {
    g_object_unref(gdk_drawable_get_colormap(pixmap_));
    g_object_unref(pixmap_);
    pixmap_ = NULL;
  }
}

void WebPluginDelegateImpl::PluginDestroyed() {
  delete this;
}

bool WebPluginDelegateImpl::Initialize(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebPlugin* plugin,
                                       bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin);
  NPAPI::PluginInstance* old_instance =
      NPAPI::PluginInstance::SetInitializingInstance(instance_);

  bool start_result = instance_->Start(url, argn, argv, argc, load_manually);

  NPAPI::PluginInstance::SetInitializingInstance(old_instance);

  if (!start_result)
    return false;

  windowless_ = instance_->windowless();
  if (windowless_) {
    // For windowless plugins we should set the containing window handle
    // as the instance window handle. This is what Safari does. Not having
    // a valid window handle causes subtle bugs with plugins which retreive
    // the window handle and validate the same. The window handle can be
    // retreived via NPN_GetValue of NPNVnetscapeWindow.
    // instance_->set_window_handle(parent_);
    // CreateDummyWindowForActivation();
    // handle_event_pump_messages_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  } else {
    if (!WindowedCreatePlugin())
      return false;
  }

  plugin->SetWindow(windowed_handle_);
  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegateImpl::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    // TODO(evanm): I played with this for quite a while but couldn't
    // figure out a way to make Flash not crash unless I didn't call
    // NPP_SetWindow.  Perhaps it just should be marked with the quirk
    // that wraps the NPP_SetWindow call.
    // window_.window = NULL;
    // if (!(quirks_ & PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY)) {
    //   instance_->NPP_SetWindow(&window_);
    // }

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }
}

void WebPluginDelegateImpl::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (windowless_) {
    WindowlessUpdateGeometry(window_rect, clip_rect);
  } else {
    WindowedUpdateGeometry(window_rect, clip_rect);
  }
}

void WebPluginDelegateImpl::Paint(cairo_surface_t* context,
                                  const gfx::Rect& rect) {
  if (windowless_)
    WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(cairo_surface_t* context) {
  NOTIMPLEMENTED();
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(NPReason reason) {
  instance()->DidFinishLoadWithReason(reason);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

void WebPluginDelegateImpl::SendJavaScriptStream(const std::string& url,
                                                 const std::wstring& result,
                                                 bool success,
                                                 bool notify_needed,
                                                 intptr_t notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegateImpl::DidReceiveManualResponse(
    const std::string& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  if (!windowless_) {
    // Calling NPP_WriteReady before NPP_SetWindow causes movies to not load in
    // Flash.  See http://b/issue?id=892174.
    DCHECK(windowed_did_set_window_);
  }

  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegateImpl::DidReceiveManualData(const char* buffer,
                                                 int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegateImpl::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegateImpl::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegateImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  /* XXX NPEvent evt;
  evt.event = PluginInstallerImpl::kInstallMissingPluginMessage;
  evt.lParam = 0;
  evt.wParam = 0;
  instance()->NPP_HandleEvent(&evt); */
}

void WebPluginDelegateImpl::WindowedUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (WindowedReposition(window_rect, clip_rect) ||
      !windowed_did_set_window_) {
    // Let the plugin know that it has been moved
    WindowedSetWindow();
  }
}

namespace {

// This is just a GtkSocket, with size_request overridden, so that we always
// control the size of the widget.
class GtkFixedSocket {
 public:
  // Create a new instance of our GTK widget object.
  static GtkWidget* CreateNewWidget() {
    return GTK_WIDGET(g_object_new(GetType(), NULL));
  }

 private:
  // Create and register our custom container type with GTK.
  static GType GetType() {
    static GType type = 0;  // We only want to register our type once.
    if (!type) {
      static const GTypeInfo info = {
        sizeof(GtkSocketClass),
        NULL, NULL,
        static_cast<GClassInitFunc>(&ClassInit),
        NULL, NULL,
        sizeof(GtkSocket),  // We are identical to a GtkSocket.
        0, NULL,
      };
      type = g_type_register_static(GTK_TYPE_SOCKET,
                                    "GtkFixedSocket",
                                    &info,
                                    static_cast<GTypeFlags>(0));
    }
    return type;
  }

  // Implementation of the class initializer.
  static void ClassInit(gpointer klass, gpointer class_data_unusued) {
    GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(klass);
    widget_class->size_request = &HandleSizeRequest;
  }

  // Report our allocation size during size requisition.  This means we control
  // the size, from calling gtk_widget_size_allocate in WindowedReposition().
  static void HandleSizeRequest(GtkWidget* widget,
                                GtkRequisition* requisition) {
    requisition->width = widget->allocation.width;
    requisition->height = widget->allocation.height;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(GtkFixedSocket);
};

gboolean PlugRemovedCallback(GtkSocket* socket) {
  // This is called when the other side of the socket goes away.
  // We return TRUE to indicate that we don't want to destroy our side.
  return TRUE;
}

}  // namespace

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);

  bool xembed;
  NPError err = instance_->NPP_GetValue(NPPVpluginNeedsXEmbed, &xembed);
  DCHECK(err == NPERR_NO_ERROR);
  if (!xembed) {
    NOTIMPLEMENTED() << "Windowed plugin but without xembed.";
    return false;
  }

  windowed_handle_ = GtkFixedSocket::CreateNewWidget();
  g_signal_connect(GTK_SOCKET(windowed_handle_), "plug-removed",
                   G_CALLBACK(PlugRemovedCallback), NULL);
  gtk_container_add(GTK_CONTAINER(parent_), windowed_handle_);
  // TODO(evanm): connect to signals on the socket, like when the other side
  // goes away.

  gtk_widget_show(windowed_handle_);
  gtk_widget_realize(windowed_handle_);

  window_.window = GINT_TO_POINTER(
      gtk_socket_get_id(GTK_SOCKET(windowed_handle_)));

  if (!window_.ws_info)
    window_.ws_info = new NPSetWindowCallbackStruct;
  NPSetWindowCallbackStruct* extra =
      static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  extra->display = GDK_WINDOW_XDISPLAY(windowed_handle_->window);
  GdkVisual* visual = gdk_drawable_get_visual(windowed_handle_->window);
  extra->visual = GDK_VISUAL_XVISUAL(visual);
  extra->depth = visual->depth;
  extra->colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(windowed_handle_->window));

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (windowed_handle_ != NULL) {
    gtk_widget_destroy(windowed_handle_);
    windowed_handle_ = NULL;
  }
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (!windowed_handle_) {
    NOTREACHED();
    return false;
  }

  if (window_rect_ == window_rect && clip_rect_ == clip_rect)
    return false;


  if (window_rect.size() != window_rect_.size()) {
    // Clipping is handled by WebPlugin.
    GtkAllocation allocation = { window_rect.x(), window_rect.y(),
                                 window_rect.width(), window_rect.height() };
    // TODO(deanm): we probably want to match Windows here, where x and y is
    // fixed at 0, and we're just sizing the window.
    // Tell our parent GtkFixed container where to place the widget.
    gtk_fixed_move(
        GTK_FIXED(parent_), windowed_handle_, window_rect.x(), window_rect.y());
    gtk_widget_size_allocate(windowed_handle_, &allocation);
  }

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  // TODO(deanm): Is this really needed?
  // Ensure that the entire window gets repainted.
  gtk_widget_queue_draw(windowed_handle_);

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  // XXX instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();

  //window_.window = windowed_handle_;
  window_.type = NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // Set this flag before entering the instance in case of side-effects.
  windowless_needs_set_window_ = true;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    WindowlessSetWindow(true);
  }
}

void WebPluginDelegateImpl::EnsurePixmapAtLeastSize(int width, int height) {
  if (pixmap_) {
    gint cur_width, cur_height;
    gdk_drawable_get_size(pixmap_, &cur_width, &cur_height);
    if (cur_width >= width && cur_height >= height)
      return;  // We are already the appropriate size.

    // Otherwise, we need to recreate ourselves.
    g_object_unref(gdk_drawable_get_colormap(pixmap_));
    g_object_unref(pixmap_);
    pixmap_ = NULL;
  }

  // |sys_visual| is owned by gdk; we shouldn't free it.
  GdkVisual* sys_visual = gdk_visual_get_system();
  pixmap_ = gdk_pixmap_new(NULL,  // use width/height/depth params
                           width, height, sys_visual->depth);
  GdkColormap* colormap = gdk_colormap_new(gdk_visual_get_system(),
                                           FALSE);
  gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap_), colormap);
}

#ifdef DEBUG_RECTANGLES
namespace {

// Draw a rectangle on a Cairo surface.
// Useful for debugging various rectangles involved in drawing plugins.
void DrawDebugRectangle(cairo_surface_t* surface,
                        const gfx::Rect& rect,
                        float r, float g, float b) {
  cairo_t* cairo = cairo_create(surface);
  cairo_set_source_rgba(cairo, r, g, b, 0.5);
  cairo_rectangle(cairo, rect.x(), rect.y(),
                  rect.width(), rect.height());
  cairo_stroke(cairo);
  cairo_destroy(cairo);
}

}  // namespace
#endif

void WebPluginDelegateImpl::WindowlessPaint(cairo_surface_t* context,
                                            const gfx::Rect& damage_rect) {
  // Compare to:
  // http://mxr.mozilla.org/firefox/source/layout/generic/nsObjectFrame.cpp:
  // nsPluginInstanceOwner::Renderer::NativeDraw().

  DCHECK(context);

  // We need to pass the DC to the plugin via NPP_SetWindow in the
  // first paint to ensure that it initiates rect invalidations.
  // TODO(evanm): for now, it appears we always need to do this.
  if (true)
    windowless_needs_set_window_ = true;

  // TODO(darin): we should avoid calling NPP_SetWindow here since it may
  // cause page layout to be invalidated.

  // We really don't need to continually call SetWindow.
  // m_needsSetWindow flags when the geometry has changed.
  if (windowless_needs_set_window_)
    WindowlessSetWindow(false);

  // The actual dirty region is just the intersection of the plugin
  // window with the damage region.  However, the plugin wants to draw
  // relative to the containing window's origin, so our pixmap must be
  // from the window's origin down to the bottom-right edge of the
  // dirty region.
  //
  // +-----------------------------+-----------------------------+
  // |                             |                             |
  // |    pixmap     +-------------+                             |
  // |               |   damage    |                window       |
  // |               |             |                             |
  // |       +-------+-------------+----------+                  |
  // |       |       | draw        |          |                  |
  // +-------+-------+-------------+          |                  |
  // |       |                                |                  |
  // |       |        plugin                  |                  |
  // |       +--------------------------------+                  |
  // |                                                           |
  // |                                                           |
  // +-----------------------------------------------------------+
  //
  // TOOD(evanm): on Windows, we instead just translate the origin of
  // the DC that we hand to the plugin.  Does such a thing exist on X?
  // TODO(evanm): make use of the clip rect as well.

  gfx::Rect plugin_rect(window_.x, window_.y, window_.width, window_.height);
  gfx::Rect draw_rect = plugin_rect.Intersect(damage_rect);

  gfx::Rect pixmap_rect(0, 0,
                        draw_rect.x() + draw_rect.width(),
                        draw_rect.y() + draw_rect.height());

  EnsurePixmapAtLeastSize(pixmap_rect.width(), pixmap_rect.height());

  // Copy the current image into the pixmap, so the plugin can draw over
  // this background.
  cairo_t* cairo = gdk_cairo_create(pixmap_);
  cairo_set_source_surface(cairo, context, 0, 0);
  cairo_rectangle(cairo, draw_rect.x(), draw_rect.y(),
                  draw_rect.width(), draw_rect.height());
  cairo_clip(cairo);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  // Construct the paint message, targeting the pixmap.
  XGraphicsExposeEvent event = {0};
  event.type = GraphicsExpose;
  event.display = GDK_DISPLAY();
  event.drawable = GDK_PIXMAP_XID(pixmap_);
  event.x = draw_rect.x();
  event.y = draw_rect.y();
  event.width = draw_rect.width();
  event.height = draw_rect.height();

  // Tell the plugin to paint into the pixmap.
  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);
  NPError err = instance()->NPP_HandleEvent(reinterpret_cast<XEvent*>(&event));
  DCHECK_EQ(err, NPERR_NO_ERROR);

  // Now copy the rendered image pixmap back into the drawing buffer.
  cairo = cairo_create(context);
  gdk_cairo_set_source_pixmap(cairo, pixmap_, 0, 0);
  cairo_rectangle(cairo, draw_rect.x(), draw_rect.y(),
                  draw_rect.width(), draw_rect.height());
  cairo_clip(cairo);
  cairo_paint(cairo);
  cairo_destroy(cairo);

#ifdef DEBUG_RECTANGLES
  // Draw some debugging rectangles.
  // Pixmap rect = blue.
  DrawDebugRectangle(context, pixmap_rect, 0, 0, 1);
  // Drawing rect = red.
  DrawDebugRectangle(context, draw_rect, 1, 0, 0);
#endif
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());
  // Mozilla docs say that this window param is not used for windowless
  // plugins; rather, the window is passed during the GraphicsExpose event.
  DCHECK(window_.window == 0);

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  if (!window_.ws_info)
    window_.ws_info = new NPSetWindowCallbackStruct;
  NPSetWindowCallbackStruct* extra =
      static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  extra->display = GDK_DISPLAY();
  GdkVisual* visual = gdk_visual_get_system();
  extra->visual = GDK_VISUAL_XVISUAL(visual);
  extra->depth = visual->depth;
  GdkColormap* colormap = gdk_colormap_new(gdk_visual_get_system(), FALSE);
  extra->colormap = GDK_COLORMAP_XCOLORMAP(colormap);

  if (!force_set_window)
    windowless_needs_set_window_ = false;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  DCHECK(instance()->windowless());

  NOTIMPLEMENTED();
  /*  NPEvent focus_event;
  focus_event.event = WM_SETFOCUS;
  focus_event.wParam = 0;
  focus_event.lParam = 0;

  instance()->NPP_HandleEvent(&focus_event);*/
}

bool WebPluginDelegateImpl::HandleEvent(NPEvent* event,
                                        WebCursor* cursor) {
  bool ret = instance()->NPP_HandleEvent(event) != 0;

#if 0
  if (event->event == WM_MOUSEMOVE) {
    // Snag a reference to the current cursor ASAP in case the plugin modified
    // it. There is a nasty race condition here with the multiprocess browser
    // as someone might be setting the cursor in the main process as well.
    *cursor = current_windowless_cursor_;
  }
#endif

  return ret;
}

WebPluginResourceClient* WebPluginDelegateImpl::CreateResourceClient(
    int resource_id, const std::string &url, bool notify_needed,
    intptr_t notify_data, intptr_t existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);

    plugin_stream->CancelRequest();

    return plugin_stream->AsResourceClient();
  }

  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(
      resource_id, url, mime_type, notify_needed,
      reinterpret_cast<void*>(notify_data));
  return stream;
}

void WebPluginDelegateImpl::URLRequestRouted(const std::string&url,
                                             bool notify_needed,
                                             intptr_t notify_data) {
  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
}
