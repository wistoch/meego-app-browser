// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "views/controls/combobox/combobox.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::A;

class NetworkScreenTest : public WizardInProcessBrowserTest {
 public:
  NetworkScreenTest(): WizardInProcessBrowserTest("network") {
    cellular_.name = "Cellular network";
    wifi_.ssid = "WiFi network";
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();

    mock_login_library_ = new MockLoginLibrary();
    test_api()->SetLoginLibrary(mock_login_library_, true);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);

    // Minimal set of expectations needed on NetworkScreen initialization.
    // Status bar expectations are defined with RetiresOnSaturation() so
    // these mocks will be active once status bar is initialized.
    EXPECT_CALL(*mock_network_library_, ethernet_connected())
        .Times(1)
        .WillOnce((Return(false)));
    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
        .Times(1)
        .WillOnce((Return(false)));
    EXPECT_CALL(*mock_network_library_, wifi_networks())
        .Times(1)
        .WillOnce((ReturnRef(wifi_networks_)));
    EXPECT_CALL(*mock_network_library_, cellular_networks())
        .Times(1)
        .WillOnce((ReturnRef(cellular_networks_)));
    EXPECT_CALL(*mock_network_library_, AddObserver(_))
        .Times(1);
    EXPECT_CALL(*mock_network_library_, RemoveObserver(_))
        .Times(1);

    SetStatusAreaMocksExpectations();
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    test_api()->SetLoginLibrary(NULL, false);
  }

  void EthernetExpectations(bool connected, bool connecting) {
    EXPECT_CALL(*mock_network_library_, ethernet_connected())
        .Times(1)
        .WillRepeatedly((Return(connected)));
    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
        .Times(1)
        .WillRepeatedly((Return(connecting)));
  }

  void WifiExpectations(bool connected, bool connecting) {
    EXPECT_CALL(*mock_network_library_, wifi_connected())
        .Times(1)
        .WillOnce((Return(connected)));
    EXPECT_CALL(*mock_network_library_, wifi_connecting())
        .Times(1)
        .WillOnce((Return(connecting)));
  }

  void SetupWifiNetwork(bool connected, bool connecting) {
    wifi_networks_.clear();
    wifi_.connected = connected;
    wifi_.connecting = connecting;
    wifi_networks_.push_back(wifi_);
  }

  void SetupCellularNetwork(bool connected, bool connecting) {
    cellular_networks_.clear();
    cellular_.connected = connected;
    cellular_.connecting = connecting;
    cellular_networks_.push_back(cellular_);
  }

  void CellularExpectations(bool connected, bool connecting) {
    EXPECT_CALL(*mock_network_library_, cellular_connected())
        .Times(1)
        .WillOnce((Return(connected)));
    EXPECT_CALL(*mock_network_library_, cellular_connecting())
        .Times(1)
        .WillOnce((Return(connecting)));
  }

  void WifiCellularNetworksExpectations() {
    EXPECT_CALL(*mock_network_library_, wifi_networks())
       .Times(1)
       .WillOnce((ReturnRef(wifi_networks_)));
    EXPECT_CALL(*mock_network_library_, cellular_networks())
       .Times(1)
       .WillOnce((ReturnRef(cellular_networks_)));
  }

  void WifiSsidExpectation(const std::string& ssid) {
    EXPECT_CALL(*mock_network_library_, wifi_ssid())
        .Times(1)
        .WillOnce((ReturnRef(ssid)));
  }

  void CellularNameExpectation(const std::string& name) {
    EXPECT_CALL(*mock_network_library_, cellular_name())
        .Times(1)
        .WillOnce((ReturnRef(name)));
  }

  MockLoginLibrary* mock_login_library_;

  CellularNetworkVector cellular_networks_;
  WifiNetworkVector wifi_networks_;

  CellularNetwork cellular_;
  WifiNetwork wifi_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

class DummyComboboxModel : public ComboboxModel {
 public:
  virtual int GetItemCount() { return 2; }

