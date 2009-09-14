// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/views/sync/sync_setup_wizard.h"

#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/sync/sync_setup_flow.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"

class SyncResourcesSource : public ChromeURLDataManager::DataSource {
 public:
  SyncResourcesSource()
      : DataSource(chrome::kSyncResourcesHost, MessageLoop::current()) {
  }
  virtual ~SyncResourcesSource() { }

  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string& path) const {
    if (path == chrome::kSyncThrobberPath)
      return "image/png";
    else
      return "text/html";
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncResourcesSource);
};

void SyncResourcesSource::StartDataRequest(const std::string& path_raw,
                                           int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  if (path_raw == chrome::kSyncThrobberPath) {
    ResourceBundle::GetSharedInstance().LoadImageResourceBytes(IDR_THROBBER,
        &html_bytes->data);
    SendResponse(request_id, html_bytes);
    return;
  }

  std::string response;
  if (path_raw == chrome::kSyncGaiaLoginPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString(L"settingupsync",
                                "Setting up Bookmarks Sync");
    localized_strings.SetString(L"errorsigningin", "Error signing in");
    localized_strings.SetString(L"introduction",
        "Google Chrome can store your bookmark data with your Google account."
        "Bookmarks that you create on this computer will instantly be made"
        "available on all the computers synced to this account.");
    localized_strings.SetString(L"signinwithyour", "Sign in with your");
    localized_strings.SetString(L"accountlabel", "Account");
    localized_strings.SetString(L"cannotbeblank",
                                "Required field cannot be left blank");
    localized_strings.SetString(L"passwordlabel", "Password:");
    localized_strings.SetString(L"emaillabel", "Email:");
    localized_strings.SetString(L"invalidcredentials",
                                "Username and password do not match.");
    localized_strings.SetString(L"couldnotconnect",
                                "Could not connect to the server");
    localized_strings.SetString(L"cannotaccessaccount",
                                "I cannot access my account");
    localized_strings.SetString(L"createaccount",
                                "Create a Google Account");

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_GAIA_LOGIN_HTML));

    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == chrome::kSyncMergeAndSyncPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString(L"mergeandsyncwarning",
        "Your existing online bookmarks will be merged with the "
        "bookmarks on this machine. You can use the Bookmark Manager to "
        "organize your bookmarks after the merge.");
    localized_strings.SetString(L"titlewarning",
                                "Your bookmarks will be merged.");
    localized_strings.SetString(L"mergeandsynclabel", "Merge and sync");
    localized_strings.SetString(L"abortlabel", "Abort");
    localized_strings.SetString(L"alldone", "All Done!");
    localized_strings.SetString(L"closelabel", "Close");

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_MERGE_AND_SYNC_HTML));

    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == chrome::kSyncSetupFlowPath) {
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_SYNC_SETUP_FLOW_HTML));
    response = html.as_string();
  }
  // Send the response.
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

SyncSetupWizard::SyncSetupWizard(ProfileSyncService* service)
      : service_(service), flow_container_(new SyncSetupFlowContainer()) {
  // Register data sources for HTML content we require.
  // g_browser_process and/or io_thread may not exist during testing.
  if (g_browser_process && g_browser_process->io_thread()) {
    // Add our network layer data source for 'cloudy' URLs.
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
                          &ChromeURLDataManager::AddDataSource,
                          new SyncResourcesSource()));
  }
}

SyncSetupWizard::~SyncSetupWizard() {
  delete flow_container_;
}

void SyncSetupWizard::Step(State advance_state) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow) {
    // A setup flow is in progress and dialog is currently showing.
    flow->Advance(advance_state);
  } else if (!service_->profile()->GetPrefs()->GetBoolean(
             prefs::kSyncHasSetupCompleted)) {
    if (advance_state == DONE || advance_state == GAIA_SUCCESS)
      return;
    // No flow is in progress, and we have never escorted the user all the
    // way through the wizard flow.
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state, DONE));
  } else {
    // No flow in in progress, but we've finished the wizard flow once before.
    // This is just a discrete run.
    if (advance_state == DONE || advance_state == GAIA_SUCCESS)
      return;  // Nothing to do.
    flow_container_->set_flow(SyncSetupFlow::Run(service_, flow_container_,
        advance_state, GetEndStateForDiscreteRun(advance_state)));
  }
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL;
}

// static
SyncSetupWizard::State SyncSetupWizard::GetEndStateForDiscreteRun(
    State start_state) {
  State result = start_state == GAIA_LOGIN ? GAIA_SUCCESS : DONE;
  DCHECK_NE(DONE, result) <<
      "Invalid start state for discrete run: " << start_state;
  return result;
}

#endif  // CHROME_PERSONALIZATION
