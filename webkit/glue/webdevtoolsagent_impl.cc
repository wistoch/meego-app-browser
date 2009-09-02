// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "EventListener.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptObject.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_delegate.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webview_impl.h"

using WebCore::Document;
using WebCore::InspectorBackend;
using WebCore::InspectorController;
using WebCore::InspectorFrontend;
using WebCore::InspectorResource;
using WebCore::Node;
using WebCore::Page;
using WebCore::SafeAllocation;
using WebCore::ScriptObject;
using WebCore::ScriptState;
using WebCore::ScriptValue;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8DOMWrapper;
using WebCore::V8Proxy;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebURLRequest;


namespace {

void InspectorBackendWeakReferenceCallback(v8::Persistent<v8::Value> object,
                                           void* parameter) {
  InspectorBackend* backend = static_cast<InspectorBackend*>(parameter);
  backend->deref();
  object.Dispose();
}

} //  namespace

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentDelegate* delegate)
    : host_id_(delegate->GetHostId()),
      delegate_(delegate),
      web_view_impl_(web_view_impl),
      attached_(false) {
  debugger_agent_delegate_stub_.set(new DebuggerAgentDelegateStub(this));
  tools_agent_delegate_stub_.set(new ToolsAgentDelegateStub(this));
  tools_agent_native_delegate_stub_.set(new ToolsAgentNativeDelegateStub(this));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DebuggerAgentManager::OnWebViewClosed(web_view_impl_);
  DisposeUtilityContext();
}

void WebDevToolsAgentImpl::DisposeUtilityContext() {
  if (!utility_context_.IsEmpty()) {
    utility_context_.Dispose();
    utility_context_.Clear();
  }
}

void WebDevToolsAgentImpl::UnhideResourcesPanelIfNecessary() {
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->ensureResourceTrackingSettingsLoaded();
  String command = String::format("[\"setResourcesPanelEnabled\", %s]",
      ic->resourceTrackingEnabled() ? "true" : "false");
  tools_agent_delegate_stub_->DispatchOnClient(command);
}

void WebDevToolsAgentImpl::Attach() {
  if (attached_) {
    return;
  }
  debugger_agent_impl_.set(
      new DebuggerAgentImpl(web_view_impl_,
                            debugger_agent_delegate_stub_.get(),
                            this));
  ResetInspectorFrontendProxy();
  UnhideResourcesPanelIfNecessary();
  // Allow controller to send messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setWindowVisible(true, false);
  attached_ = true;
}

void WebDevToolsAgentImpl::Detach() {
  // Prevent controller from sending messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->hideHighlight();
  ic->close();
  DisposeUtilityContext();
  inspector_frontend_script_state_.clear();
  devtools_agent_host_.set(NULL);
  debugger_agent_impl_.set(NULL);
  attached_ = false;
}

void WebDevToolsAgentImpl::OnNavigate() {
  DebuggerAgentManager::OnNavigate();
}

void WebDevToolsAgentImpl::DidCommitLoadForFrame(
    WebViewImpl* webview,
    WebFrame* frame,
    bool is_new_navigation) {
  if (!attached_) {
    return;
  }
  WebDataSource* ds = frame->dataSource();
  const WebURLRequest& request = ds->request();
  GURL url = ds->hasUnreachableURL() ?
      ds->unreachableURL() :
      request.url();
  if (webview->GetMainFrame() == frame) {
    ResetInspectorFrontendProxy();
    tools_agent_delegate_stub_->FrameNavigate(
        url.possibly_invalid_spec());
  }
  UnhideResourcesPanelIfNecessary();
}

void WebDevToolsAgentImpl::WindowObjectCleared(WebFrameImpl* webframe) {
  DebuggerAgentManager::SetHostId(webframe, host_id_);
  if (attached_) {
    // Push context id into the client if it is already attached.
    debugger_agent_delegate_stub_->SetContextId(host_id_);
  }
}

void WebDevToolsAgentImpl::ForceRepaint() {
  delegate_->ForceRepaint();
}

void WebDevToolsAgentImpl::ExecuteUtilityFunction(
      int call_id,
      const String& function_name,
      const String& json_args) {
  String result;
  String exception;
  result = debugger_agent_impl_->ExecuteUtilityFunction(utility_context_,
      function_name, json_args, &exception);
  tools_agent_delegate_stub_->DidExecuteUtilityFunction(call_id,
      result, exception);
}

void WebDevToolsAgentImpl::GetResourceContent(
    int call_id,
    int identifier) {
  String content;
  Page* page = web_view_impl_->page();
  if (page) {
    RefPtr<InspectorResource> resource =
        page->inspectorController()->resources().get(identifier);
    if (resource.get()) {
      content = resource->sourceString();
    }
  }
  tools_agent_native_delegate_stub_->DidGetResourceContent(call_id, content);
}