  virtual std::wstring GetItemAt(int index) {
    return L"Item " + IntToWString(index);
  }
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Basic) {
  ASSERT_TRUE(controller());
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);
  ASSERT_EQ(1, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE),
            network_screen->GetItemAt(0));
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, NetworksConnectedNotSelected) {
  ASSERT_TRUE(controller());
  NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  EthernetExpectations(true, false);
  WifiCellularNetworksExpectations();
  network_screen->NetworkChanged(network_library);

  // When OOBE flow is active network selection should be explicit.
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
            network_screen->GetItemAt(1));

  // Ethernet - disconnected, WiFi & Cellular - connected.
  EthernetExpectations(false, false);
  SetupWifiNetwork(true, false);
  WifiExpectations(true, false);
  SetupCellularNetwork(true, false);
  CellularExpectations(true, false);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(wifi_.ssid);
  CellularNameExpectation(cellular_.name);
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(3, network_screen->GetItemCount());
  EXPECT_EQ(ASCIIToWide(wifi_.ssid), network_screen->GetItemAt(1));
  EXPECT_EQ(ASCIIToWide(cellular_.name), network_screen->GetItemAt(2));

  // Ethernet, WiFi & Cellular - connected.
  EthernetExpectations(true, false);
  WifiExpectations(true, false);
  CellularExpectations(true, false);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(wifi_.ssid);
  CellularNameExpectation(cellular_.name);
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(4, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
            network_screen->GetItemAt(1));
  EXPECT_EQ(ASCIIToWide(wifi_.ssid), network_screen->GetItemAt(2));
  EXPECT_EQ(ASCIIToWide(cellular_.name), network_screen->GetItemAt(3));
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, EthernetSelected) {
  ASSERT_TRUE(controller());
  NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  // Emulate connecting to Ethernet.
  EthernetExpectations(false, true);
  WifiCellularNetworksExpectations();
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
            network_screen->GetItemAt(1));
  ASSERT_EQ(network_screen, controller()->current_screen());

  // Emulate connected Ethernet.
  EthernetExpectations(true, false);
  WifiCellularNetworksExpectations();
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(2, network_screen->GetItemCount());
  ASSERT_EQ(network_screen, controller()->current_screen());

  DummyComboboxModel combobox_model;
  views::Combobox combobox(&combobox_model);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::NETWORK_CONNECTED))
      .Times(1);
  controller()->set_observer(mock_screen_observer.get());

  // Emulate combobox selection.
  EthernetExpectations(true, false);
  WifiCellularNetworksExpectations();
  network_screen->ItemChanged(&combobox, 0, 1);
  network_view->SetSelectedNetworkItem(1);
  ui_test_utils::RunAllPendingInMessageLoop();
  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, WifiSelected) {
  ASSERT_TRUE(controller());
  NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  EthernetExpectations(false, false);
  SetupWifiNetwork(false, false);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(std::string());
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(ASCIIToWide(wifi_.ssid), network_screen->GetItemAt(1));

  DummyComboboxModel combobox_model;
  views::Combobox combobox(&combobox_model);

  // Emulate combobox selection.
  EthernetExpectations(false, false);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(std::string());
  network_screen->ItemChanged(&combobox, 0, 1);
  network_view->SetSelectedNetworkItem(1);
  EXPECT_CALL(*mock_network_library_,
              ConnectToWifiNetwork(A<WifiNetwork>(), string16()))
      .Times(1);
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_EQ(2, network_screen->GetItemCount());

  // Emulate connecting to WiFi network.
  EthernetExpectations(false, false);
  SetupWifiNetwork(false, true);
  WifiExpectations(false, true);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(wifi_.ssid);
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());

  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::NETWORK_CONNECTED))
      .Times(1);
  controller()->set_observer(mock_screen_observer.get());

  // Emulate connected WiFi network.
  EthernetExpectations(false, false);
  SetupWifiNetwork(true, false);
  WifiExpectations(true, false);
  WifiCellularNetworksExpectations();
  WifiSsidExpectation(wifi_.ssid);
  network_screen->NetworkChanged(network_library);
  ui_test_utils::RunAllPendingInMessageLoop();
  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CellularSelected) {
  ASSERT_TRUE(controller());
  NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  EthernetExpectations(false, false);
  SetupCellularNetwork(false, false);
  WifiCellularNetworksExpectations();
  CellularNameExpectation(std::string());
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(ASCIIToWide(cellular_.name), network_screen->GetItemAt(1));

  DummyComboboxModel combobox_model;
  views::Combobox combobox(&combobox_model);

  // Emulate combobox selection.
  EthernetExpectations(false, false);
  WifiCellularNetworksExpectations();
  CellularNameExpectation(std::string());
  network_screen->ItemChanged(&combobox, 0, 1);
  network_view->SetSelectedNetworkItem(1);
  EXPECT_CALL(*mock_network_library_, ConnectToCellularNetwork(_))
      .Times(1);
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_EQ(2, network_screen->GetItemCount());

  // Emulate connecting to cellular network.
  EthernetExpectations(false, false);
  SetupCellularNetwork(false, true);
  CellularExpectations(false, true);
  WifiCellularNetworksExpectations();
  CellularNameExpectation(cellular_.name);
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());

  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::NETWORK_CONNECTED))
      .Times(1);
  controller()->set_observer(mock_screen_observer.get());

  // Emulate connected cellular network.
  EthernetExpectations(false, false);
  SetupCellularNetwork(true, false);
  CellularExpectations(true, false);
  WifiCellularNetworksExpectations();
  CellularNameExpectation(cellular_.name);
  network_screen->NetworkChanged(network_library);
  ui_test_utils::RunAllPendingInMessageLoop();
  controller()->set_observer(NULL);
}

}  // namespace chromeos
