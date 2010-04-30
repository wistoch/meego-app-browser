// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "net/url_request/url_request_job.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::NetworkLibraryImpl> {
  void RetainCallee(chromeos::NetworkLibraryImpl* obj) {}
  void ReleaseCallee(chromeos::NetworkLibraryImpl* obj) {}
};

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

// static
const int NetworkLibraryImpl::kNetworkTrafficeTimerSecs = 1;

NetworkLibraryImpl::NetworkLibraryImpl()
    : traffic_type_(0),
      network_status_connection_(NULL),
      available_devices_(0),
      enabled_devices_(0),
      connected_devices_(0),
      offline_mode_(false) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    Init();
  }
  g_url_request_job_tracker.AddObserver(this);
}

NetworkLibraryImpl::~NetworkLibraryImpl() {
  if (network_status_connection_) {
    DisconnectMonitorNetwork(network_status_connection_);
  }
  g_url_request_job_tracker.RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImpl, URLRequestJobTracker::JobObserver implementation:

void NetworkLibraryImpl::OnJobAdded(URLRequestJob* job) {
  CheckNetworkTraffic(false);
}

void NetworkLibraryImpl::OnJobRemoved(URLRequestJob* job) {
  CheckNetworkTraffic(false);
}

void NetworkLibraryImpl::OnJobDone(URLRequestJob* job,
                               const URLRequestStatus& status) {
  CheckNetworkTraffic(false);
}

void NetworkLibraryImpl::OnJobRedirect(URLRequestJob* job, const GURL& location,
                                   int status_code) {
  CheckNetworkTraffic(false);
}

void NetworkLibraryImpl::OnBytesRead(URLRequestJob* job, int byte_count) {
  CheckNetworkTraffic(true);
}

void NetworkLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkLibraryImpl::RequestWifiScan() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    RequestScan(TYPE_WIFI);
  }
}

void NetworkLibraryImpl::ConnectToWifiNetwork(WifiNetwork network,
                                              const string16& password,
                                              const string16& identity,
                                              const string16& certpath) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    ConnectToNetworkWithCertInfo(network.service_path.c_str(),
                     password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
                     identity.empty() ? NULL : UTF16ToUTF8(identity).c_str(),
                     certpath.empty() ? NULL : UTF16ToUTF8(certpath).c_str());
  }
}

void NetworkLibraryImpl::ConnectToWifiNetwork(const string16& ssid,
                                              const string16& password,
                                              const string16& identity,
                                              const string16& certpath,
                                              bool auto_connect) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    // First create a service from hidden network.
    ServiceInfo* service = GetWifiService(UTF16ToUTF8(ssid).c_str(),
                                          SECURITY_UNKNOWN);
    if (service) {
      // Set auto-connect.
      SetAutoConnect(service->service_path, auto_connect);
      // Now connect to that service.
      ConnectToNetworkWithCertInfo(service->service_path,
                       password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
                       identity.empty() ? NULL : UTF16ToUTF8(identity).c_str(),
                       certpath.empty() ? NULL : UTF16ToUTF8(certpath).c_str());

      // Clean up ServiceInfo object.
      FreeServiceInfo(service);
    } else {
      LOG(WARNING) << "Cannot find hidden network: " << ssid;
      // TODO(chocobo): Show error message.
    }
  }
}

void NetworkLibraryImpl::ConnectToCellularNetwork(CellularNetwork network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    ConnectToNetwork(network.service_path.c_str(), NULL);
  }
}

void NetworkLibraryImpl::DisconnectFromWifiNetwork(const WifiNetwork& network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    DisconnectFromNetwork(network.service_path.c_str());
  }
}

void NetworkLibraryImpl::DisconnectFromCellularNetwork(
    const CellularNetwork& network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    DisconnectFromNetwork(network.service_path.c_str());
  }
}

void NetworkLibraryImpl::SaveWifiNetwork(const WifiNetwork& network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    SetPassphrase(network.service_path.c_str(), network.passphrase.c_str());
    SetAutoConnect(network.service_path.c_str(), network.auto_connect);
  }
}

void NetworkLibraryImpl::ForgetWifiNetwork(const WifiNetwork& network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    DeleteRememberedService(network.service_path.c_str());
  }
}

void NetworkLibraryImpl::ForgetCellularNetwork(const CellularNetwork& network) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    DeleteRememberedService(network.service_path.c_str());
  }
}

void NetworkLibraryImpl::EnableEthernetNetworkDevice(bool enable) {
  EnableNetworkDeviceType(TYPE_ETHERNET, enable);
}

void NetworkLibraryImpl::EnableWifiNetworkDevice(bool enable) {
  EnableNetworkDeviceType(TYPE_WIFI, enable);
}

void NetworkLibraryImpl::EnableCellularNetworkDevice(bool enable) {
  EnableNetworkDeviceType(TYPE_CELLULAR, enable);
}

