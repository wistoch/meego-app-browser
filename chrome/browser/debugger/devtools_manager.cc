// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

DevToolsManager::DevToolsManager() : web_contents_listeners_(NULL) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!web_contents_listeners_.get()) <<
      "All devtools client hosts must alredy have been destroyed.";
  DCHECK(navcontroller_to_client_host_.empty());
  DCHECK(client_host_to_navcontroller_.empty());
}

void DevToolsManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == NotificationType::WEB_CONTENTS_DISCONNECTED);

  Source<WebContents> src(source);
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(*src.ptr());
  if (!client_host) {
    return;
  }

  NavigationController* controller = src->controller();
  bool active = (controller->active_contents() == src.ptr());
  if (active) {
    // Active tab contents disconnecting from its renderer means that the tab
    // is closing.
    client_host->InspectedTabClosing();
  }
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    const WebContents& web_contents) {
  NavigationController* navigation_controller = web_contents.controller();
  ClientHostMap::const_iterator it =
      navcontroller_to_client_host_.find(navigation_controller);
  if (it != navcontroller_to_client_host_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    const WebContents& web_contents,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(web_contents));

  NavigationController* navigation_controller = web_contents.controller();
  navcontroller_to_client_host_[navigation_controller] = client_host;
  client_host_to_navcontroller_[client_host] = navigation_controller;
  client_host->set_close_listener(this);

  StartListening(navigation_controller);
}

void DevToolsManager::ForwardToDevToolsAgent(
    const RenderViewHost& client_rvh,
    const IPC::Message& message) {
    for (ClientHostMap::const_iterator it =
             navcontroller_to_client_host_.begin();
       it != navcontroller_to_client_host_.end();
       ++it) {
    DevToolsWindow* win = it->second->AsDevToolsWindow();
    if (!win) {
      continue;
    }
    if (win->HasRenderViewHost(client_rvh)) {
      ForwardToDevToolsAgent(*win, message);
      return;
    }
  }
}

void DevToolsManager::ForwardToDevToolsAgent(const DevToolsClientHost& from,
                                             const IPC::Message& message) {
  NavigationController* nav_controller =
      GetDevToolsAgentNavigationController(from);
  if (!nav_controller) {
    NOTREACHED();
    return;
  }

  // TODO(yurys): notify client that the agent is no longer available
  TabContents* tc = nav_controller->active_contents();
  if (!tc) {
    return;
  }
  WebContents* wc = tc->AsWebContents();
  if (!wc) {
    return;
  }
  RenderViewHost* target_host = wc->render_view_host();
  if (!target_host) {
    return;
  }

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsManager::ForwardToDevToolsClient(const RenderViewHost& from,
                                              const IPC::Message& message) {
  WebContents* wc = from.delegate()->GetAsWebContents();
  if (!wc) {
    NOTREACHED();
    return;
  }
  DevToolsClientHost* target_host = GetDevToolsClientHostFor(*wc);
  if (!target_host) {
    NOTREACHED();
    return;
  }
  target_host->SendMessageToClient(message);
}

void DevToolsManager::ClientHostClosing(DevToolsClientHost* host) {
  NavigationController* controller = GetDevToolsAgentNavigationController(
      *host);
  DCHECK(controller);

  // This should be done before StopListening as the latter checks number of
  // alive devtools instances.
  navcontroller_to_client_host_.erase(controller);
  client_host_to_navcontroller_.erase(host);

  StopListening(controller);
}

NavigationController* DevToolsManager::GetDevToolsAgentNavigationController(
    const DevToolsClientHost& client_host) {
  NavControllerMap::const_iterator it =
      client_host_to_navcontroller_.find(&client_host);
  if (it != client_host_to_navcontroller_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::StartListening(
    NavigationController* navigation_controller) {
  // TODO(yurys): add render host change listener
  if (!web_contents_listeners_.get()) {
    web_contents_listeners_.reset(new NotificationRegistrar);
    web_contents_listeners_->Add(
        this,
        NotificationType::WEB_CONTENTS_DISCONNECTED,
        NotificationService::AllSources());
  }
}

void DevToolsManager::StopListening(
    NavigationController* navigation_controller) {
  DCHECK(web_contents_listeners_.get());
  if (navcontroller_to_client_host_.empty()) {
    DCHECK(client_host_to_navcontroller_.empty());
    web_contents_listeners_.reset();
  }
}
