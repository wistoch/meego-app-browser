// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "V8Binding.h"
#include "v8_custom.h"
#include "v8_proxy.h"
#include "v8_utility.h"
#include "base/string_util.h"
#include "base/values.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/devtools/devtools_rpc_js.h"
#include "webkit/glue/devtools/dom_agent.h"
#include "webkit/glue/devtools/net_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsclient_delegate.h"
#include "webkit/glue/webdevtoolsclient_impl.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview_impl.h"

using namespace WebCore;
using WebKit::WebScriptSource;
using WebKit::WebString;

DEFINE_RPC_JS_BOUND_OBJ(DebuggerAgent, DEBUGGER_AGENT_STRUCT,
    DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(DomAgent, DOM_AGENT_STRUCT,
    DomAgentDelegate, DOM_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(NetAgent, NET_AGENT_STRUCT,
    NetAgentDelegate, NET_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT,
    ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

namespace {

class RemoteDebuggerCommandExecutor : public CppBoundClass {
 public:
  RemoteDebuggerCommandExecutor(
      WebDevToolsClientDelegate* delegate,
      WebFrame* frame,
      const std::wstring& classname)
      : delegate_(delegate) {
    BindToJavascript(frame, classname);
    BindMethod("DebuggerCommand",
                &RemoteDebuggerCommandExecutor::DebuggerCommand);
  }
  virtual ~RemoteDebuggerCommandExecutor() {}

  // The DebuggerCommand() function provided to Javascript.
  void DebuggerCommand(const CppArgumentList& args, CppVariant* result) {
    std::string command = args[0].ToString();
    result->SetNull();
    delegate_->SendDebuggerCommandToAgent(command);
  }

 private:
  WebDevToolsClientDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggerCommandExecutor);
};

} //  namespace

// static
HashMap<WebCore::Page*, WebDevToolsClientImpl*>
    WebDevToolsClientImpl::page_to_client_;

// static
v8::Persistent<v8::FunctionTemplate>
    WebDevToolsClientImpl::host_template_;

// static
void WebDevToolsClientImpl::InitBoundObject() {
  if (!host_template_.IsEmpty()) {
    return;
  }
  v8::HandleScope scope;
  v8::Local<v8::FunctionTemplate> local_template =
      v8::FunctionTemplate::New(V8Proxy::CheckNewLegal);
  host_template_ = v8::Persistent<v8::FunctionTemplate>::New(local_template);

  v8::Local<v8::Signature> default_signature =
      v8::Signature::New(host_template_);
  v8::Local<v8::ObjectTemplate> proto = host_template_->PrototypeTemplate();
  InitProtoFunction(proto,
                    "addSourceToFrame",
                    WebDevToolsClientImpl::JsAddSourceToFrame,
                    default_signature);
  InitProtoFunction(proto,
                    "loaded",
                    WebDevToolsClientImpl::JsLoaded,
                    default_signature);
  InitProtoFunction(proto,
                    "search",
                    WebCore::V8Custom::v8InspectorControllerSearchCallback,
                    default_signature);
  InitProtoFunction(proto,
                    "activateWindow",
                    WebDevToolsClientImpl::JsActivateWindow,
                    default_signature);
  host_template_->SetClassName(v8::String::New("DevToolsHost"));
}

// static
void WebDevToolsClientImpl::InitProtoFunction(
    v8::Handle<v8::ObjectTemplate> proto,
    const char* name,
    v8::InvocationCallback callback,
    v8::Handle<v8::Signature> signature) {
  proto->Set(
      v8::String::New(name),
      v8::FunctionTemplate::New(
          callback,
          v8::Handle<v8::Value>(),
          signature),
      static_cast<v8::PropertyAttribute>(v8::DontDelete));
}

// static
WebDevToolsClient* WebDevToolsClient::Create(
    WebView* view,
    WebDevToolsClientDelegate* delegate) {
  return new WebDevToolsClientImpl(static_cast<WebViewImpl*>(view), delegate);
}

WebDevToolsClientImpl::WebDevToolsClientImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsClientDelegate* delegate)
    : web_view_impl_(web_view_impl),
      delegate_(delegate),
      loaded_(false),
      page_(NULL) {
  WebFrameImpl* frame = web_view_impl_->main_frame();

  // Debugger commands should be sent using special method.
  debugger_command_executor_obj_.set(new RemoteDebuggerCommandExecutor(
      delegate, frame, L"RemoteDebuggerCommandExecutor"));
  debugger_agent_obj_.set(new JsDebuggerAgentBoundObj(
      this, frame, L"RemoteDebuggerAgent"));
  dom_agent_obj_.set(new JsDomAgentBoundObj(this, frame, L"RemoteDomAgent"));
  net_agent_obj_.set(new JsNetAgentBoundObj(this, frame, L"RemoteNetAgent"));
  tools_agent_obj_.set(
      new JsToolsAgentBoundObj(this, frame, L"RemoteToolsAgent"));
  page_ = web_view_impl_->page();
  page_to_client_.set(page_, this);
  WebDevToolsClientImpl::InitBoundObject();

  v8::HandleScope scope;
  v8::Handle<v8::Context> frame_context = V8Proxy::GetContext(frame->frame());
  v8::Context::Scope frame_scope(frame_context);

  v8::Local<v8::Function> constructor = host_template_->GetFunction();
  v8::Local<v8::Object> host_obj = SafeAllocation::NewInstance(constructor);

  v8::Handle<v8::Object> global = frame_context->Global();
  global->Set(v8::String::New("DevToolsHost"), host_obj);
}

WebDevToolsClientImpl::~WebDevToolsClientImpl() {
  page_to_client_.remove(page_);
}

void WebDevToolsClientImpl::DispatchMessageFromAgent(
    const std::string& raw_msg) {
  if (!loaded_) {
    pending_incoming_messages_.append(raw_msg);
    return;
  }
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));

  std::string expr;
  if (dom_agent_obj_->Dispatch(*message.get(), &expr)
          || net_agent_obj_->Dispatch(*message.get(), &expr)
          || tools_agent_obj_->Dispatch(*message.get(), &expr)
          || debugger_agent_obj_->Dispatch(*message.get(), &expr)) {
    web_view_impl_->GetMainFrame()->ExecuteScript(
        WebScriptSource(WebString::fromUTF8(expr)));
  }
}

