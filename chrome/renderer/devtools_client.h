// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_CLIENT_H_
#define CHROME_RENDERER_DEVTOOLS_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontendClient.h"

namespace IPC {
class Message;
}
class MessageLoop;
class RenderView;

namespace WebKit {
class WebDevToolsFrontend;
class WebString;
}

struct DevToolsMessageData;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class DevToolsClient : public WebKit::WebDevToolsFrontendClient {
 public:
  explicit DevToolsClient(RenderView* view);
  virtual ~DevToolsClient();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& message);

  // WebDevToolsFrontendClient implementation
  virtual void sendFrontendLoaded();
  virtual void sendMessageToBackend(const WebKit::WebString&);
  virtual void sendDebuggerCommandToAgent(const WebKit::WebString& command);

  virtual void activateWindow();
  virtual void closeWindow();
  virtual void requestDockWindow();
  virtual void requestUndockWindow();

  virtual bool shouldHideScriptsPanel();

 private:
  void OnDispatchOnInspectorFrontend(const std::string& message);

  // Sends message to DevToolsAgent.
  void Send(const IPC::Message& tools_agent_message);

  RenderView* render_view_;  // host render view
  scoped_ptr<WebKit::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

#endif  // CHROME_RENDERER_DEVTOOLS_CLIENT_H_
