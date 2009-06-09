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
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/devtools/devtools_rpc_js.h"
#include "webkit/glue/devtools/dom_agent.h"
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
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT,
    ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

class ToolsAgentNativeDelegateImpl : public ToolsAgentNativeDelegate {
 public:
  struct ResourceContentRequestData {
    String mime_type;
    RefPtr<Node> frame;
  };

  ToolsAgentNativeDelegateImpl(WebFrameImpl* frame) : frame_(frame) {}
  virtual ~ToolsAgentNativeDelegateImpl() {}

  // ToolsAgentNativeDelegate implementation.
  virtual void DidGetResourceContent(int request_id, const String& content) {
    if (!resource_content_requests_.contains(request_id)) {
      NOTREACHED();
      return;
    }
    ResourceContentRequestData request =
        resource_content_requests_.take(request_id);

    InspectorController* ic = frame_->frame()->page()->inspectorController();

    ic->addSourceToFrame(request.mime_type, content, request.frame.get());
  }

  void RequestSent(int resource_id, String mime_type, Node* frame) {
    ResourceContentRequestData data;
    data.mime_type = mime_type;
    data.frame = frame;
    DCHECK(!resource_content_requests_.contains(resource_id));
    resource_content_requests_.set(resource_id, data);
  }

 private:
  WebFrameImpl* frame_;
  HashMap<int, ResourceContentRequestData> resource_content_requests_;
  DISALLOW_COPY_AND_ASSIGN(ToolsAgentNativeDelegateImpl);
};

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
      loaded_(false) {
  WebFrameImpl* frame = web_view_impl_->main_frame();

  // Debugger commands should be sent using special method.
  debugger_command_executor_obj_.set(new RemoteDebuggerCommandExecutor(
      delegate, frame, L"RemoteDebuggerCommandExecutor"));
  debugger_agent_obj_.set(new JsDebuggerAgentBoundObj(
      this, frame, L"RemoteDebuggerAgent"));
  dom_agent_obj_.set(new JsDomAgentBoundObj(this, frame, L"RemoteDomAgent"));
  tools_agent_obj_.set(
      new JsToolsAgentBoundObj(this, frame, L"RemoteToolsAgent"));
  tools_agent_native_delegate_impl_.set(
      new ToolsAgentNativeDelegateImpl(frame));

  v8::HandleScope scope;
  v8::Handle<v8::Context> frame_context = V8Proxy::GetContext(frame->frame());
  dev_tools_host_.set(new BoundObject(frame_context, this, "DevToolsHost"));
  dev_tools_host_->AddProtoFunction(
      "addSourceToFrame",
      WebDevToolsClientImpl::JsAddSourceToFrame);
  dev_tools_host_->AddProtoFunction(
      "addResourceSourceToFrame",
      WebDevToolsClientImpl::JsAddResourceSourceToFrame);
  dev_tools_host_->AddProtoFunction(
      "loaded",
      WebDevToolsClientImpl::JsLoaded);
  dev_tools_host_->AddProtoFunction(
      "search",
      WebCore::V8Custom::v8InspectorControllerSearchCallback);
  dev_tools_host_->AddProtoFunction(
      "activateWindow",
      WebDevToolsClientImpl::JsActivateWindow);
  dev_tools_host_->Build();
}

WebDevToolsClientImpl::~WebDevToolsClientImpl() {
}

void WebDevToolsClientImpl::DispatchMessageFromAgent(
      const std::string& class_name,
      const std::string& method_name,
      const std::string& raw_msg) {
  if (ToolsAgentNativeDelegateDispatch::Dispatch(
          tools_agent_native_delegate_impl_.get(),
          class_name,
          method_name,
          raw_msg)) {
    return;
  }

  std::string expr = StringPrintf(
      "devtools.dispatch('%s','%s',%s)",
      class_name.c_str(),
      method_name.c_str(),
      raw_msg.c_str());
  if (!loaded_) {
    pending_incoming_messages_.append(expr);
    return;
  }
  ExecuteScript(expr);
}

void WebDevToolsClientImpl::AddResourceSourceToFrame(int resource_id,
                                                     String mime_type,
                                                     Node* frame) {
  tools_agent_obj_->GetResourceContent(resource_id, resource_id);
  tools_agent_native_delegate_impl_->RequestSent(resource_id, mime_type, frame);
}

void WebDevToolsClientImpl::ExecuteScript(const std::string& expr) {
  web_view_impl_->GetMainFrame()->ExecuteScript(
      WebScriptSource(WebString::fromUTF8(expr)));
}


void WebDevToolsClientImpl::SendRpcMessage(const std::string& class_name,
                                           const std::string& method_name,
                                           const std::string& raw_msg) {
  delegate_->SendMessageToAgent(class_name, method_name, raw_msg);
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

  Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
  InspectorController* inspectorController = page->inspectorController();
  return WebCore::v8Boolean(inspectorController->
      addSourceToFrame(mime_type, source_string, node));
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsAddResourceSourceToFrame(
    const v8::Arguments& args) {
  int resource_id = static_cast<int>(args[0]->NumberValue());
  String mime_type = WebCore::toWebCoreStringWithNullCheck(args[1]);
  if (mime_type.isEmpty()) {
    return v8::Undefined();
  }
  Node* node = V8Proxy::DOMWrapperToNode<Node>(args[2]);
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->AddResourceSourceToFrame(resource_id, mime_type, node);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsLoaded(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->loaded_ = true;

  // Grant the devtools page the ability to have source view iframes.
  Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
  SecurityOrigin* origin = page->mainFrame()->domWindow()->securityOrigin();
  origin->grantUniversalAccess();

  for (Vector<std::string>::iterator it =
           client->pending_incoming_messages_.begin();
       it != client->pending_incoming_messages_.end();
       ++it) {
    client->ExecuteScript(*it);
  }
  client->pending_incoming_messages_.clear();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsActivateWindow(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->delegate_->ActivateWindow();
  return v8::Undefined();
}
