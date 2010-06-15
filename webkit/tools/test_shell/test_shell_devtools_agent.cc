// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_devtools_agent.h"

#include "base/message_loop.h"
#include "grit/webkit_chromium_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/tools/test_shell/test_shell_devtools_callargs.h"
#include "webkit/tools/test_shell/test_shell_devtools_client.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCString;
using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebView;

// static
void TestShellDevToolsAgent::DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  bool old_state = current->NestableTasksAllowed();
  current->SetNestableTasksAllowed(true);
  current->RunAllPending();
  current->SetNestableTasksAllowed(old_state);
}

// Warning at call_method_factory_(this) treated as error is switched off.

#if defined(OS_WIN)
#pragma warning(disable : 4355)
#endif // defined(OS_WIN)

TestShellDevToolsAgent::TestShellDevToolsAgent(WebView* web_view)
    : call_method_factory_(this),
      dev_tools_client_(NULL),
      web_view_(web_view) {
  static int dev_tools_agent_counter;
  routing_id_ = ++dev_tools_agent_counter;
  if (routing_id_ == 1)
    WebDevToolsAgent::setMessageLoopDispatchHandler(
        &TestShellDevToolsAgent::DispatchMessageLoop);
  web_dev_tools_agent_ = WebDevToolsAgent::create(web_view_, this);
  web_view_->setDevToolsAgent(web_dev_tools_agent_);
}

void TestShellDevToolsAgent::sendMessageToFrontend(
       const WebDevToolsMessageData& data) {
  if (dev_tools_client_)
    dev_tools_client_->AsyncCall(TestShellDevToolsCallArgs(data));
}

void TestShellDevToolsAgent::forceRepaint() {
}

void TestShellDevToolsAgent::runtimeFeatureStateChanged(
    const WebKit::WebString& feature, bool enabled) {
  // TODO(loislo): implement this.
}

WebCString TestShellDevToolsAgent::injectedScriptSource() {
  base::StringPiece injectjsWebkit =
      webkit_glue::GetDataResource(IDR_DEVTOOLS_INJECT_WEBKIT_JS);
  return WebCString(injectjsWebkit.data(), injectjsWebkit.length());
}

WebCString TestShellDevToolsAgent::injectedScriptDispatcherSource() {
  base::StringPiece injectDispatchjs =
      webkit_glue::GetDataResource(IDR_DEVTOOLS_INJECT_DISPATCH_JS);
  return WebCString(injectDispatchjs.data(), injectDispatchjs.length());
}

WebCString TestShellDevToolsAgent::debuggerScriptSource() {
  base::StringPiece debuggerScriptjs =
      webkit_glue::GetDataResource(IDR_DEVTOOLS_DEBUGGER_SCRIPT_JS);
  return WebCString(debuggerScriptjs.data(), debuggerScriptjs.length());
}

void TestShellDevToolsAgent::AsyncCall(const TestShellDevToolsCallArgs &args) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      call_method_factory_.NewRunnableMethod(&TestShellDevToolsAgent::Call,
                                             args),
      0);
}

void TestShellDevToolsAgent::Call(const TestShellDevToolsCallArgs &args) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->dispatchMessageFromFrontend(args.data_);
  if (TestShellDevToolsCallArgs::calls_count() == 1 && dev_tools_client_)
    dev_tools_client_->all_messages_processed();
}

WebDevToolsAgent* TestShellDevToolsAgent::GetWebAgent() {
  if (!web_view_)
    return NULL;
  return web_view_->devToolsAgent();
}

void TestShellDevToolsAgent::attach(TestShellDevToolsClient* client) {
  DCHECK(!dev_tools_client_);
  dev_tools_client_ = client;
  WebDevToolsAgent *web_agent = GetWebAgent();
  if (web_agent)
    web_agent->attach();
}

void TestShellDevToolsAgent::detach(TestShellDevToolsClient* client) {
  DCHECK(dev_tools_client_);
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->detach();
  dev_tools_client_ = NULL;
}

bool TestShellDevToolsAgent::setTimelineProfilingEnabled(bool enabled) {
  WebDevToolsAgent* agent = GetWebAgent();
  if (!agent)
    return false;
  agent->setTimelineProfilingEnabled(enabled);
  return true;
}

bool TestShellDevToolsAgent::evaluateInWebInspector(
      long call_id,
      const std::string& script) {
  WebDevToolsAgent* agent = GetWebAgent();
  if (!agent)
    return false;
  agent->evaluateInWebInspector(call_id, WebString::fromUTF8(script));
  return true;
}

// static
void WebKit::WebDevToolsAgentClient::sendMessageToFrontendOnIOThread(
    WebKit::WebDevToolsMessageData const &) {
  NOTIMPLEMENTED();
}