void NetworkLibraryImpl::EnableOfflineMode(bool enable) {
  if (!CrosLibrary::Get()->EnsureLoaded())
    return;

  // If network device is already enabled/disabled, then don't do anything.
  if (enable && offline_mode_) {
    LOG(INFO) << "Trying to enable offline mode when it's already enabled. ";
    return;
  }
  if (!enable && !offline_mode_) {
    LOG(INFO) << "Trying to disable offline mode when it's already disabled. ";
    return;
  }

  if (SetOfflineMode(enable)) {
    offline_mode_ = enable;
  }
}

NetworkIPConfigVector NetworkLibraryImpl::GetIPConfigs(
    const std::string& device_path) {
  NetworkIPConfigVector ipconfig_vector;
  if (!device_path.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; i++) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        ipconfig_vector.push_back(
            NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                            ipconfig.netmask, ipconfig.gateway,
                            ipconfig.name_servers));
      }
      FreeIPConfigStatus(ipconfig_status);
      // Sort the list of ip configs by type.
      std::sort(ipconfig_vector.begin(), ipconfig_vector.end());
    }
  }
  return ipconfig_vector;
}

// static
void NetworkLibraryImpl::NetworkStatusChangedHandler(void* object) {
  NetworkLibraryImpl* network = static_cast<NetworkLibraryImpl*>(object);
  SystemInfo* system = GetSystemInfo();
  if (system) {
    network->UpdateNetworkStatus(system);
    FreeSystemInfo(system);
  }
}

// static
void NetworkLibraryImpl::ParseSystem(SystemInfo* system,
    EthernetNetwork* ethernet,
    WifiNetworkVector* wifi_networks,
    CellularNetworkVector* cellular_networks,
    WifiNetworkVector* remembered_wifi_networks,
    CellularNetworkVector* remembered_cellular_networks) {
  DLOG(INFO) << "ParseSystem:";
  bool ethernet_found = false;
  for (int i = 0; i < system->service_size; i++) {
    const ServiceInfo& service = system->services[i];
    DLOG(INFO) << "  (" << service.type <<
                  ") " << service.name <<
                  " mode=" << service.mode <<
                  " state=" << service.state <<
                  " sec=" << service.security <<
                  " req=" << service.passphrase_required <<
                  " pass=" << service.passphrase <<
                  " str=" << service.strength <<
                  " fav=" << service.favorite <<
                  " auto=" << service.auto_connect <<
                  " error=" << service.error;
    bool connecting = service.state == STATE_ASSOCIATION ||
                      service.state == STATE_CONFIGURATION ||
                      service.state == STATE_CARRIER;
    bool connected = service.state == STATE_READY;
    // if connected, get ip config
    std::string ip_address;
    if (connected && service.device_path) {
      IPConfigStatus* ipconfig_status = ListIPConfigs(service.device_path);
      if (ipconfig_status) {
        for (int i = 0; i < ipconfig_status->size; i++) {
          IPConfig ipconfig = ipconfig_status->ips[i];
          if (strlen(ipconfig.address) > 0)
            ip_address = ipconfig.address;
          DLOG(INFO) << "          ipconfig: " <<
                        " type=" << ipconfig.type <<
                        " address=" << ipconfig.address <<
                        " mtu=" << ipconfig.mtu <<
                        " netmask=" << ipconfig.netmask <<
                        " broadcast=" << ipconfig.broadcast <<
                        " peer_address=" << ipconfig.peer_address <<
                        " gateway=" << ipconfig.gateway <<
                        " domainname=" << ipconfig.domainname <<
                        " name_servers=" << ipconfig.name_servers;
        }
        FreeIPConfigStatus(ipconfig_status);
      }
    }
    if (service.type == TYPE_ETHERNET) {
      ethernet_found = true;
      ethernet->connecting = connecting;
      ethernet->connected = connected;
      ethernet->device_path = service.device_path ? service.device_path :
                                                    std::string();
      ethernet->ip_address = ip_address;
    } else if (service.type == TYPE_WIFI) {
      wifi_networks->push_back(WifiNetwork(service,
                                           connecting,
                                           connected,
                                           ip_address));
    } else if (service.type == TYPE_CELLULAR) {
      cellular_networks->push_back(CellularNetwork(service,
                                                   connecting,
                                                   connected,
                                                   ip_address));
    }
  }
  if (!ethernet_found) {
    ethernet->connecting = false;
    ethernet->connected = false;
    ethernet->device_path = std::string();
    ethernet->ip_address = std::string();
  }
  DLOG(INFO) << "Remembered networks:";
  for (int i = 0; i < system->remembered_service_size; i++) {
    const ServiceInfo& service = system->remembered_services[i];
    // Only serices marked as auto_connect are considered remembered networks.
    // TODO(chocobo): Don't add to remembered service if currently available.
    if (service.auto_connect) {
      DLOG(INFO) << "  (" << service.type <<
                    ") " << service.name <<
                    " mode=" << service.mode <<
                    " sec=" << service.security <<
                    " pass=" << service.passphrase <<
                    " auto=" << service.auto_connect;
      if (service.type == TYPE_WIFI) {
        remembered_wifi_networks->push_back(WifiNetwork(service,
                                                        false,
                                                        false,
                                                        std::string()));
      } else if (service.type == TYPE_CELLULAR) {
        remembered_cellular_networks->push_back(CellularNetwork(service,
                                                                false,
                                                                false,
                                                                std::string()));
      }
    }
  }
}

