// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/json_pref_store.h"

CloudPrintProxy::CloudPrintProxy() {
}

CloudPrintProxy::~CloudPrintProxy() {
  DCHECK(CalledOnValidThread());
  Shutdown();
}

void CloudPrintProxy::Initialize(JsonPrefStore* service_prefs) {
  DCHECK(CalledOnValidThread());
  service_prefs_ = service_prefs;
}

void CloudPrintProxy::EnableForUser(const std::string& lsid) {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    return;

  backend_.reset(new CloudPrintProxyBackend(this));
  std::string proxy_id;
  service_prefs_->prefs()->GetString(prefs::kCloudPrintProxyId, &proxy_id);
  if (proxy_id.empty()) {
    proxy_id = cloud_print::GenerateProxyId();
    service_prefs_->prefs()->SetString(prefs::kCloudPrintProxyId, proxy_id);
    service_prefs_->WritePrefs();
  }
  // If we have been passed in an LSID, we want to use this to authenticate.
  // Else we will try and retrieve the last used auth tokens from prefs.
  if (!lsid.empty()) {
    backend_->InitializeWithLsid(lsid, proxy_id);
  } else {
    std::string cloud_print_token;
    service_prefs_->prefs()->GetString(prefs::kCloudPrintAuthToken,
                                       &cloud_print_token);
    DCHECK(!cloud_print_token.empty());
    std::string cloud_print_xmpp_token;
    service_prefs_->prefs()->GetString(prefs::kCloudPrintXMPPAuthToken,
                                       &cloud_print_xmpp_token);
    DCHECK(!cloud_print_xmpp_token.empty());
    std::string cloud_print_email;
    service_prefs_->prefs()->GetString(prefs::kCloudPrintEmail,
                                       &cloud_print_email);
    DCHECK(!cloud_print_email.empty());
    backend_->InitializeWithToken(cloud_print_token, cloud_print_xmpp_token,
                                  cloud_print_email, proxy_id);
  }
}

void CloudPrintProxy::DisableForUser() {
  DCHECK(CalledOnValidThread());
  Shutdown();
}

void CloudPrintProxy::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}

// Notification methods from the backend. Called on UI thread.
void CloudPrintProxy::OnPrinterListAvailable(
    const cloud_print::PrinterList& printer_list) {
  DCHECK(CalledOnValidThread());
  // We could potentially show UI here allowing the user to select which
  // printers to register. For now, we just register all.
  backend_->RegisterPrinters(printer_list);
}

void CloudPrintProxy::OnAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email) {
  DCHECK(CalledOnValidThread());
  service_prefs_->prefs()->SetString(prefs::kCloudPrintAuthToken,
                                     cloud_print_token);
  service_prefs_->prefs()->SetString(prefs::kCloudPrintXMPPAuthToken,
                                     cloud_print_xmpp_token);
  service_prefs_->prefs()->SetString(prefs::kCloudPrintEmail, email);
  service_prefs_->WritePrefs();
}

