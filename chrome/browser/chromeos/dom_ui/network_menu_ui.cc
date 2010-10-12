// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/network_menu_ui.h"

#include "base/values.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/views/native_menu_domui.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "views/controls/menu/menu_2.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
//
// MenuHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to the "system" view.
class NetworkMenuHandler : public chromeos::MenuHandlerBase,
                           public base::SupportsWeakPtr<NetworkMenuHandler> {
 public:
  NetworkMenuHandler();
  virtual ~NetworkMenuHandler();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void HandleAction(const ListValue* values);

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuHandler);
};

void NetworkMenuHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "action",
      NewCallback(this,
                  &NetworkMenuHandler::HandleAction));
}

void NetworkMenuHandler::HandleAction(const ListValue* values) {
  menus::MenuModel* model = GetMenuModel();
  if (model)
    static_cast<chromeos::NetworkMenuUI*>(dom_ui_)->ModelAction(model, values);
}

NetworkMenuHandler::NetworkMenuHandler() {
}

NetworkMenuHandler::~NetworkMenuHandler() {
}

} // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// NetworkMenuUI
//
////////////////////////////////////////////////////////////////////////////////

NetworkMenuUI::NetworkMenuUI(TabContents* contents)
    : chromeos::MenuUI(
        contents,
        ALLOW_THIS_IN_INITIALIZER_LIST(
            CreateMenuUIHTMLSource(*this,
                                   chrome::kChromeUINetworkMenu,
                                   "NetworkMenu",
                                   IDR_NETWORK_MENU_JS,
                                   -1))) {
  NetworkMenuHandler* handler = new NetworkMenuHandler();
  AddMessageHandler((handler)->Attach(this));
}

void NetworkMenuUI::AddCustomConfigValues(DictionaryValue* config) const {
}

void NetworkMenuUI::ModelAction(const menus::MenuModel* model,
                                const ListValue* values) {
  //const NetworkMenu* network_menu = static_cast<const NetworkMenu*>(model);
  // Invoke model methods here.
}

DictionaryValue* NetworkMenuUI::CreateMenuItem(const menus::MenuModel* model,
                                               int index,
                                               const char* type,
                                               int* max_icon_width,
                                               bool* has_accel) const {
  // Create a MenuUI menu item, then append network specific values.
  DictionaryValue* item = MenuUI::CreateMenuItem(model,
                                                 index,
                                                 type,
                                                 max_icon_width,
                                                 has_accel);
  // Network menu specific values.
  const NetworkMenu* network_menu = static_cast<const NetworkMenu*>(model);
  NetworkMenu::NetworkInfo info;
  bool found = network_menu->GetNetworkAt(index, &info);

  item->SetBoolean("visible", found);
  item->SetString("network_type", info.network_type);
  item->SetString("status", info.status);
  item->SetString("message", info.message);
  item->SetString("ip_address", info.ip_address);
  item->SetBoolean("need_passphrase", info.need_passphrase);
  item->SetBoolean("remembered", info.remembered);
  return item;
}

views::Menu2* NetworkMenuUI::CreateMenu2(menus::MenuModel* model) {
  views::Menu2* menu = new views::Menu2(model);
  NativeMenuDOMUI::SetMenuURL(
      menu, GURL(StringPrintf("chrome://%s", chrome::kChromeUINetworkMenu)));
  return menu;
}

}  // namespace chromeos
