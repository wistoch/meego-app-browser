// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_
#define WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

#include "v8/include/v8-debug.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"

namespace WebCore {
class PageGroupLoadDeferrer;
}

namespace WebKit {
class WebFrameImpl;
class WebViewImpl;
}

class DebuggerAgentImpl;
class DictionaryValue;

// There is single v8 instance per render process. Also there may be several
// RenderViews and consequently devtools agents in the process that want to talk
// to the v8 debugger. This class coordinates communication between the debug
// agents and v8 debugger. It will set debug output handler as long as at least
// one debugger agent is attached and remove it when last debugger agent is
// detached. When message is received from debugger it will route it to the
// right debugger agent if there is one otherwise the message will be ignored.
//
// v8 may send a message(e.g. exception event) after which it
// would expect some actions from the handler. If there is no appropriate
// debugger agent to handle such messages the manager will perform the action
// itself, otherwise v8 may hang waiting for the action.
class DebuggerAgentManager : public Noncopyable {
 public:
  static void DebugAttach(DebuggerAgentImpl* debugger_agent);
  static void DebugDetach(DebuggerAgentImpl* debugger_agent);
  static void DebugBreak(DebuggerAgentImpl* debugger_agent);
  static void DebugCommand(const WebCore::String& command);

  static void ExecuteDebuggerCommand(const WebCore::String& command,
                                     int caller_id);
  static void SetMessageLoopDispatchHandler(
      WebKit::WebDevToolsAgent::MessageLoopDispatchHandler handler);

  // Sets |host_id| as the frame context data. This id is used to filter scripts
  // related to the inspected page.
  static void SetHostId(WebKit::WebFrameImpl* webframe, int host_id);

  static void OnWebViewClosed(WebKit::WebViewImpl* webview);

  static void OnNavigate();

  class UtilityContextScope : public Noncopyable {
   public:
    UtilityContextScope() {
      ASSERT(!in_utility_context_);
      in_utility_context_ = true;
    }
    ~UtilityContextScope() {
      if (debug_break_delayed_) {
        v8::Debug::DebugBreak();
        debug_break_delayed_ = false;
      }
      in_utility_context_ = false;
    }
  };

 private:
  DebuggerAgentManager();
  ~DebuggerAgentManager();

  static void V8DebugHostDispatchHandler();
  static void OnV8DebugMessage(const v8::Debug::Message& message);
  static void SendCommandToV8(const WebCore::String& cmd,
                              v8::Debug::ClientData* data);
  static void SendContinueCommandToV8();

  static DebuggerAgentImpl* FindAgentForCurrentV8Context();
  static DebuggerAgentImpl* DebuggerAgentForHostId(int host_id);

  typedef HashMap<int, DebuggerAgentImpl*> AttachedAgentsMap;
  static AttachedAgentsMap* attached_agents_map_;

  static WebKit::WebDevToolsAgent::MessageLoopDispatchHandler
      message_loop_dispatch_handler_;
  static bool in_host_dispatch_handler_;
  typedef HashMap<WebKit::WebViewImpl*, WebCore::PageGroupLoadDeferrer*>
      DeferrersMap;
  static DeferrersMap page_deferrers_;

  static bool in_utility_context_;
  static bool debug_break_delayed_;
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEBUGGER_AGENT_MANAGER_H_