void WebDevToolsAgentImpl::DispatchMessageFromClient(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& param1,
    const std::string& param2,
    const std::string& param3) {
  if (ToolsAgentDispatch::Dispatch(
      this, class_name, method_name, param1, param2, param3)) {
    return;
  }

  if (!attached_) {
    return;
  }

  if (debugger_agent_impl_.get() &&
      DebuggerAgentDispatch::Dispatch(
          debugger_agent_impl_.get(), class_name, method_name,
          param1, param2, param3)) {
    return;
  }
}

void WebDevToolsAgentImpl::InspectElement(int x, int y) {
  Node* node = web_view_impl_->GetNodeForWindowPos(x, y);
  if (!node) {
    return;
  }
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->inspect(node);
}

void WebDevToolsAgentImpl::SendRpcMessage(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& param1,
    const std::string& param2,
    const std::string& param3) {
  delegate_->SendMessageToClient(class_name, method_name, param1, param2,
      param3);
}

void WebDevToolsAgentImpl::InitDevToolsAgentHost() {
  devtools_agent_host_.set(
      new BoundObject(utility_context_, this, "DevToolsAgentHost"));
  devtools_agent_host_->AddProtoFunction(
      "dispatch",
      WebDevToolsAgentImpl::JsDispatchOnClient);
  devtools_agent_host_->Build();

  v8::HandleScope scope;
  v8::Context::Scope utility_scope(utility_context_);
  // Call custom code to create inspector backend wrapper in the utility context
  // instead of calling V8DOMWrapper::convertToV8Object that would create the
  // wrapper in the Page main frame context.
  v8::Handle<v8::Object> backend_wrapper = CreateInspectorBackendV8Wrapper();
  if (backend_wrapper.IsEmpty()) {
    return;
  }
  utility_context_->Global()->Set(
      v8::String::New("InspectorController"),
      backend_wrapper);
}

v8::Local<v8::Object> WebDevToolsAgentImpl::CreateInspectorBackendV8Wrapper() {
  V8ClassIndex::V8WrapperType descriptorType = V8ClassIndex::INSPECTORBACKEND;
  v8::Handle<v8::Function> function =
      V8DOMWrapper::getTemplate(descriptorType)->GetFunction();
  if (function.IsEmpty()) {
    // Return if allocation failed.
    return v8::Local<v8::Object>();
  }
  v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
  if (instance.IsEmpty()) {
    // Avoid setting the wrapper if allocation failed.
    return v8::Local<v8::Object>();
  }
  InspectorBackend* backend =
      web_view_impl_->page()->inspectorController()->inspectorBackend();
  V8DOMWrapper::setDOMWrapper(instance, V8ClassIndex::ToInt(descriptorType),
                              backend);
  // Create a weak reference to the v8 wrapper of InspectorBackend to deref
  // InspectorBackend when the wrapper is garbage collected.
  backend->ref();
  v8::Persistent<v8::Object> weak_handle =
      v8::Persistent<v8::Object>::New(instance);
  weak_handle.MakeWeak(backend, &InspectorBackendWeakReferenceCallback);
  return instance;
}

void WebDevToolsAgentImpl::ResetInspectorFrontendProxy() {
  DisposeUtilityContext();
  debugger_agent_impl_->CreateUtilityContext(
      web_view_impl_->page()->mainFrame(),
      &utility_context_);
  InitDevToolsAgentHost();

  v8::HandleScope scope;
  v8::Context::Scope context_scope(utility_context_);
  inspector_frontend_script_state_.set(new ScriptState(
      web_view_impl_->page()->mainFrame(),
      utility_context_));
  v8::Handle<v8::Object> injected_script = v8::Local<v8::Object>::Cast(
      utility_context_->Global()->Get(v8::String::New("InjectedScript")));
  ScriptState* state = inspector_frontend_script_state_.get();
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setFrontendProxyObject(
      state,
      ScriptObject(state, utility_context_->Global()),
      ScriptObject(state, injected_script));
}

// static
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsDispatchOnClient(
    const v8::Arguments& args) {
  v8::TryCatch exception_catcher;
  String message = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (message.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  agent->tools_agent_delegate_stub_->DispatchOnClient(message);
  return v8::Undefined();
}

// static
void WebDevToolsAgent::ExecuteDebuggerCommand(
    const std::string& command,
    int caller_id) {
  DebuggerAgentManager::ExecuteDebuggerCommand(command, caller_id);
}

// static
void WebDevToolsAgent::SetMessageLoopDispatchHandler(
    MessageLoopDispatchHandler handler) {
  DebuggerAgentManager::SetMessageLoopDispatchHandler(handler);
}
