// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_

#include <wtf/HashSet.h>

#include "v8.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/webdevtoolsagent.h"

class WebDevToolsAgentImpl;
class WebViewImpl;

namespace WebCore {
class Document;
class Node;
class Page;
class String;
}

class DebuggerAgentImpl : public DebuggerAgent {
 public:
  // Creates utility context with injected js agent.
  static void ResetUtilityContext(WebCore::Document* document,
                                  v8::Persistent<v8::Context>* context);

  DebuggerAgentImpl(WebViewImpl* web_view_impl,
                    DebuggerAgentDelegate* delegate,
                    WebDevToolsAgentImpl* webdevtools_agent);
  virtual ~DebuggerAgentImpl();

  // DebuggerAgent implementation.
  virtual void DebugBreak();
  virtual void GetContextId();

  void DebuggerOutput(const std::string& out);

  // Executes function with the given name in the utility context. Passes node
  // and json args as parameters. Note that the function called must be
  // implemented in the inject.js file.
  WebCore::String ExecuteUtilityFunction(
      v8::Handle<v8::Context> context,
      const WebCore::String& function_name,
      WebCore::Node* node,
      const WebCore::String& json_args,
      WebCore::String* exception);

  WebCore::Page* GetPage();
  WebDevToolsAgentImpl* webdevtools_agent() { return webdevtools_agent_; };

  WebViewImpl* web_view() { return web_view_impl_; }

 private:
  WebViewImpl* web_view_impl_;
  DebuggerAgentDelegate* delegate_;
  WebDevToolsAgentImpl* webdevtools_agent_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_IMPL_H_
