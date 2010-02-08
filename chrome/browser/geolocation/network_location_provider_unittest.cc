// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/network_location_provider.h"

#include <map>

#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestServerUrl[] = "https://www.geolocation.test/service";
const char kTestHost[] = "myclienthost.test";
}  // namespace

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener
    : public LocationProviderBase::ListenerInterface {
 public:
  MessageLoopQuitListener()
      : client_message_loop_(MessageLoop::current()),
        updated_provider_(NULL),
        movement_provider_(NULL) {
    DCHECK(client_message_loop_);
  }
  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider) {
    EXPECT_EQ(client_message_loop_, MessageLoop::current());
    updated_provider_ = provider;
    client_message_loop_->Quit();
  }
  virtual void MovementDetected(LocationProviderBase* provider) {
    EXPECT_EQ(client_message_loop_, MessageLoop::current());
    movement_provider_ = provider;
    client_message_loop_->Quit();
  }
  MessageLoop* client_message_loop_;
  LocationProviderBase* updated_provider_;
  LocationProviderBase* movement_provider_;
};

class FakeAccessTokenStore : public LocationProviderBase::AccessTokenStore {
 public:
  FakeAccessTokenStore() : allow_set_(true) {}

  virtual bool SetAccessToken(const GURL& url,
                              const string16& access_token) {
    if (!allow_set_)
      return false;
    token_map_[url] = access_token;
    return true;
  }
  virtual bool GetAccessToken(const GURL& url, string16* access_token) {
    std::map<GURL, string16>::iterator item = token_map_.find(url);
    if (item == token_map_.end())
      return false;
    *access_token = item->second;
    return true;
  }
  bool allow_set_;
  std::map<GURL, string16> token_map_;
};


// A mock implementation of DeviceDataProviderImplBase for testing. Adapted from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation_test.cc
template<typename DataType>
class MockDeviceDataProviderImpl
    : public DeviceDataProviderImplBase<DataType> {
 public:
  // Factory method for use with DeviceDataProvider::SetFactory.
  static DeviceDataProviderImplBase<DataType>* Create() {
    return new MockDeviceDataProviderImpl<DataType>();
  }
  static MockDeviceDataProviderImpl<DataType>* instance() {
    CHECK(instance_ != NULL);
    return instance_;
  }

  MockDeviceDataProviderImpl() {
    CHECK(instance_ == NULL);
    instance_ = this;
  }
  virtual ~MockDeviceDataProviderImpl() {
    CHECK(this == instance_);
    instance_ = NULL;
  }

  // DeviceDataProviderImplBase implementation.
  virtual bool StartDataProvider() {
    return true;
  }
  virtual bool GetData(DataType* data_out) {
    CHECK(data_out);
    AutoLock lock(data_mutex_);
    *data_out = data_;
    // We always have all the data we can get, so return true.
    return true;
  }

  void SetData(const DataType& new_data) {
    data_mutex_.Acquire();
    const bool differs = data_.DiffersSignificantly(new_data);
    data_ = new_data;
    data_mutex_.Release();
    if (differs)
      this->NotifyListeners();
  }

 private:
  static MockDeviceDataProviderImpl<DataType>* instance_;

  DataType data_;
  Lock data_mutex_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceDataProviderImpl);
};

template<typename DataType>
MockDeviceDataProviderImpl<DataType>*
MockDeviceDataProviderImpl<DataType>::instance_ = NULL;

