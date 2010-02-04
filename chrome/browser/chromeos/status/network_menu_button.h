// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_

#include <string>
#include <vector>

#include "app/throb_animation.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/status/password_dialog_view.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class SkBitmap;

namespace gfx {
class Canvas;
}

namespace chromeos {

// The network menu button in the status area.
// This class will handle getting the wifi networks and populating the menu.
// It will also handle the status icon changing and connecting to another
// wifi/cellular network.
//
// The network menu looks like this:
//
// <icon>  Wifi: <status>
//         Turn Wifi <action>
// <icon>     Wifi Network A
// <check>    Wifi Network B
// <icon>     Wifi Network C
// --------------------------------
// <icon>  Cellular: <status>
//         Turn Cellular <action>
// <icon>     Cellular Network A
// <check>    Cellular Network B
// <icon>     Cellular Network C
// --------------------------------
// <icon>  Ethernet: <status>
//         Turn Ethernet <action>
//
// <icon> will show the current state of the network device and the strength of
//   the wifi/cellular networks.
// <check> will be a check mark icon for the currently connected wifi.
// <status> will be one of: Connected, Connecting, Disconnected, or Off.
// <action> will be either On or Off depending on the current state.
class NetworkMenuButton : public StatusAreaButton,
                          public views::ViewMenuDelegate,
                          public menus::MenuModel,
                          public PasswordDialogDelegate,
                          public NetworkLibrary::Observer {
 public:
  explicit NetworkMenuButton(gfx::NativeWindow parent_window);
  virtual ~NetworkMenuButton();

  // menus::MenuModel implementation.
  virtual bool HasIcons() const  { return true; }
  virtual int GetItemCount() const;
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      menus::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual bool IsEnabledAt(int index) const;
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}

  // PasswordDialogDelegate implementation.
  virtual bool OnPasswordDialogCancel() { return true; }
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void NetworkTraffic(NetworkLibrary* cros, int traffic_type);

 protected:
  // StatusAreaButton implementation.
  virtual void DrawIcon(gfx::Canvas* canvas);

 private:
  enum MenuItemFlags {
    FLAG_DISABLED          = 1 << 0,
    FLAG_TOGGLE_ETHERNET   = 1 << 1,
    FLAG_TOGGLE_WIFI       = 1 << 2,
    FLAG_TOGGLE_CELLULAR   = 1 << 3,
    FLAG_TOGGLE_OFFLINE    = 1 << 4,
    FLAG_ACTIVATE_WIFI     = 1 << 5,
    FLAG_ACTIVATE_CELLULAR = 1 << 6
  };

  struct MenuItem {
    MenuItem()
        : type(menus::MenuModel::TYPE_SEPARATOR),
          flags(0) {}
    MenuItem(menus::MenuModel::ItemType type, string16 label, SkBitmap icon,
             WifiNetwork wifi_network, CellularNetwork cellular_network,
             int flags)
        : type(type),
          label(label),
          icon(icon),
          wifi_network(wifi_network),
          cellular_network(cellular_network),
          flags(flags) {}

    menus::MenuModel::ItemType type;
    string16 label;
    SkBitmap icon;
    WifiNetwork wifi_network;
    CellularNetwork cellular_network;
    int flags;
  };
  typedef std::vector<MenuItem> MenuItemVector;

  static SkBitmap IconForWifiStrength(int strength);

  static SkBitmap IconForCellularStrength(int strength);

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Called by RunMenu to initialize our list of menu items.
  void InitMenuItems();

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of wifi strength images.
  static const int kNumWifiImages;

  // The minimum opacity of the wifi bars.
  static const int kMinOpacity;

  // The maximum opacity of the wifi bars.
  static const int kMaxOpacity;

  // The wifi icons used in menu. These are built on initialization.
  static SkBitmap* menu_wifi_icons_;

  // The ethernet icon used in menu,
  static SkBitmap* menu_wired_icon_;

  // The disconnected icon used in menu,
  static SkBitmap* menu_disconnected_icon_;

  // Our menu items.
  MenuItemVector menu_items_;

  // The activated wifi network.
  WifiNetwork activated_wifi_network_;

  // The network menu.
  views::Menu2 network_menu_;

  // Our parent window
  gfx::NativeWindow parent_window_;

  // The throb animation that does the wifi connecting animation.
  ThrobAnimation animation_connecting_;

  // The throb animation that does the downloading animation.
  ThrobAnimation animation_downloading_;

  // The throb animation that does the uploading animation.
  ThrobAnimation animation_uploading_;

  // The duration of the icon throbbing in milliseconds.
  static const int kThrobDuration;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
