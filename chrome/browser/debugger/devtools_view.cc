// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_view.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/property_bag.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"

DevToolsView::DevToolsView(Profile* profile)
    : tab_contents_(NULL),
      profile_(profile) {
  web_container_ = new TabContentsContainerView();
  AddChildView(web_container_);
}

DevToolsView::~DevToolsView() {
}

std::string DevToolsView::GetClassName() const {
  return "DevToolsView";
}

gfx::Size DevToolsView::GetPreferredSize() {
  return gfx::Size(640, 640);
}

void DevToolsView::Layout() {
  web_container_->SetBounds(0, 0, width(), height());
}

void DevToolsView::ViewHierarchyChanged(bool is_add,
                                     views::View* parent,
                                     views::View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    Init();
  }
}

void DevToolsView::Init() {
  // We can't create the TabContents until we've actually been put into a real
  // view hierarchy somewhere.
  tab_contents_ = new TabContents(profile_, NULL, MSG_ROUTING_NONE, NULL);
  tab_contents_->set_delegate(this);
  web_container_->SetTabContents(tab_contents_);
  tab_contents_->render_view_host()->AllowDOMUIBindings();

  // chrome://devtools/devtools.html
  GURL contents(std::string(chrome::kChromeUIDevToolsURL) + "devtools.html");

  // this will call CreateRenderView to create renderer process
  tab_contents_->controller().LoadURL(contents, GURL(),
                                      PageTransition::START_PAGE);

  // If each DevTools front end has its own renderer process allow to inspect
  // DevTools windows.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab)) {
    views::Accelerator accelerator('J', true /* shift_down */,
                                   true /* ctrl_down */, false /* alt_down */);
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->RegisterAccelerator(accelerator, this);
  }
}

bool DevToolsView::AcceleratorPressed(const views::Accelerator& accelerator) {
  // TODO(yurys): get rid of this hack and pull the accelerator from the
  // resources
  views::Accelerator la('J', true /* shift_down */, true /* ctrl_down */,
                        false /* alt_down */);
  if (!tab_contents_) {
    return false;
  }
  if (!(la == accelerator)) {
    return false;
  }
  DevToolsManager* manager = g_browser_process->devtools_manager();
  manager->OpenDevToolsWindow(tab_contents_->render_view_host());
  return true;
}

void DevToolsView::OnWindowClosing() {
  DCHECK(tab_contents_) << "OnWindowClosing is called twice";
  if (tab_contents_) {
    // Detach last (and only) tab.
    web_container_->SetTabContents(NULL);

    // Destroy the tab and navigation controller.
    delete tab_contents_;
    tab_contents_ = NULL;
  }
}

void DevToolsView::SendMessageToClient(const IPC::Message& message) {
  if (tab_contents_) {
    RenderViewHost* target_host = tab_contents_->render_view_host();
    IPC::Message* m =  new IPC::Message(message);
    m->set_routing_id(target_host->routing_id());
    target_host->Send(m);
  }
}

RenderViewHost* DevToolsView::GetRenderViewHost() const {
  if (tab_contents_) {
    return tab_contents_->render_view_host();
  }
  return NULL;
}

void DevToolsView::OpenURLFromTab(TabContents* source,
                               const GURL& url, const GURL& referrer,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) {
  NOTREACHED();
}
