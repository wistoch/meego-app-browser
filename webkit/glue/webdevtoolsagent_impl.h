// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_

#include <string>

#include "base/scoped_ptr.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/dom_agent.h"
#include "webkit/glue/devtools/net_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/webdevtoolsagent.h"

namespace WebCore {
class Document;
class Node;
class String;
}

class DebuggerAgentDelegateStub;
class DebuggerAgentImpl;
class DomAgentImpl;
class NetAgentImpl;
class Value;
class WebDevToolsAgentDelegate;
class WebFrame;
class WebFrame;
class WebViewImpl;

class WebDevToolsAgentImpl
    : public WebDevToolsAgent,
      public ToolsAgent,
      public DevToolsRpc::Delegate {
 public:
  WebDevToolsAgentImpl(WebViewImpl* web_view_impl,
      WebDevToolsAgentDelegate* delegate);
  virtual ~WebDevToolsAgentImpl();

  // ToolsAgent implementation.
  virtual void HighlightDOMNode(int node_id);
  virtual void HideDOMNodeHighlight();
  virtual void EvaluateJavaScript(int call_id, const String& js);
  virtual void ExecuteUtilityFunction(
      int call_id,
      const WebCore::String& function_name,
      int node_id,
      const WebCore::String& json_args);

  // WebDevToolsAgent implementation.
  virtual void Attach();
  virtual void Detach();
  virtual void DispatchMessageFromClient(const std::string& raw_msg);
  virtual void InspectElement(int x, int y);

  // DevToolsRpc::Delegate implementation.
  void SendRpcMessage(const std::string& raw_msg);

  // Methods called by the glue.
  void SetMainFrameDocumentReady(bool ready);
  void DidCommitLoadForFrame(WebViewImpl* webview,
                             WebFrame* frame,
                             bool is_new_navigation);

  NetAgentImpl* net_agent_impl() { return net_agent_impl_.get(); }

 private:
  WebDevToolsAgentDelegate* delegate_;
  WebViewImpl* web_view_impl_;
  WebCore::Document* document_;
  scoped_ptr<DebuggerAgentDelegateStub> debugger_agent_delegate_stub_;
  scoped_ptr<DomAgentDelegateStub> dom_agent_delegate_stub_;
  scoped_ptr<NetAgentDelegateStub> net_agent_delegate_stub_;
  scoped_ptr<ToolsAgentDelegateStub> tools_agent_delegate_stub_;
  scoped_ptr<DebuggerAgentImpl> debugger_agent_impl_;
  scoped_ptr<DomAgentImpl> dom_agent_impl_;
  scoped_ptr<NetAgentImpl> net_agent_impl_;
  bool attached_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgentImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
