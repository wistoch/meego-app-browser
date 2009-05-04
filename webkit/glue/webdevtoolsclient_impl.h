// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_

#include <string>

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

#include "v8.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/webdevtoolsclient.h"

namespace WebCore {
class Page;
class String;
}

class JsDebuggerAgentBoundObj;
class JsDomAgentBoundObj;
class JsNetAgentBoundObj;
class JsToolsAgentBoundObj;
class WebDevToolsClientDelegate;
class WebViewImpl;

class WebDevToolsClientImpl : public WebDevToolsClient,
                              public CppBoundClass,
                              public DevToolsRpc::Delegate {
 public:
  WebDevToolsClientImpl(
      WebViewImpl* web_view_impl,
      WebDevToolsClientDelegate* delegate);
  virtual ~WebDevToolsClientImpl();

  // DevToolsRpc::Delegate implementation.
  virtual void SendRpcMessage(const std::string& raw_msg);

  // WebDevToolsClient implementation.
  virtual void DispatchMessageFromAgent(const std::string& raw_msg);

 private:
  static v8::Handle<v8::Value> JsAddSourceToFrame(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsLoaded(const v8::Arguments& args);
  static v8::Persistent<v8::FunctionTemplate> host_template_;
  static HashMap<WebCore::Page*, WebDevToolsClientImpl*> page_to_client_;

  static void InitBoundObject();

  WebViewImpl* web_view_impl_;
  WebDevToolsClientDelegate* delegate_;
  OwnPtr<CppBoundClass> debugger_command_executor_obj_;
  OwnPtr<JsDebuggerAgentBoundObj> debugger_agent_obj_;
  OwnPtr<JsDomAgentBoundObj> dom_agent_obj_;
  OwnPtr<JsNetAgentBoundObj> net_agent_obj_;
  OwnPtr<JsToolsAgentBoundObj> tools_agent_obj_;
  bool loaded_;
  Vector<std::string> pending_incoming_messages_;
  WebCore::Page* page_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
