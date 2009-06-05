// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "base/message_loop.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"

DevToolsManager::DevToolsManager()
    : inspected_rvh_for_reopen_(NULL) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(inspected_rvh_to_client_host_.empty());
  DCHECK(client_host_to_inspected_rvh_.empty());
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    RenderViewHost* inspected_rvh) {
  InspectedRvhToClientHostMap::iterator it =
      inspected_rvh_to_client_host_.find(inspected_rvh);
  if (it != inspected_rvh_to_client_host_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    RenderViewHost* inspected_rvh,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(inspected_rvh));

  inspected_rvh_to_client_host_[inspected_rvh] = client_host;
  client_host_to_inspected_rvh_[client_host] = inspected_rvh;
  client_host->set_close_listener(this);

  SendAttachToAgent(inspected_rvh);
}

void DevToolsManager::ForwardToDevToolsAgent(
    RenderViewHost* client_rvh,
    const IPC::Message& message) {
    for (InspectedRvhToClientHostMap::iterator it =
             inspected_rvh_to_client_host_.begin();
         it != inspected_rvh_to_client_host_.end();
         ++it) {
    DevToolsWindow* win = it->second->AsDevToolsWindow();
    if (!win) {
      continue;
    }
    if (client_rvh == win->GetRenderViewHost()) {
      ForwardToDevToolsAgent(win, message);
      return;
    }
  }
}

void DevToolsManager::ForwardToDevToolsAgent(DevToolsClientHost* from,
                                             const IPC::Message& message) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(from);
  if (!inspected_rvh) {
    // TODO(yurys): notify client that the agent is no longer available
    NOTREACHED();
    return;
  }

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(inspected_rvh->routing_id());
  inspected_rvh->Send(m);
}

void DevToolsManager::ForwardToDevToolsClient(RenderViewHost* inspected_rvh,
                                              const IPC::Message& message) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(inspected_rvh);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->SendMessageToClient(message);
}

void DevToolsManager::OpenDevToolsWindow(RenderViewHost* inspected_rvh) {
  EnableDevToolsInPrefs(inspected_rvh);
  DevToolsClientHost* host = GetDevToolsClientHostFor(inspected_rvh);
  if (!host) {
    host = new DevToolsWindow(
        inspected_rvh->site_instance()->browsing_instance()->profile());
    RegisterDevToolsClientHostFor(inspected_rvh, host);
  }
  DevToolsWindow* window = host->AsDevToolsWindow();
  if (window)
    window->Show();
}

void DevToolsManager::InspectElement(RenderViewHost* inspected_rvh,
                                     int x,
                                     int y) {
  OpenDevToolsWindow(inspected_rvh);
  IPC::Message* m = new DevToolsAgentMsg_InspectElement(x, y);
  m->set_routing_id(inspected_rvh->routing_id());
  inspected_rvh->Send(m);
}

void DevToolsManager::ClientHostClosing(DevToolsClientHost* host) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(host);
  if (!inspected_rvh) {
    return;
  }
  SendDetachToAgent(inspected_rvh);

  inspected_rvh_to_client_host_.erase(inspected_rvh);
  client_host_to_inspected_rvh_.erase(host);
}

RenderViewHost* DevToolsManager::GetInspectedRenderViewHost(
    DevToolsClientHost* client_host) {
  ClientHostToInspectedRvhMap::iterator it =
      client_host_to_inspected_rvh_.find(client_host);
  if (it != client_host_to_inspected_rvh_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::UnregisterDevToolsClientHostFor(
      RenderViewHost* inspected_rvh) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(inspected_rvh);
  if (!host) {
    return;
  }
  host->InspectedTabClosing();
  inspected_rvh_to_client_host_.erase(inspected_rvh);
  client_host_to_inspected_rvh_.erase(host);
  if (inspected_rvh_for_reopen_ == inspected_rvh) {
    inspected_rvh_for_reopen_ = NULL;
  }
}

void DevToolsManager::OnNavigatingToPendingEntry(RenderViewHost* rvh,
                                                 RenderViewHost* dest_rvh,
                                                 const GURL& gurl) {
  DevToolsClientHost* client_host =
      GetDevToolsClientHostFor(rvh);
  if (client_host) {
    // Navigating to URL in the inspected window.
    inspected_rvh_to_client_host_.erase(rvh);
    inspected_rvh_to_client_host_[dest_rvh] = client_host;
    client_host_to_inspected_rvh_[client_host] = dest_rvh;
    SendAttachToAgent(dest_rvh);
    return;
  }

  // Iterate over client hosts and if there is one that has render view host
  // changing, reopen entire client window (this must be caused by the user
  // manually refreshing its content).
  for (ClientHostToInspectedRvhMap::iterator it =
           client_host_to_inspected_rvh_.begin();
       it != client_host_to_inspected_rvh_.end(); ++it) {
    DevToolsWindow* window = it->first->AsDevToolsWindow();
    if (window && window->GetRenderViewHost() == rvh) {
      RenderViewHost* inspected_rvn = it->second;
      UnregisterDevToolsClientHostFor(inspected_rvn);
      SendDetachToAgent(inspected_rvn);
      inspected_rvh_for_reopen_ = inspected_rvn;
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &DevToolsManager::ForceReopenWindow));
      return;
    }
  }
}

void DevToolsManager::SendAttachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    IPC::Message* m = new DevToolsAgentMsg_Attach();
    m->set_routing_id(inspected_rvh->routing_id());
    inspected_rvh->Send(m);
  }
}

void DevToolsManager::SendDetachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    IPC::Message* m = new DevToolsAgentMsg_Detach();
    m->set_routing_id(inspected_rvh->routing_id());
    inspected_rvh->Send(m);
  }
}

void DevToolsManager::EnableDevToolsInPrefs(RenderViewHost* inspected_rvh) {
  Profile* profile = inspected_rvh->site_instance()->browsing_instance()->
      profile();
  if (!profile->GetPrefs()->GetBoolean(prefs::kWebKitDeveloperExtrasEnabled)) {
    //TODO(pfeldman): Show message box with warning to the user.
    profile->GetPrefs()->SetBoolean(prefs::kWebKitDeveloperExtrasEnabled,
        true);
  }
}

void DevToolsManager::ForceReopenWindow() {
  if (inspected_rvh_for_reopen_) {
    OpenDevToolsWindow(inspected_rvh_for_reopen_);
    inspected_rvh_for_reopen_ = NULL;
  }
}
