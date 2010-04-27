// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_proxy_settings.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/importer/firefox_importer_utils.h"

namespace {

const wchar_t* const kNetworkProxyTypeKey = L"network.proxy.type";
const char* const kHTTPProxyKey = "network.proxy.http";
const wchar_t* const kHTTPProxyPortKey = L"network.proxy.http_port";
const char* const kSSLProxyKey = "network.proxy.ssl";
const wchar_t* const kSSLProxyPortKey = L"network.proxy.ssl_port";
const char* const kFTPProxyKey = "network.proxy.ftp";
const wchar_t* const kFTPProxyPortKey = L"network.proxy.ftp_port";
const char* const kGopherProxyKey = "network.proxy.gopher";
const wchar_t* const kGopherProxyPortKey = L"network.proxy.gopher_port";
const char* const kSOCKSHostKey = "network.proxy.socks";
const wchar_t* const kSOCKSHostPortKey = L"network.proxy.socks_port";
const wchar_t* const kSOCKSVersionKey = L"network.proxy.socks_version";
const char* const kAutoconfigURL = "network.proxy.autoconfig_url";
const char* const kNoProxyListKey = "network.proxy.no_proxies_on";
const char* const kPrefFileName = "prefs.js";

FirefoxProxySettings::ProxyConfig IntToProxyConfig(int type) {
  switch (type) {
    case 1:
      return FirefoxProxySettings::MANUAL;
    case 2:
      return FirefoxProxySettings::AUTO_FROM_URL;
    case 4:
      return FirefoxProxySettings::AUTO_DETECT;
    case 5:
      return FirefoxProxySettings::SYSTEM;
    default:
      LOG(ERROR) << "Unknown Firefox proxy config type: " << type;
      return FirefoxProxySettings::NO_PROXY;
  }
}

FirefoxProxySettings::SOCKSVersion IntToSOCKSVersion(int type) {
  switch (type) {
    case 4:
      return FirefoxProxySettings::V4;
    case 5:
      return FirefoxProxySettings::V5;
    default:
      LOG(ERROR) << "Unknown Firefox proxy config type: " << type;
      return FirefoxProxySettings::UNKNONW;
  }
}

}  // namespace

FirefoxProxySettings::FirefoxProxySettings() {
  Reset();
}

FirefoxProxySettings::~FirefoxProxySettings() {
}

void FirefoxProxySettings::Reset() {
  config_type_ = NO_PROXY;
  http_proxy_.clear();
  http_proxy_port_ = 0;
  ssl_proxy_.clear();
  ssl_proxy_port_ = 0;
  ftp_proxy_.clear();
  ftp_proxy_port_ = 0;
  gopher_proxy_.clear();
  gopher_proxy_port_ = 0;
  socks_host_.clear();
  socks_port_ = 0;
  socks_version_ = UNKNONW;
  proxy_bypass_list_.clear();
  autoconfig_url_.clear();
}

// static
 bool FirefoxProxySettings::GetSettings(FirefoxProxySettings* settings) {
  DCHECK(settings);
  settings->Reset();

  FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return false;
  FilePath pref_file = profile_path.AppendASCII(kPrefFileName);
  return GetSettingsFromFile(pref_file, settings);
}

// static
bool FirefoxProxySettings::GetSettingsFromFile(const FilePath& pref_file,
                                               FirefoxProxySettings* settings) {
  DictionaryValue dictionary;
  if (!ParsePrefFile(pref_file, &dictionary))
    return false;

  int proxy_type = 0;
  if (!dictionary.GetInteger(kNetworkProxyTypeKey, &proxy_type))
    return true;  // No type means no proxy.

  settings->config_type_ = IntToProxyConfig(proxy_type);
  if (settings->config_type_ == AUTO_FROM_URL) {
    if (!dictionary.GetStringASCII(kAutoconfigURL,
                                   &(settings->autoconfig_url_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy autoconfig URL";
    }
    return false;
  }

  if (settings->config_type_ == MANUAL) {
    if (!dictionary.GetStringASCII(kHTTPProxyKey, &(settings->http_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy HTTP host";
    if (!dictionary.GetInteger(kHTTPProxyPortKey,
                               &(settings->http_proxy_port_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy HTTP port";
    }
    if (!dictionary.GetStringASCII(kSSLProxyKey, &(settings->ssl_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL host";
    if (!dictionary.GetInteger(kSSLProxyPortKey, &(settings->ssl_proxy_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL port";
    if (!dictionary.GetStringASCII(kFTPProxyKey, &(settings->ftp_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy FTP host";
    if (!dictionary.GetInteger(kFTPProxyPortKey, &(settings->ftp_proxy_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL port";
    if (!dictionary.GetStringASCII(kGopherProxyKey, &(settings->gopher_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy gopher host";
    if (!dictionary.GetInteger(kGopherProxyPortKey,
                               &(settings->gopher_proxy_port_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy gopher port";
    }
    if (!dictionary.GetStringASCII(kSOCKSHostKey, &(settings->socks_host_)))
      LOG(ERROR) << "Failed to retrieve Firefox SOCKS host";
    if (!dictionary.GetInteger(kSOCKSHostPortKey, &(settings->socks_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox SOCKS port";
    int socks_version;
    if (dictionary.GetInteger(kSOCKSVersionKey, &socks_version))
      settings->socks_version_ = IntToSOCKSVersion(socks_version);

    std::string proxy_bypass;
    if (dictionary.GetStringASCII(kNoProxyListKey, &proxy_bypass) &&
        !proxy_bypass.empty()) {
      StringTokenizer string_tok(proxy_bypass, ",");
      while (string_tok.GetNext()) {
        std::string token = string_tok.token();
        TrimWhitespaceASCII(token, TRIM_ALL, &token);
        if (!token.empty())
          settings->proxy_bypass_list_.push_back(token);
      }
    }
  }
  return true;
}
