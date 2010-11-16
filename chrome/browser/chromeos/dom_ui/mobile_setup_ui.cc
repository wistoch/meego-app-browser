// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/mobile_setup_ui.h"

#include <algorithm>
#include <map>
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Host page JS API function names.
const char kJsApiStartActivation[] = "startActivation";
const char kJsApiCloseTab[] = "closeTab";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";

const wchar_t kJsDeviceStatusChangedHandler[] =
    L"mobile.MobileSetup.deviceStateChanged";

// Error codes matching codes defined in the cellular config file.
const char kErrorDefault[] = "default";
const char kErrorBadConnectionPartial[] = "bad_connection_partial";
const char kErrorBadConnectionActivated[] = "bad_connection_activated";
const char kErrorRoamingOnConnection[] = "roaming_connection";
const char kErrorNoEVDO[] = "no_evdo";
const char kErrorRoamingActivation[] = "roaming_activation";
const char kErrorRoamingPartiallyActivated[] = "roaming_partially_activated";
const char kErrorNoService[] = "no_service";
const char kFailedPaymentError[] = "failed_payment";

// Cellular configuration file path.
const char kCellularConfigPath[] =
    "/usr/share/chromeos-assets/mobile/mobile_config.json";

// Cellular config file field names.
const char kVersionField[] = "version";
const char kErrorsField[] = "errors";

chromeos::CellularNetwork* GetCellularNetwork() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  if (lib->cellular_networks().begin() != lib->cellular_networks().end()) {
    return *(lib->cellular_networks().begin());
  }
  return NULL;
}

chromeos::CellularNetwork* GetCellularNetwork(const std::string& service_path) {
  return chromeos::CrosLibrary::Get()->
      GetNetworkLibrary()->FindCellularNetworkByPath(service_path);
}

}  // namespace

class CellularConfigDocument {
 public:
  CellularConfigDocument() {}

  // Return error message for a given code.
  std::string GetErrorMessage(const std::string& code);
  const std::string& version() { return version_; }

  bool LoadFromFile(const FilePath& config_path);

 private:
  std::string version_;
  std::map<std::string, std::string> error_map_;

  DISALLOW_COPY_AND_ASSIGN(CellularConfigDocument);
};

static std::map<std::string, std::string> error_messages_;

class MobileSetupUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit MobileSetupUIHTMLSource(const std::string& service_path);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~MobileSetupUIHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(MobileSetupUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler
  : public DOMMessageHandler,
    public chromeos::NetworkLibrary::NetworkManagerObserver,
    public chromeos::NetworkLibrary::NetworkObserver,
    public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  explicit MobileSetupHandler(const std::string& service_path);
  virtual ~MobileSetupHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);
  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* obj,
                                const chromeos::Network* network);

 private:
  typedef enum PlanActivationState {
    PLAN_ACTIVATION_PAGE_LOADING = -1,
    PLAN_ACTIVATION_START = 0,
    PLAN_ACTIVATION_INITIATING_ACTIVATION = 1,
    PLAN_ACTIVATION_RECONNECTING = 2,
    PLAN_ACTIVATION_SHOWING_PAYMENT = 3,
    PLAN_ACTIVATION_DONE = 4,
    PLAN_ACTIVATION_ERROR = 5,
  } PlanActivationState;

  class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
   public:
    explicit TaskProxy(const base::WeakPtr<MobileSetupHandler>& handler)
        : handler_(handler) {
    }
    TaskProxy(const base::WeakPtr<MobileSetupHandler>& handler,
              const std::string& status)
        : handler_(handler), status_(status) {
    }
    void HandleStartActivation() {
      if (handler_)
        handler_->StartActivation();
    }
    void HandleSetTransactionStatus() {
      if (handler_)
        handler_->SetTransactionStatus(status_);
    }
   private:
    base::WeakPtr<MobileSetupHandler> handler_;
    std::string status_;
    DISALLOW_COPY_AND_ASSIGN(TaskProxy);
  };

  // Handlers for JS DOMUI messages.
  void HandleCloseTab(const ListValue* args);
  void HandleSetTransactionStatus(const ListValue* args);
  void HandleStartActivation(const ListValue* args);
  void SetTransactionStatus(const std::string& status);
  void StartActivation();

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Verify the state of cellular network and modify internal state.
  void EvaluateCellularNetwork(chromeos::CellularNetwork* network);
  // Check the current cellular network for error conditions.
  bool GotActivationError(const chromeos::CellularNetwork* network,
                          std::string* error);
  void ChangeState(chromeos::CellularNetwork* network,
                   PlanActivationState new_state,
                   const std::string& error_description);
  // Prepares network devices for cellular activation process.
  void SetupActivationProcess(chromeos::CellularNetwork* network);
  // Resets network devices after cellular activation process.
  // |network| should be NULL if the activation process failed.
  void CompleteActivation(chromeos::CellularNetwork* network);
  // Control routines for handling other types of connections during
  // cellular activation.
  void ReEnableOtherConnections();

  // Converts the currently active CellularNetwork device into a JS object.
  static void GetDeviceInfo(const chromeos::CellularNetwork* network,
                            DictionaryValue* value);
  static bool ShouldReportDeviceState(std::string* state, std::string* error);

  // Performs activation state cellular device evaluation.
  // Returns false if device activation failed. In this case |error|
  // will contain error message to be reported to DOM UI.
  static bool EvaluateCellularDeviceState(bool* report_status,
                                          std::string* state,
                                          std::string* error);

  // Return error message for a given code.
  static std::string GetErrorMessage(const std::string& code);
  static void LoadCellularConfig();

  static const char* GetStateDescription(PlanActivationState state);

  static scoped_ptr<CellularConfigDocument> cellular_config_;

  TabContents* tab_contents_;
  // Internal handler state.
  PlanActivationState state_;
  std::string service_path_;
  // Flags that control if wifi and ethernet connection needs to be restored
  // after the activation of cellular network.
  bool reenable_wifi_;
  bool reenable_ethernet_;
  bool reenable_cert_check_;
  bool transaction_complete_signalled_;
  bool activation_status_test_;
  DISALLOW_COPY_AND_ASSIGN(MobileSetupHandler);
};

scoped_ptr<CellularConfigDocument> MobileSetupHandler::cellular_config_;

////////////////////////////////////////////////////////////////////////////////
//
// CellularConfigDocument
//
////////////////////////////////////////////////////////////////////////////////

std::string CellularConfigDocument::GetErrorMessage(const std::string& code) {
  std::map<std::string, std::string>::iterator iter = error_map_.find(code);
  if (iter == error_map_.end())
    return code;
  return iter->second;
}