void WebDevToolsClientImpl::SendRpcMessage(const std::string& raw_msg) {
  delegate_->SendMessageToAgent(raw_msg);
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsAddSourceToFrame(
    const v8::Arguments& args) {
  if (args.Length() < 2) {
    return v8::Undefined();
  }

  v8::TryCatch exception_catcher;

  String mime_type = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (mime_type.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  String source_string = WebCore::toWebCoreStringWithNullCheck(args[1]);
  if (source_string.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  Node* node = V8Proxy::DOMWrapperToNode<Node>(args[2]);
  if (!node || !node->attached()) {
    return v8::Undefined();
  }

  Page* page = V8Proxy::retrieveActiveFrame()->page();
  InspectorController* inspectorController = page->inspectorController();
  return WebCore::v8Boolean(inspectorController->
      addSourceToFrame(mime_type, source_string, node));
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsLoaded(
    const v8::Arguments& args) {
  Page* page = V8Proxy::retrieveActiveFrame()->page();
  WebDevToolsClientImpl* client = page_to_client_.get(page);
  client->loaded_ = true;

  // Grant the devtools page the ability to have source view iframes.
  SecurityOrigin* origin = page->mainFrame()->domWindow()->securityOrigin();
  origin->grantUniversalAccess();

  for (Vector<std::string>::iterator it =
           client->pending_incoming_messages_.begin();
       it != client->pending_incoming_messages_.end();
       ++it) {
    client->DispatchMessageFromAgent(*it);
  }
  client->pending_incoming_messages_.clear();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsActivateWindow(
    const v8::Arguments& args) {
  Page* page = V8Proxy::retrieveActiveFrame()->page();
  WebDevToolsClientImpl* client = page_to_client_.get(page);
  client->delegate_->ActivateWindow();
  return v8::Undefined();
}
