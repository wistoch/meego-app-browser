// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/network_screen.h"

#include "app/l10n_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "grit/generated_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"


namespace {

// Time in seconds for connection timeout.
const int kConnectionTimeoutSec = 15;

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

NetworkScreen::NetworkScreen(WizardScreenDelegate* delegate)
    : ViewScreen<NetworkSelectionView>(delegate),
      is_network_subscribed_(false),
      continue_pressed_(false) {
}

NetworkScreen::~NetworkScreen() {
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void NetworkScreen::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (network && network->Connected()) {
    NotifyOnConnection();
  } else {
    continue_pressed_ = true;
    WaitForConnection(network_id_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::Observer implementation:

void NetworkScreen::NetworkChanged(NetworkLibrary* network_lib) {
  UpdateStatus(network_lib);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, ViewScreen implementation:

void NetworkScreen::CreateView() {
  language_switch_menu_.InitLanguageMenu();
  ViewScreen<NetworkSelectionView>::CreateView();
}

NetworkSelectionView* NetworkScreen::AllocateView() {
  return new NetworkSelectionView(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

void NetworkScreen::Refresh() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    SubscribeNetworkNotification();
    NetworkChanged(chromeos::CrosLibrary::Get()->GetNetworkLibrary());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, private:

void NetworkScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
  }
}

void NetworkScreen::NotifyOnConnection() {
  // TODO(nkostylev): Check network connectivity.
  UnsubscribeNetworkNotification();
  connection_timer_.Stop();
  delegate()->GetObserver(this)->OnExit(ScreenObserver::NETWORK_CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  // TODO(nkostylev): Notify on connection error.
  StopWaitingForConnection(network_id_);
}

void NetworkScreen::UpdateStatus(NetworkLibrary* network) {
  if (!view() || !network)
    return;

  if (network->ethernet_connected()) {
    StopWaitingForConnection(
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
  } else if (network->wifi_connected()) {
    StopWaitingForConnection(ASCIIToUTF16(network->wifi_name()));
  } else if (network->cellular_connected()) {
    StopWaitingForConnection(ASCIIToUTF16(network->cellular_name()));
  } else if (network->ethernet_connecting()) {
    WaitForConnection(
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
  } else if (network->wifi_connecting()) {
    WaitForConnection(ASCIIToUTF16(network->wifi_name()));
  } else if (network->cellular_connecting()) {
    WaitForConnection(ASCIIToUTF16(network->cellular_name()));
  } else {
    view()->EnableContinue(network->Connected());
  }
}

void NetworkScreen::StopWaitingForConnection(const string16& network_id) {
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  bool is_connected = network && network->Connected();
  if (is_connected && continue_pressed_) {
    NotifyOnConnection();
    return;
  }

  continue_pressed_ = false;
  connection_timer_.Stop();

  network_id_ = network_id;
  view()->ShowConnectingStatus(false, network_id_);
  view()->EnableContinue(is_connected);
}

void NetworkScreen::WaitForConnection(const string16& network_id) {
  connection_timer_.Stop();
  connection_timer_.Start(base::TimeDelta::FromSeconds(kConnectionTimeoutSec),
                          this,
                          &NetworkScreen::OnConnectionTimeout);

  network_id_ = network_id;
  view()->ShowConnectingStatus(true, network_id_);

  view()->EnableContinue(false);
}

}  // namespace chromeos