void NetworkLibraryImpl::Init() {
  // First, get the currently available networks.  This data is cached
  // on the connman side, so the call should be quick.
  SystemInfo* system = GetSystemInfo();
  if (system) {
    LOG(INFO) << "Getting initial CrOS network info.";
    UpdateNetworkStatus(system);
    FreeSystemInfo(system);
  }
  LOG(INFO) << "Registering for network status updates.";
  // Now, register to receive updates on network status.
  network_status_connection_ = MonitorNetwork(&NetworkStatusChangedHandler,
                                              this);
}

void NetworkLibraryImpl::EnableNetworkDeviceType(ConnectionType device,
                                                 bool enable) {
  if (!CrosLibrary::Get()->EnsureLoaded())
    return;

  // If network device is already enabled/disabled, then don't do anything.
  if (enable && (enabled_devices_ & (1 << device))) {
    LOG(WARNING) << "Trying to enable a device that's already enabled: "
                 << device;
    return;
  }
  if (!enable && !(enabled_devices_ & (1 << device))) {
    LOG(WARNING) << "Trying to disable a device that's already disabled: "
                 << device;
    return;
  }

  EnableNetworkDevice(device, enable);
}

void NetworkLibraryImpl::UpdateNetworkStatus(SystemInfo* system) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &NetworkLibraryImpl::UpdateNetworkStatus, system));
    return;
  }

  wifi_networks_.clear();
  cellular_networks_.clear();
  remembered_wifi_networks_.clear();
  remembered_cellular_networks_.clear();
  ParseSystem(system, &ethernet_, &wifi_networks_, &cellular_networks_,
              &remembered_wifi_networks_, &remembered_cellular_networks_);

  wifi_ = WifiNetwork();
  for (size_t i = 0; i < wifi_networks_.size(); i++) {
    if (wifi_networks_[i].connecting || wifi_networks_[i].connected) {
      wifi_ = wifi_networks_[i];
      break;  // There is only one connected or connecting wifi network.
    }
  }
  cellular_ = CellularNetwork();
  for (size_t i = 0; i < cellular_networks_.size(); i++) {
    if (cellular_networks_[i].connecting || cellular_networks_[i].connected) {
      cellular_ = cellular_networks_[i];
      break;  // There is only one connected or connecting cellular network.
    }
  }

  available_devices_ = system->available_technologies;
  enabled_devices_ = system->enabled_technologies;
  connected_devices_ = system->connected_technologies;
  offline_mode_ = system->offline_mode;

  FOR_EACH_OBSERVER(Observer, observers_, NetworkChanged(this));
}

void NetworkLibraryImpl::CheckNetworkTraffic(bool download) {
  // If we already have a pending upload and download notification, then
  // shortcut and return.
  if (traffic_type_ == (Observer::TRAFFIC_DOWNLOAD | Observer::TRAFFIC_UPLOAD))
    return;
  // Figure out if we are uploading and/or downloading. We are downloading
  // if download == true. We are uploading if we have upload progress.
  if (download)
    traffic_type_ |= Observer::TRAFFIC_DOWNLOAD;
  if ((traffic_type_ & Observer::TRAFFIC_UPLOAD) == 0) {
    URLRequestJobTracker::JobIterator it;
    for (it = g_url_request_job_tracker.begin();
         it != g_url_request_job_tracker.end();
         ++it) {
      URLRequestJob* job = *it;
      if (job->GetUploadProgress() > 0) {
        traffic_type_ |= Observer::TRAFFIC_UPLOAD;
        break;
      }
    }
  }
  // If we have new traffic data to send out and the timer is not currently
  // running, then start a new timer.
  if (traffic_type_ && !timer_.IsRunning()) {
    timer_.Start(base::TimeDelta::FromSeconds(kNetworkTrafficeTimerSecs), this,
                 &NetworkLibraryImpl::NetworkTrafficTimerFired);
  }
}

void NetworkLibraryImpl:: NetworkTrafficTimerFired() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &NetworkLibraryImpl::NotifyNetworkTraffic,
                        traffic_type_));
  // Reset traffic type so that we don't send the same data next time.
  traffic_type_ = 0;
}

void NetworkLibraryImpl::NotifyNetworkTraffic(int traffic_type) {
  FOR_EACH_OBSERVER(Observer, observers_, NetworkTraffic(this, traffic_type));
}

bool NetworkLibraryImpl::Connected() const {
  return ethernet_connected() || wifi_connected() || cellular_connected();
}

bool NetworkLibraryImpl::Connecting() const {
  return ethernet_connecting() || wifi_connecting() || cellular_connecting();
}

const std::string& NetworkLibraryImpl::IPAddress() const {
  // Returns highest priority IP address.
  if (ethernet_connected())
    return ethernet_.ip_address;
  if (wifi_connected())
    return wifi_.ip_address;
  if (cellular_connected())
    return cellular_.ip_address;
  return ethernet_.ip_address;
}

}  // namespace chromeos
