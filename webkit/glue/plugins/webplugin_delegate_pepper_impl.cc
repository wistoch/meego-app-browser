// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webplugin_delegate_pepper_impl.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;


WebPluginDelegatePepperImpl* WebPluginDelegatePepperImpl::Create(
    const FilePath& filename,
    const std::string& mime_type,
    gfx::PluginWindowHandle containing_view) {
  scoped_refptr<NPAPI::PluginLib> plugin_lib =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin_lib.get() == NULL)
    return NULL;

  NPError err = plugin_lib->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin_lib->CreateInstance(mime_type);
  return new WebPluginDelegatePepperImpl(containing_view, instance.get());
}

bool WebPluginDelegatePepperImpl::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    WebPlugin* plugin,
    bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin_);
  int argc = 0;
  scoped_array<char*> argn(new char*[arg_names.size()]);
  scoped_array<char*> argv(new char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = const_cast<char*>(arg_names[i].c_str());
    argv[argc] = const_cast<char*>(arg_values[i].c_str());
    argc++;
  }

  bool start_result = instance_->Start(
      url, argn.get(), argv.get(), argc, load_manually);
  if (!start_result)
    return false;

  // For windowless plugins we should set the containing window handle
  // as the instance window handle. This is what Safari does. Not having
  // a valid window handle causes subtle bugs with plugins which retreive
  // the window handle and validate the same. The window handle can be
  // retreived via NPN_GetValue of NPNVnetscapeWindow.
  instance_->set_window_handle(parent_);

  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegatePepperImpl::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    window_.window = NULL;
    instance_->NPP_SetWindow(&window_);

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }
}

void WebPluginDelegatePepperImpl::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  WindowlessUpdateGeometry(window_rect, clip_rect);
}

NPObject* WebPluginDelegatePepperImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegatePepperImpl::DidFinishLoadWithReason(
    const GURL& url,
    NPReason reason,
    intptr_t notify_data) {
  instance()->DidFinishLoadWithReason(
      url, reason, reinterpret_cast<void*>(notify_data));
}

int WebPluginDelegatePepperImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

void WebPluginDelegatePepperImpl::SendJavaScriptStream(
    const GURL& url,
    const std::string& result,
    bool success,
    bool notify_needed,
    intptr_t notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegatePepperImpl::DidReceiveManualResponse(
    const GURL& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegatePepperImpl::DidReceiveManualData(const char* buffer,
                                                       int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegatePepperImpl::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegatePepperImpl::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegatePepperImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

WebPluginResourceClient* WebPluginDelegatePepperImpl::CreateResourceClient(
    int resource_id, const GURL& url, bool notify_needed,
    intptr_t notify_data, intptr_t existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);

    return plugin_stream->AsResourceClient();
  }

  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(
      resource_id, url, mime_type, notify_needed,
      reinterpret_cast<void*>(notify_data));
  return stream;
}

bool WebPluginDelegatePepperImpl::IsPluginDelegateWindow(
    gfx::NativeWindow window) {
  return false;
}

bool WebPluginDelegatePepperImpl::GetPluginNameFromWindow(
    gfx::NativeWindow window, std::wstring *plugin_name) {
  return false;
}

bool WebPluginDelegatePepperImpl::IsDummyActivationWindow(
    gfx::NativeWindow window) {
  return false;
}

WebPluginDelegatePepperImpl::WebPluginDelegatePepperImpl(
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : plugin_(NULL),
      instance_(instance),
      parent_(containing_view) {
  memset(&window_, 0, sizeof(window_));
}

WebPluginDelegatePepperImpl::~WebPluginDelegatePepperImpl() {
  DestroyInstance();
}

void WebPluginDelegatePepperImpl::PluginDestroyed() {
  delete this;
}

void WebPluginDelegatePepperImpl::Paint(gfx::NativeDrawingContext context, const gfx::Rect& rect) {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepperImpl::Print(gfx::NativeDrawingContext context) {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepperImpl::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepperImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;
    WindowlessSetWindow(true);
    // TODO(sehr): update the context here?
  }
}

void WebPluginDelegatePepperImpl::WindowlessPaint(
    gfx::NativeDrawingContext context,
    const gfx::Rect& damage_rect) {
  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);
  // TODO(sehr): save the context here?
}

void WebPluginDelegatePepperImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegatePepperImpl::SetFocus() {
  // TODO(sehr): what to do here?
}

bool WebPluginDelegatePepperImpl::HandleInputEvent(const WebInputEvent& event,
                                                   WebCursorInfo* cursor_info) {
  bool ret = false;
  // TODO(sehr): pass event to plugin.
  // instance()->NPP_HandleEvent(&np_event) != 0;

  return ret;
}