// Main test fixture
class NetworkLocationProviderTest : public testing::Test {
 public:
  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);
  }

  virtual void TearDown() {
    WifiDataProvider::ResetFactory();
    RadioDataProvider::ResetFactory();
    URLFetcher::set_factory(NULL);
    base::LeakTracker<URLFetcher>::CheckForLeaks();
  }

  LocationProviderBase* CreateProvider() {
    return NewNetworkLocationProvider(
        &access_token_store_,
        NULL,  // No URLContextGetter needed, as using test urlfecther factory.
        test_server_url_,
        ASCIIToUTF16(kTestHost));
  }

 protected:
  NetworkLocationProviderTest() : test_server_url_(kTestServerUrl) {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    RadioDataProvider::SetFactory(
        MockDeviceDataProviderImpl<RadioData>::Create);
    WifiDataProvider::SetFactory(
        MockDeviceDataProviderImpl<WifiData>::Create);
  }

  static int IndexToChannal(int index) { return index + 4; }
  static int IndexToAge(int index) { return (index * 3) + 100; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address = ASCIIToUTF16(StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = i;
      ap.age = IndexToAge(i);
      ap.channel = IndexToChannal(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = ASCIIToUTF16("Some nice network");
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static void ParseRequest(const std::string& request_data,
                           WifiData* wifi_data_out,
                           std::string* access_token_out) {
    CHECK(wifi_data_out && access_token_out);
    scoped_ptr<Value> value(base::JSONReader::Read(request_data, false));
    EXPECT_TRUE(value != NULL);
    EXPECT_EQ(Value::TYPE_DICTIONARY, value->GetType());
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());
    std::string attr_value;
    EXPECT_TRUE(dictionary->GetString(L"version", &attr_value));
    EXPECT_EQ(attr_value, "1.1.0");
    EXPECT_TRUE(dictionary->GetString(L"host", &attr_value));
    EXPECT_EQ(attr_value, kTestHost);
    // Everything else is optional.
    ListValue* wifi_aps;
    if (dictionary->GetList(L"wifi_towers", &wifi_aps)) {
      int i = 0;
      for (ListValue::const_iterator it = wifi_aps->begin();
           it < wifi_aps->end(); ++it, ++i) {
        EXPECT_EQ(Value::TYPE_DICTIONARY, (*it)->GetType());
        DictionaryValue* ap = static_cast<DictionaryValue*>(*it);
        AccessPointData data;
        ap->GetStringAsUTF16(L"mac_address", &data.mac_address);
        ap->GetInteger(L"signal_strength", &data.radio_signal_strength);
        ap->GetInteger(L"age", &data.age);
        ap->GetInteger(L"channel", &data.channel);
        ap->GetInteger(L"signal_to_noise", &data.signal_to_noise);
        ap->GetStringAsUTF16(L"ssid", &data.ssid);
        wifi_data_out->access_point_data.insert(data);
      }
    } else {
      wifi_data_out->access_point_data.clear();
    }
    if (!dictionary->GetString(L"access_token", access_token_out))
      access_token_out->clear();
  }

  static void CheckEmptyRequestIsValid(const std::string& request_data) {
    WifiData wifi_aps;
    std::string access_token;
    ParseRequest(request_data, &wifi_aps, &access_token);
    EXPECT_EQ(0, static_cast<int>(wifi_aps.access_point_data.size()));
    EXPECT_TRUE(access_token.empty());
  }

  static void CheckRequestIsValid(const std::string& request_data,
                                  int expected_wifi_aps,
                                  const std::string& expected_access_token) {
    WifiData wifi_aps;
    std::string access_token;
    ParseRequest(request_data, &wifi_aps, &access_token);
    EXPECT_EQ(expected_wifi_aps,
              static_cast<int>(wifi_aps.access_point_data.size()));
    WifiData expected_data = CreateReferenceWifiScanData(expected_wifi_aps);
    WifiData::AccessPointDataSet::const_iterator expected =
        expected_data.access_point_data.begin();
    WifiData::AccessPointDataSet::const_iterator actual =
        wifi_aps.access_point_data.begin();
    for (int i = 0; i < expected_wifi_aps; ++i) {
      EXPECT_EQ(expected->mac_address, actual->mac_address) << i;
      EXPECT_EQ(expected->radio_signal_strength, actual->radio_signal_strength)
          << i;
      EXPECT_EQ(expected->age, actual->age) << i;
      EXPECT_EQ(expected->channel, actual->channel) << i;
      EXPECT_EQ(expected->signal_to_noise, actual->signal_to_noise) << i;
      EXPECT_EQ(expected->ssid, actual->ssid) << i;
      ++expected;
      ++actual;
    }
    EXPECT_EQ(expected_access_token, access_token);
  }

  const GURL test_server_url_;
  MessageLoop main_message_loop_;
  FakeAccessTokenStore  access_token_store_;
  TestURLFetcherFactory url_fetcher_factory_;
};


TEST_F(NetworkLocationProviderTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, MessageLoop::current());
  scoped_refptr<LocationProviderBase> provider(CreateProvider());
  EXPECT_TRUE(NULL != provider.get());
  provider = NULL;
  SUCCEED();
}

TEST_F(NetworkLocationProviderTest, StartProvider) {
  scoped_refptr<LocationProviderBase> provider(CreateProvider());
  EXPECT_TRUE(provider->StartProvider());
  TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_EQ(test_server_url_, fetcher->original_url());

  // No wifi data so expect an empty request.
  CheckEmptyRequestIsValid(fetcher->upload_data());
}

