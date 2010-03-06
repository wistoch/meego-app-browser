// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_

#include <string>

#include "app/combobox_model.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/network_list.h"
#include "chrome/browser/chromeos/status/password_dialog_view.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace views {
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

class ScreenObserver;

// View for the network selection/initial welcome screen.
class NetworkSelectionView : public views::View,
                             public ComboboxModel,
                             public views::Combobox::Listener,
                             public views::ButtonListener,
                             public PasswordDialogDelegate,
                             public NetworkLibrary::Observer {
 public:
  explicit NetworkSelectionView(ScreenObserver* observer);
  virtual ~NetworkSelectionView();

  void Init();
  void UpdateLocalizedStrings();
  void Refresh();

  // views::View: implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // ComboboxModel implementation:
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // PasswordDialogDelegate implementation:
  virtual bool OnPasswordDialogCancel() { return true; }
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  // NetworkLibrary::Observer implementation:
  virtual void NetworkChanged(NetworkLibrary* network_lib);
  virtual void NetworkTraffic(NetworkLibrary* cros, int traffic_type);

 private:
  // Returns currently selected network in the combobox.
  NetworkList::NetworkItem* GetSelectedNetwork();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Opens password dialog for the encrypted networks.
  void OpenPasswordDialog(WifiNetwork network);

  // Selects network by type and id.
  void SelectNetwork(NetworkList::NetworkType type,
                     const string16& id);

  // Shows network connecting status or network selection otherwise.
  void ShowConnectingStatus(bool connecting, const string16& network_id);

  // Subscribe/unsubscribes from network change notification.
  void ChangeNetworkNotification(bool subscribe);

  // Dialog controls.
  views::Combobox* network_combobox_;
  views::Label* welcome_label_;
  views::Label* select_network_label_;
  views::Label* connecting_network_label_;
  views::NativeButton* offline_button_;

  // Notifications receiver.
  ScreenObserver* observer_;

  // True if subscribed to network change notification.
  bool network_notification_;

  // Cached networks.
  NetworkList networks_;
  string16 network_id_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSelectionView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
