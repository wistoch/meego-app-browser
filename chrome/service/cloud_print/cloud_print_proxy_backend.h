// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_

#include <map>
#include <string>

#include "base/thread.h"
#include "chrome/service/cloud_print/printer_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class CloudPrintProxyService;
class DictionaryValue;

// CloudPrintProxyFrontend is the interface used by CloudPrintProxyBackend to
// communicate with the entity that created it and, presumably, is interested in
// cloud print proxy related activity.
// NOTE: All methods will be invoked by a CloudPrintProxyBackend on the same
// thread used to create that CloudPrintProxyBackend.
class CloudPrintProxyFrontend {
 public:
  CloudPrintProxyFrontend() {}

  // There is a list of printers available that can be registered.
  virtual void OnPrinterListAvailable(
      const cloud_print::PrinterList& printer_list) = 0;
  // We successfully authenticated with the cloud print server. This callback
  // allows the frontend to persist the tokens.
  virtual void OnAuthenticated(const std::string& cloud_print_token,
                               const std::string& cloud_print_xmpp_token,
                               const std::string& email) = 0;

 protected:
  // Don't delete through SyncFrontend interface.
  virtual ~CloudPrintProxyFrontend() {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyFrontend);
};

class CloudPrintProxyBackend {
 public:
  explicit CloudPrintProxyBackend(CloudPrintProxyFrontend* frontend,
                                  const GURL& cloud_print_server_url);
  ~CloudPrintProxyBackend();

  bool InitializeWithLsid(const std::string& lsid, const std::string& proxy_id);
  bool InitializeWithToken(const std::string cloud_print_token,
                           const std::string cloud_print_xmpp_token,
                           const std::string email,
                           const std::string& proxy_id);
  void Shutdown();
  void RegisterPrinters(const cloud_print::PrinterList& printer_list);
  void HandlePrinterNotification(const std::string& printer_id);

 private:
  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core;
  // A thread we dedicate for use to perform initialization and
  // authentication.
  base::Thread core_thread_;
  // Our core, which communicates with AuthWatcher for GAIA authentication and
  // which contains printer registration code.
  scoped_refptr<Core> core_;
  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;
  // The frontend which is responsible for displaying UI and updating Prefs
  CloudPrintProxyFrontend* frontend_;

  friend class base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyBackend);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_