TEST_F(NetworkLocationProviderTest, MultipleWifiScansComplete) {
  scoped_refptr<LocationProviderBase> provider(CreateProvider());
  EXPECT_TRUE(provider->StartProvider());

  TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  CheckEmptyRequestIsValid(fetcher->upload_data());
  // Complete the network request with bad position fix (using #define so we
  // can paste this into various other strings below)
  #define REFERENCE_ACCESS_TOKEN "2:k7j3G6LaL6u_lafw:4iXOeOpTh1glSXe"
  const char* kNoFixNetworkResponse =
      "{"
      "  \"location\": null,"
      "  \"access_token\": \"" REFERENCE_ACCESS_TOKEN "\""
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, URLRequestStatus(), 200,  // OK
      ResponseCookies(), kNoFixNetworkResponse);

  // This should have set the access token anyhow
  EXPECT_EQ(1, static_cast<int>(access_token_store_.token_map_.size()));
  string16 token;
  EXPECT_TRUE(access_token_store_.GetAccessToken(test_server_url_, &token));
  EXPECT_EQ(REFERENCE_ACCESS_TOKEN, UTF16ToUTF8(token));

  Position position;
  provider->GetPosition(&position);
  EXPECT_FALSE(position.IsValidFix());

  // Now wifi data arrives
  const int kFirstScanAps = 6;
  MockDeviceDataProviderImpl<WifiData>::instance()->SetData(
      CreateReferenceWifiScanData(kFirstScanAps));  // Will notify listeners
  fetcher = url_fetcher_factory_.GetFetcherByID(kFirstScanAps);
  ASSERT_TRUE(fetcher != NULL);
  // The request should have access token (set previously) and the wifi data.
  CheckRequestIsValid(fetcher->upload_data(),
                      kFirstScanAps,
                      REFERENCE_ACCESS_TOKEN);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"location\": {"
      "    \"latitude\": 51.0,"
      "    \"longitude\": -0.1,"
      "    \"altitude\": 30.1,"
      "    \"accuracy\": 1200.4,"
      "    \"altitude_accuracy\": 10.6"
      "  }"
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, URLRequestStatus(), 200,  // OK
      ResponseCookies(), kReferenceNetworkResponse);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.1, position.altitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());

  // Token should still be in the store.
  EXPECT_EQ(1, static_cast<int>(access_token_store_.token_map_.size()));
  EXPECT_TRUE(access_token_store_.GetAccessToken(test_server_url_, &token));
  EXPECT_EQ(REFERENCE_ACCESS_TOKEN, UTF16ToUTF8(token));

  // Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  MockDeviceDataProviderImpl<WifiData>::instance()->SetData(
      CreateReferenceWifiScanData(kSecondScanAps));
  fetcher = url_fetcher_factory_.GetFetcherByID(kSecondScanAps);
  EXPECT_FALSE(fetcher);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.IsValidFix());

  // Now a third scan with more than twice the original amount -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  MockDeviceDataProviderImpl<WifiData>::instance()->SetData(
      CreateReferenceWifiScanData(kThirdScanAps));
  fetcher = url_fetcher_factory_.GetFetcherByID(kThirdScanAps);
  EXPECT_TRUE(fetcher);
  // ...reply with a network error.
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_,
      URLRequestStatus(URLRequestStatus::FAILED, -1),
      200,  // should be ignored
      ResponseCookies(), "");

  // Error means we now no longer have a fix.
  provider->GetPosition(&position);
  EXPECT_FALSE(position.is_valid_latlong());
  EXPECT_FALSE(position.IsValidFix());

  // Wifi scan returns to original set: should be serviced from cache.
  const TestURLFetcher* orig_fetcher =
      url_fetcher_factory_.GetFetcherByID(kFirstScanAps);
  MockDeviceDataProviderImpl<WifiData>::instance()->SetData(
      CreateReferenceWifiScanData(kFirstScanAps));
  fetcher = url_fetcher_factory_.GetFetcherByID(kFirstScanAps);
  EXPECT_EQ(orig_fetcher, fetcher);  // No new request created.

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.IsValidFix());
}

// TODO(joth): Add tests for corner cases around the 2 second startup delay
//            (e.g. timer firing, or being pre-empted by data arriving)
