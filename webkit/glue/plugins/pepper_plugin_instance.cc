// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_instance.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_event_conversion.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_var.h"

using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebPluginContainer;

namespace pepper {

namespace {

void RectToPPRect(const gfx::Rect& input, PP_Rect* output) {
  *output = PP_MakeRectFromXYWH(input.x(), input.y(),
                                input.width(), input.height());
}

PP_Var GetWindowObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetWindowObject();
}

PP_Var GetOwnerElementObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetOwnerElementObject();
}

bool BindGraphicsDeviceContext(PP_Instance instance_id, PP_Resource device_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->BindGraphicsDeviceContext(device_id);
}

bool IsFullFrame(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->full_frame();
}

const PPB_Instance ppb_instance = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &BindGraphicsDeviceContext,
  &IsFullFrame,
};

}  // namespace

PluginInstance::PluginInstance(PluginDelegate* delegate,
                               PluginModule* module,
                               const PPP_Instance* instance_interface)
    : delegate_(delegate),
      module_(module),
      instance_interface_(instance_interface),
      container_(NULL),
      full_frame_(false),
      find_identifier_(-1) {
  DCHECK(delegate);
  module_->InstanceCreated(this);
  delegate_->InstanceCreated(this);
}

PluginInstance::~PluginInstance() {
  delegate_->InstanceDeleted(this);
  module_->InstanceDeleted(this);
}

// static
const PPB_Instance* PluginInstance::GetInterface() {
  return &ppb_instance;
}

// static
PluginInstance* PluginInstance::FromPPInstance(PP_Instance instance) {
  return reinterpret_cast<PluginInstance*>(instance);
}

PP_Instance PluginInstance::GetPPInstance() {
  return reinterpret_cast<intptr_t>(this);
}

void PluginInstance::Paint(WebCanvas* canvas,
                           const gfx::Rect& plugin_rect,
                           const gfx::Rect& paint_rect) {
  if (device_context_2d_)
    device_context_2d_->Paint(canvas, plugin_rect, paint_rect);
}

void PluginInstance::InvalidateRect(const gfx::Rect& rect) {
  if (!container_ || position_.IsEmpty())
    return;  // Nothing to do.
  if (rect.IsEmpty())
    container_->invalidate();
  else
    container_->invalidateRect(rect);
}

PP_Var PluginInstance::GetWindowObject() {
  if (!container_)
    return PP_MakeVoid();

  WebFrame* frame = container_->element().document().frame();
  if (!frame)
    return PP_MakeVoid();

  return NPObjectToPPVar(frame->windowObject());
}

PP_Var PluginInstance::GetOwnerElementObject() {
  if (!container_)
    return PP_MakeVoid();

  return NPObjectToPPVar(container_->scriptableObjectForElement());
}

bool PluginInstance::BindGraphicsDeviceContext(PP_Resource device_id) {
  if (!device_id) {
    // Special-case clearing the current device.
    if (device_context_2d_) {
      device_context_2d_->BindToInstance(NULL);
      device_context_2d_ = NULL;
      InvalidateRect(gfx::Rect());
    }
    return true;
  }

  scoped_refptr<DeviceContext2D> device_2d =
      Resource::GetAs<DeviceContext2D>(device_id);

  if (device_2d) {
    if (!device_2d->BindToInstance(this))
      return false;  // Can't bind to more than one instance.
    device_context_2d_ = device_2d;
    // BindToInstance will have invalidated the plugin if necessary.
  }

  return true;
}

void PluginInstance::Delete() {
  instance_interface_->Delete(GetPPInstance());

  container_ = NULL;
}

bool PluginInstance::Initialize(WebPluginContainer* container,
                                const std::vector<std::string>& arg_names,
                                const std::vector<std::string>& arg_values,
                                bool full_frame) {
  container_ = container;
  full_frame_ = full_frame;

  if (!instance_interface_->New(GetPPInstance()))
    return false;

  size_t argc = 0;
  scoped_array<const char*> argn(new const char*[arg_names.size()]);
  scoped_array<const char*> argv(new const char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = arg_names[i].c_str();
    argv[argc] = arg_values[i].c_str();
    argc++;
  }

  return instance_interface_->Initialize(GetPPInstance(),
                                         argc, argn.get(), argv.get());
}

bool PluginInstance::HandleDocumentLoad(URLLoader* loader) {
  return instance_interface_->HandleDocumentLoad(GetPPInstance(),
                                                 loader->GetResource());
}

bool PluginInstance::HandleInputEvent(const WebKit::WebInputEvent& event,
                                      WebCursorInfo* cursor_info) {
  scoped_ptr<PP_Event> pp_event(CreatePP_Event(event));
  if (!pp_event.get())
    return false;

  return instance_interface_->HandleEvent(GetPPInstance(), pp_event.get());
}

PP_Var PluginInstance::GetInstanceObject() {
  return instance_interface_->GetInstanceObject(GetPPInstance());
}

void PluginInstance::ViewChanged(const gfx::Rect& position,
                                 const gfx::Rect& clip) {
  position_ = position;
  if (clip.IsEmpty()) {
    // WebKit can give weird (x,y) positions for empty clip rects (since the
    // position technically doesn't matter). But we want to make these
    // consistent since this is given to the plugin, so force everything to 0
    // in the "everything is clipped" case.
    clip_ = gfx::Rect();
  } else {
    clip_ = clip;
  }

  PP_Rect pp_position, pp_clip;
  RectToPPRect(position_, &pp_position);
  RectToPPRect(clip_, &pp_clip);
  instance_interface_->ViewChanged(GetPPInstance(), &pp_position, &pp_clip);
}

void PluginInstance::ViewInitiatedPaint() {
  if (device_context_2d_)
    device_context_2d_->ViewInitiatedPaint();
}

void PluginInstance::ViewFlushedPaint() {
  if (device_context_2d_)
    device_context_2d_->ViewFlushedPaint();
}

string16 PluginInstance::GetSelectedText(bool html) {
  // TODO: implement me
  return string16();
}

void PluginInstance::Zoom(float factor, bool text_only) {
  // TODO: implement me
}

bool PluginInstance::SupportsFind() {
  // TODO: implement me
  return false;
}

void PluginInstance::StartFind(const string16& search_text,
                               bool case_sensitive,
                               int identifier) {
  find_identifier_ = identifier;
  // TODO: implement me
}

void PluginInstance::SelectFindResult(bool forward) {
  // TODO: implement me
}

void PluginInstance::StopFind() {
  find_identifier_ = -1;
  // TODO: implement me
}

}  // namespace pepper