bool CellularConfigDocument::LoadFromFile(const FilePath& config_path) {
  error_map_.clear();

  std::string config;
  if (!file_util::ReadFileToString(config_path, &config))
    return false;

  scoped_ptr<Value> root(base::JSONReader::Read(config, true));
  DCHECK(root.get() != NULL);
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
    LOG(WARNING) << "Bad cellular config file";
    return false;
  }

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());
  if (!root_dict->GetString(kVersionField, &version_)) {
    LOG(WARNING) << "Cellular config file missing version";
    return false;
  }
  DictionaryValue* errors = NULL;
  if (!root_dict->GetDictionary(kErrorsField, &errors))
    return false;
  for (DictionaryValue::key_iterator keys = errors->begin_keys();
       keys != errors->end_keys();
       ++keys) {
    std::string value;
    if (!errors->GetString(*keys, &value)) {
      LOG(WARNING) << "Bad cellular config error value";
      error_map_.clear();
      return false;
    }

    error_map_.insert(std::pair<std::string, std::string>(*keys, value));
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource(
    const std::string& service_path)
    : DataSource(chrome::kChromeUIMobileSetupHost, MessageLoop::current()),
      service_path_(service_path) {
}

void MobileSetupUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  const chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);

  DictionaryValue strings;
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE));
  strings.SetString("connecting_header",
                    l10n_util::GetStringFUTF16(IDS_MOBILE_CONNECTING_HEADER,
                        network ? UTF8ToUTF16(network->name()) : string16()));
  strings.SetString("error_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ERROR_HEADER));
  strings.SetString("activating_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ACTIVATING_HEADER));
  strings.SetString("completed_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_HEADER));
  strings.SetString("completed_text",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_TEXT));
  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MOBILE_SETUP_PAGE_HTML));
  const std::string full_html = jstemplate_builder::GetTemplatesHtml(
      html, &strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupHandler
//
////////////////////////////////////////////////////////////////////////////////
MobileSetupHandler::MobileSetupHandler(const std::string& service_path)
    : tab_contents_(NULL),
      state_(PLAN_ACTIVATION_PAGE_LOADING),
      service_path_(service_path),
      reenable_wifi_(false),
      reenable_ethernet_(false),
      reenable_cert_check_(false),
      transaction_complete_signalled_(false),
      activation_status_test_(false) {
}

MobileSetupHandler::~MobileSetupHandler() {
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  ReEnableOtherConnections();
}

DOMMessageHandler* MobileSetupHandler::Attach(DOMUI* dom_ui) {
  return DOMMessageHandler::Attach(dom_ui);
}

void MobileSetupHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
  LoadCellularConfig();
  SetupActivationProcess(GetCellularNetwork(service_path_));
}

void MobileSetupHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(kJsApiStartActivation,
      NewCallback(this, &MobileSetupHandler::HandleStartActivation));
  dom_ui_->RegisterMessageCallback(kJsApiCloseTab,
      NewCallback(this, &MobileSetupHandler::HandleCloseTab));
  dom_ui_->RegisterMessageCallback(kJsApiSetTransactionStatus,
      NewCallback(this, &MobileSetupHandler::HandleSetTransactionStatus));
}

void MobileSetupHandler::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  // Note that even though we get here when the service has
  // reappeared after disappearing earlier in the activation
  // process, there's no need to re-establish the NetworkObserver,
  // because the service path remains the same.
  EvaluateCellularNetwork(GetCellularNetwork(service_path_));
}

void MobileSetupHandler::OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                          const chromeos::Network* network) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  DCHECK(network && network->type() == chromeos::TYPE_CELLULAR);
  EvaluateCellularNetwork(
      static_cast<chromeos::CellularNetwork*>(
          const_cast<chromeos::Network*>(network)));
}

void MobileSetupHandler::HandleCloseTab(const ListValue* args) {
  if (!dom_ui_)
    return;
  Browser* browser = BrowserList::FindBrowserWithFeature(
      dom_ui_->GetProfile(), Browser::FEATURE_TABSTRIP);
  if (browser)
    browser->CloseTabContents(tab_contents_);
}

void MobileSetupHandler::HandleStartActivation(const ListValue* args) {
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleStartActivation));
}

void MobileSetupHandler::HandleSetTransactionStatus(const ListValue* args) {
  const size_t kSetTransactionStatusParamCount = 1;
  if (args->GetSize() != kSetTransactionStatusParamCount)
    return;
  // Get change callback function name.
  std::string status;
  if (!args->GetString(0, &status))
    return;
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), status);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleSetTransactionStatus));
}

void MobileSetupHandler::StartActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);
  if (!network) {
    ChangeState(NULL, PLAN_ACTIVATION_ERROR, std::string());
    return;
  }
  // Start monitoring network property changes.
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->AddNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  lib->AddNetworkObserver(network->service_path(), this);
  state_ = PLAN_ACTIVATION_START;
  EvaluateCellularNetwork(network);
}

void MobileSetupHandler::SetTransactionStatus(const std::string& status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (LowerCaseEqualsASCII(status, "ok") &&
      state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    if (transaction_complete_signalled_) {
      LOG(WARNING) << "Transaction completion signaled more than once!?";
      return;
    }
    transaction_complete_signalled_ = true;
    activation_status_test_ = false;
    state_ = PLAN_ACTIVATION_START;
    chromeos::CellularNetwork* network = GetCellularNetwork();
    if (network &&
        network->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATED) {
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
          DisconnectFromWirelessNetwork(network);
    } else {
      EvaluateCellularNetwork(network);
    }
  }
}

