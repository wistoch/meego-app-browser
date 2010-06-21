// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/wifi_data_provider_chromeos.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace chromeos {

class GeolocationChromeOsWifiDataProviderTest : public testing::Test {
 protected:
  GeolocationChromeOsWifiDataProviderTest()
      : api_(WifiDataProviderChromeOs::NewWlanApi(&net_lib_)){}
  virtual void SetUp() {
    EXPECT_CALL(net_lib_, wifi_networks())
        .WillOnce(ReturnRef(wifi_network_data_));
  }
  void AddWifiAps(int ssids, int aps_per_ssid) {
    for (int i = 0; i < ssids; ++i) {
      WifiNetwork network;
      network.set_name(StringPrintf("SSID %d", i));
      std::vector<WifiNetwork::AccessPoint> aps;
      for (int j = 0; j < aps_per_ssid; ++j) {
        WifiNetwork::AccessPoint ap;
        ap.channel = i * 10 + j;
        ap.mac_address = StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
            i, j, 3, 4, 5, 6);
        ap.signal_strength = j;
        ap.signal_to_noise = i;
        aps.push_back(ap);
      }
      network.set_access_points(aps);
      wifi_network_data_.push_back(network);
    }
  }

  chromeos::MockNetworkLibrary net_lib_;
  scoped_ptr<WifiDataProviderCommon::WlanApiInterface> api_;
  WifiNetworkVector wifi_network_data_;
  WifiData::AccessPointDataSet ap_data_;
};

TEST_F(GeolocationChromeOsWifiDataProviderTest, NoWifiAvailable) {
  EXPECT_CALL(net_lib_, wifi_available())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(0u, ap_data_.size());
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, NoAccessPointsInRange) {
  EXPECT_CALL(net_lib_, wifi_available())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(0u, ap_data_.size());
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetOneAccessPoint) {
  AddWifiAps(1, 1);
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(1u, ap_data_.size());
  EXPECT_EQ("00:00:03:04:05:06", UTF16ToUTF8(ap_data_.begin()->mac_address));
  EXPECT_EQ("SSID 0", UTF16ToUTF8(ap_data_.begin()->ssid));
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetManyAccessPoints) {
  AddWifiAps(3, 4);
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(12u, ap_data_.size());
}

}  // namespace chromeos
