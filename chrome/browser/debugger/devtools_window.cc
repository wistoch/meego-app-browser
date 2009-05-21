// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

DevToolsWindow::DevToolsWindow(Profile* profile)
    : TabStripModelObserver(),
      inspected_tab_closing_(false) {
  static std::wstring g_wp_key = L"";
  if (g_wp_key.empty()) {
    // TODO(pfeldman): Make browser's getter for this key static.
    g_wp_key.append(prefs::kBrowserWindowPlacement);
    g_wp_key.append(L"_");
    g_wp_key.append(L"DevToolsApp");

    PrefService* prefs = g_browser_process->local_state();
    prefs->RegisterDictionaryPref(g_wp_key.c_str());
    
    const DictionaryValue* wp_pref = prefs->GetDictionary(g_wp_key.c_str());
    if (!wp_pref) {
      DictionaryValue* defaults = prefs->GetMutableDictionary(
          g_wp_key.c_str());
      defaults->SetInteger(L"left", 100);
      defaults->SetInteger(L"top", 100);
      defaults->SetInteger(L"right", 740);
      defaults->SetInteger(L"bottom", 740);
      defaults->SetBoolean(L"maximized", false);
      defaults->SetBoolean(L"always_on_top", false);
    }
  }

  browser_.reset(Browser::CreateForApp(L"DevToolsApp", profile, false));
  GURL contents(std::string(chrome::kChromeUIDevToolsURL) + "devtools.html");
  browser_->AddTabWithURL(contents, GURL(), PageTransition::START_PAGE, true,
                          -1, false, NULL);
  tab_contents_ = browser_->GetSelectedTabContents();
  browser_->tabstrip_model()->AddObserver(this);
}

DevToolsWindow::~DevToolsWindow() {
}

void DevToolsWindow::Show() {
  browser_->window()->Show();
  tab_contents_->view()->SetInitialFocus();
}

DevToolsWindow* DevToolsWindow::AsDevToolsWindow() {
  return this; 
}

RenderViewHost* DevToolsWindow::GetRenderViewHost() const {
  return tab_contents_->render_view_host();
}

void DevToolsWindow::InspectedTabClosing() {
  inspected_tab_closing_ = true;
  browser_->CloseAllTabs();
}

void DevToolsWindow::SendMessageToClient(const IPC::Message& message) {
  RenderViewHost* target_host = tab_contents_->render_view_host();
  IPC::Message* m =  new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsWindow::TabClosingAt(TabContents* contents, int index) {
  if (!inspected_tab_closing_ && contents == tab_contents_) {
    // Notify manager that this DevToolsClientHost no longer exists.
    NotifyCloseListener();
  }
  if (browser_->tabstrip_model()->empty()) {
    // We are removing the last tab. Delete browser along with the
    // tabstrip_model and its listeners.
    delete this;
  }
}