void MobileSetupHandler::EvaluateCellularNetwork(
    chromeos::CellularNetwork* network) {
  if (!dom_ui_)
    return;

  PlanActivationState new_state = state_;
  if (!network) {
    LOG(WARNING) << "Cellular service lost";
    return;
  }

  LOG(INFO) << "Cellular:\n  service=" << network->GetStateString().c_str()
          << "\n  ui=" << GetStateDescription(state_)
          << "\n  activation=" << network->GetActivationStateString().c_str()
          << "\n  restricted=" << (network->restricted_pool() ? "yes" : "no")
          << "\n  error=" << network->GetErrorString().c_str()
          << "\n  setvice_path=" << network->service_path().c_str();
  switch (state_) {
    case PLAN_ACTIVATION_START: {
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_ACTIVATED: {
          if (network->failed_or_disconnected()) {
            new_state = PLAN_ACTIVATION_RECONNECTING;
          } else if (network->connection_state() == chromeos::STATE_READY) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        }
        case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED: {
          if (!activation_status_test_) {
            if (network->failed_or_disconnected()) {
              activation_status_test_ = true;
              new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
            } else if (network->connected()) {
              LOG(INFO) << "Disconnecting from " <<
                  network->service_path().c_str();
              chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
                  DisconnectFromWirelessNetwork(network);
              // Disconnect will force networks to be reevaluated, so
              // we don't want to continue processing on this path anymore.
              return;
            }
          } else {
            if (network->connected())
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            else
              new_state = PLAN_ACTIVATION_RECONNECTING;
            break;
          }
          break;
        }
        case chromeos::ACTIVATION_STATE_UNKNOWN:
        case chromeos::ACTIVATION_STATE_NOT_ACTIVATED: {
          if (network->failed_or_disconnected()) {
            new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
          } else if (network->connected()) {
            LOG(INFO) << "Disconnecting from " <<
                network->service_path().c_str();
            chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
                DisconnectFromWirelessNetwork(network);
            // Disconnect will force networks to be reevaluated, so
            // we don't want to continue processing on this path anymore.
            return;
          }
          break;
        }
        default: {
          new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_INITIATING_ACTIVATION: {
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_ACTIVATED:
          if (network->failed_or_disconnected()) {
            new_state = PLAN_ACTIVATION_RECONNECTING;
          } else if (network->connection_state() == chromeos::STATE_READY) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          if (network->connected())
            new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
          else
            new_state = PLAN_ACTIVATION_RECONNECTING;
          break;
        case chromeos::ACTIVATION_STATE_NOT_ACTIVATED:
          // Wait in this state until activation state changes.
          break;
        case chromeos::ACTIVATION_STATE_ACTIVATING:
          break;
        default:
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_RECONNECTING: {
      // Wait until the service shows up and gets activated.
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_ACTIVATED:
          if (network->connection_state() == chromeos::STATE_READY) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          if (network->connected()) {
            if (network->restricted_pool())
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
          }
          break;
        default:
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      break;
    // Just ignore all signals until the site confirms payment.
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      // Activation completed/failed, ignore network changes.
    case PLAN_ACTIVATION_DONE:
    case PLAN_ACTIVATION_ERROR:
      break;
  }

  std::string error_description;
  if (GotActivationError(network, &error_description)) {
    // Check for this special case when we try to do activate partially
    // activated device. If that attempt failed, try to disconnect to clear the
    // state and reconnect again.
    if ((network->activation_state() ==
            chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED ||
        network->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATING) &&
        network->error() == chromeos::ERROR_UNKNOWN &&
        (state_ == PLAN_ACTIVATION_INITIATING_ACTIVATION ||
            state_ == PLAN_ACTIVATION_RECONNECTING) &&
        activation_status_test_ &&
        network->connection_state() == chromeos::STATE_ACTIVATION_FAILURE) {
      new_state = PLAN_ACTIVATION_RECONNECTING;
    } else {
      new_state = PLAN_ACTIVATION_ERROR;
    }
  }
  ChangeState(network, new_state, error_description);
}

// Debugging helper function, will take it out at the end.
const char* MobileSetupHandler::GetStateDescription(
    PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_PAGE_LOADING:
      return "PAGE_LOADING";
    case PLAN_ACTIVATION_START:
      return "ACTIVATION_START";
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return "INITIATING_ACTIVATION";
    case PLAN_ACTIVATION_RECONNECTING:
      return "RECONNECTING";
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      return "SHOWING_PAYMENT";
    case PLAN_ACTIVATION_DONE:
      return "DONE";
    case PLAN_ACTIVATION_ERROR:
      return "ERROR";
  }
  return "UNKNOWN";
}


void MobileSetupHandler::CompleteActivation(
    chromeos::CellularNetwork* network) {
  // Remove observers, we are done with this page.
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  // If we have successfully activated the connection, set autoconnect flag.
  if (network) {
    network->set_auto_connect(true);
    lib->SaveCellularNetwork(network);
  }
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  // Reactivate other types of connections if we have
  // shut them down previously.
  ReEnableOtherConnections();
}


void MobileSetupHandler::ChangeState(chromeos::CellularNetwork* network,
                                     PlanActivationState new_state,
                                     const std::string& error_description) {
  static bool first_time = true;
  if (state_ == new_state && !first_time)
    return;
  LOG(INFO) << "Activation state flip old = " <<
          GetStateDescription(state_) << ", new = " <<
          GetStateDescription(new_state);
  first_time = false;
  state_ = new_state;
  DictionaryValue device_dict;

  // Signal to JS layer that the state is changing.
  if (network)
    GetDeviceInfo(network, &device_dict);
  device_dict.SetInteger("state", new_state);
  if (error_description.length())
    device_dict.SetString("error", error_description);
  dom_ui_->CallJavascriptFunction(
      kJsDeviceStatusChangedHandler, device_dict);

  // Decide what to do with network object as a result of the new state.
  switch (new_state) {
    case PLAN_ACTIVATION_START:
      break;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      DCHECK(network);
      LOG(INFO) << "Activating service " << network->service_path().c_str();
      if (!network->StartActivation())
        new_state = PLAN_ACTIVATION_ERROR;
      break;
    case PLAN_ACTIVATION_RECONNECTING: {
      DCHECK(network);
      if (network) {
        chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
            ConnectToCellularNetwork(network);
      }
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      return;
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PLAN_ACTIVATION_DONE:
      DCHECK(network);
      CompleteActivation(network);
      break;
    case PLAN_ACTIVATION_ERROR: {
      CompleteActivation(NULL);
      break;
    }
    default:
      break;
  }
}

void MobileSetupHandler::ReEnableOtherConnections() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  if (reenable_ethernet_) {
    reenable_ethernet_ = false;
    lib->EnableEthernetNetworkDevice(true);
  }
  if (reenable_wifi_) {
    reenable_wifi_ = false;
    lib->EnableWifiNetworkDevice(true);
  }

  PrefService* prefs = dom_ui_->GetProfile()->GetPrefs();
  if (reenable_cert_check_) {
    reenable_cert_check_ = false;
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled,
                      reenable_cert_check_);
  }
}


void MobileSetupHandler::SetupActivationProcess(
    chromeos::CellularNetwork* network) {
  if (!network)
    return;

  // Disable SSL cert checks since we will be doing this in
  // restricted pool.
  PrefService* prefs = dom_ui_->GetProfile()->GetPrefs();
  if (!reenable_cert_check_ &&
      prefs->GetBoolean(
          prefs::kCertRevocationCheckingEnabled)) {
    reenable_cert_check_ = true;
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled, false);
  }

  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  // Disable autoconnect to cellular network.
  network->set_auto_connect(false);
  lib->SaveCellularNetwork(network);

  // Disable ethernet and wifi.
  if (!reenable_ethernet_ && lib->ethernet_enabled()) {
    reenable_ethernet_ = true;
    lib->EnableEthernetNetworkDevice(false);
  }
  if (!reenable_wifi_ && lib->wifi_enabled()) {
    reenable_wifi_ = true;
    lib->EnableWifiNetworkDevice(false);
  }
}


bool MobileSetupHandler::GotActivationError(
    const chromeos::CellularNetwork* network, std::string* error) {
  DCHECK(network);
  bool got_error = false;
  const char* error_code = kErrorDefault;

  // This is the magic for detection of errors in during activation process.
  if (network->connection_state() == chromeos::STATE_FAILURE &&
      network->error() == chromeos::ERROR_AAA_FAILED ) {
    if (network->activation_state() ==
            chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
      error_code = kErrorBadConnectionPartial;
    } else if (network->activation_state() ==
            chromeos::ACTIVATION_STATE_ACTIVATED) {
      if (network->roaming_state() == chromeos::ROAMING_STATE_HOME) {
        error_code = kErrorBadConnectionActivated;
      } else if (network->roaming_state() == chromeos::ROAMING_STATE_ROAMING) {
        error_code = kErrorRoamingOnConnection;
      }
    }
    got_error = true;
  } else if (network->connection_state() ==
                 chromeos::STATE_ACTIVATION_FAILURE) {
    if (network->error() == chromeos::ERROR_NEED_EVDO) {
      if (network->activation_state() ==
              chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED)
        error_code = kErrorNoEVDO;
    } else if (network->error() == chromeos::ERROR_NEED_HOME_NETWORK) {
      if (network->activation_state() ==
              chromeos::ACTIVATION_STATE_NOT_ACTIVATED) {
        error_code = kErrorRoamingActivation;
      } else if (network->activation_state() ==
                    chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        error_code = kErrorRoamingPartiallyActivated;
      }
    }
    got_error = true;
  }

  if (got_error)
    *error = GetErrorMessage(error_code);

  return got_error;
}

void MobileSetupHandler::GetDeviceInfo(const chromeos::CellularNetwork* network,
          DictionaryValue* value) {
  value->SetString("carrier", network->name());
  value->SetString("payment_url", network->payment_url());
  value->SetString("MEID", network->meid());
  value->SetString("IMEI", network->imei());
  value->SetString("MDN", network->mdn());
}

std::string MobileSetupHandler::GetErrorMessage(const std::string& code) {
  if (!cellular_config_.get())
    return "";
  return cellular_config_->GetErrorMessage(code);
}

void MobileSetupHandler::LoadCellularConfig() {
  static bool config_loaded = false;
  if (config_loaded)
    return;
  config_loaded = true;
  // Load partner customization startup manifest if it is available.
  FilePath config_path(kCellularConfigPath);
  if (file_util::PathExists(config_path)) {
    scoped_ptr<CellularConfigDocument> config(new CellularConfigDocument());
    bool config_loaded = config->LoadFromFile(config_path);
    if (config_loaded) {
      LOG(INFO) << "Cellular config file loaded: " << kCellularConfigPath;
      // lock
      cellular_config_.reset(config.release());
    } else {
      LOG(ERROR) << "Error loading cellular config file: " <<
          kCellularConfigPath;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUI
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUI::MobileSetupUI(TabContents* contents) : DOMUI(contents) {
  const chromeos::CellularNetwork* network = GetCellularNetwork();
  std::string service_path = network ? network->service_path() : std::string();
  MobileSetupHandler* handler = new MobileSetupHandler(service_path);
  AddMessageHandler((handler)->Attach(this));
  handler->Init(contents);
  MobileSetupUIHTMLSource* html_source =
      new MobileSetupUIHTMLSource(service_path);

  // Set up the chrome://mobilesetup/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
