// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_webplugin_impl.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_var.h"

using WebKit::WebCanvas;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebRect;
using WebKit::WebVector;

namespace pepper {

WebPluginImpl::WebPluginImpl(
    PluginModule* plugin_module,
    WebKit::WebFrame* frame,
    const WebPluginParams& params,
    const base::WeakPtr<PluginDelegate>& plugin_delegate)
    : init_data_(new InitData()) {
  DCHECK(plugin_module);
  init_data_->module = plugin_module;
  init_data_->delegate = plugin_delegate;
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    init_data_->arg_names.push_back(params.attributeNames[i].utf8());
    init_data_->arg_values.push_back(params.attributeValues[i].utf8());
  }
}

WebPluginImpl::~WebPluginImpl() {
}

bool WebPluginImpl::initialize(WebPluginContainer* container) {
  // The plugin delegate may have gone away.
  if (!init_data_->delegate)
    return false;

  instance_ = init_data_->module->CreateInstance(init_data_->delegate);
  if (!instance_)
    return false;

  bool success = instance_->Initialize(container,
                                       init_data_->arg_names,
                                       init_data_->arg_values);
  if (!success) {
    instance_->Delete();
    instance_ = NULL;
    return false;
  }

  init_data_.reset();
  return true;
}

void WebPluginImpl::destroy() {
  if (instance_) {
    instance_->Delete();
    instance_ = NULL;
  }

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  return GetNPObject(instance_->GetInstanceObject());
}

void WebPluginImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  instance_->Paint(canvas, plugin_rect_, rect);
}

void WebPluginImpl::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  plugin_rect_ = window_rect;
  instance_->ViewChanged(plugin_rect_, clip_rect);
}

void WebPluginImpl::updateFocus(bool focused) {
}

void WebPluginImpl::updateVisibility(bool visible) {
}

bool WebPluginImpl::acceptsInputEvents() {
  return true;
}

bool WebPluginImpl::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  return instance_->HandleInputEvent(event, &cursor_info);
}

void WebPluginImpl::didReceiveResponse(
    const WebKit::WebURLResponse& response) {
}

void WebPluginImpl::didReceiveData(const char* data, int data_length) {
}

void WebPluginImpl::didFinishLoading() {
}

void WebPluginImpl::didFailLoading(const WebKit::WebURLError&) {
}

void WebPluginImpl::didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                                 void* notify_data) {
}

void WebPluginImpl::didFailLoadingFrameRequest(
    const WebKit::WebURL& url,
    void* notify_data,
    const WebKit::WebURLError& error) {
}

}  // namespace pepper
