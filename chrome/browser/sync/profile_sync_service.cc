// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION
#include "chrome/browser/sync/profile_sync_service.h"

#include <stack>
#include <vector>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "views/window/window.h"

using browser_sync::ChangeProcessor;
using browser_sync::ModelAssociator;
using browser_sync::SyncBackendHost;

// Default sync server URL.
static const char kSyncServerUrl[] = "https://clients4.google.com/chrome-sync";

ProfileSyncService::ProfileSyncService(Profile* profile)
    : last_auth_error_(AUTH_ERROR_NONE),
      profile_(profile),
      sync_service_url_(kSyncServerUrl),
      backend_initialized_(false),
      expecting_first_run_auth_needed_event_(false),
      is_auth_in_progress_(false),
      unrecoverable_error_detected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(wizard_(this)) {
  change_processor_.reset(new ChangeProcessor(this));
}

ProfileSyncService::~ProfileSyncService() {
  Shutdown(false);
}

void ProfileSyncService::set_model_associator(
    browser_sync::ModelAssociator* associator) {
  model_associator_ = associator;
  change_processor_->set_model_associator(associator);
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();
  if (!profile()->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted))
    DisableForUser();  // Clean up in case of previous crash / setup abort.
  else
    StartUp();
}

void ProfileSyncService::InitSettings() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Override the sync server URL from the command-line, if sync server and sync
  // port command-line arguments exist.
  if (command_line.HasSwitch(switches::kSyncServiceURL)) {
    std::wstring value(command_line.GetSwitchValue(switches::kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(WideToUTF8(value));
      if (custom_sync_url.is_valid()) {
        sync_service_url_ = custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }
}

void ProfileSyncService::RegisterPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service->IsPrefRegistered(prefs::kSyncLastSyncedTime))
    return;
  pref_service->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted, false);
}

void ProfileSyncService::ClearPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service->ClearPref(prefs::kSyncHasSetupCompleted);

  pref_service->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::InitializeBackend() {
  backend_->Initialize(sync_service_url_);
}

void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get())
    return;

  last_synced_time_ = base::Time::FromInternalValue(
      profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));

  backend_.reset(new SyncBackendHost(this, profile_->GetPath(),
                                     change_processor_.get()));

  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 Source<Profile>(profile_));

  // Create new model assocation manager and change processor.
  model_associator_ = new ModelAssociator(this);
  change_processor_->set_model_associator(model_associator_);

  // TODO(timsteele): HttpBridgeFactory should take a const* to the profile's
  // URLRequestContext, because it needs it to create HttpBridge objects, and
  // it may need to do that before the default request context has been set
  // up. For now, call GetRequestContext lazy-init to force creation.
  profile_->GetRequestContext();
  InitializeBackend();
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  registrar_.RemoveAll();

  if (backend_.get())
    backend_->Shutdown(sync_disabled);

  change_processor_->Stop();
  backend_.reset();

  // Clear all associations and throw away the association manager instance.
  if (model_associator_.get()) {
    model_associator_->ClearAll();
    model_associator_ = NULL;
  }

  // Clear various flags.
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  expecting_first_run_auth_needed_event_ = false;
  last_attempted_user_email_.clear();
}

void ProfileSyncService::EnableForUser() {
  if (wizard_.IsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  expecting_first_run_auth_needed_event_ = true;

  StartUp();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::DisableForUser() {
  if (wizard_.IsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  Shutdown(true);
  ClearPreferences();

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::BOOKMARK_MODEL_LOADED, type.value);
  registrar_.RemoveAll();
  StartProcessingChangesIfReady();
}

bool ProfileSyncService::MergeAndSyncAcceptanceNeeded() const {
  // If we've shown the dialog before, don't show it again.
  if (profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted))
    return false;

  return model_associator_->BookmarkModelHasUserCreatedNodes() &&
         model_associator_->SyncModelHasUserCreatedNodes();
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void ProfileSyncService::SetSyncSetupCompleted() {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  prefs->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  last_synced_time_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kSyncLastSyncedTime,
      last_synced_time_.ToInternalValue());
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError() {
  unrecoverable_error_detected_ = true;
  change_processor_->Stop();
  LOG(ERROR) << "Unrecoverable error detected -- ProfileSyncService unusable.";
}

void ProfileSyncService::OnBackendInitialized() {
  backend_initialized_ = true;
  StartProcessingChangesIfReady();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null())
    UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnAuthError() {
  last_auth_error_ = backend_->GetAuthErrorState();
  // Protect against the in-your-face dialogs that pop out of nowhere.
  // Require the user to click somewhere to run the setup wizard in the case
  // of a steady-state auth failure.
  if (wizard_.IsVisible() || expecting_first_run_auth_needed_event_) {
    wizard_.Step(AUTH_ERROR_NONE == backend_->GetAuthErrorState() ?
        SyncSetupWizard::GAIA_SUCCESS : SyncSetupWizard::GAIA_LOGIN);
  }

  if (expecting_first_run_auth_needed_event_) {
    last_auth_error_ = AUTH_ERROR_NONE;
    expecting_first_run_auth_needed_event_ = false;
  }

  if (!wizard_.IsVisible()) {
    auth_error_time_ == base::TimeTicks::Now();
  }

  is_auth_in_progress_ = false;
  // Fan the notification out to interested UI-thread components.
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::ShowLoginDialog() {
  if (wizard_.IsVisible())
    return;

  if (!auth_error_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.ReauthorizationTime",
                             base::TimeTicks::Now() - auth_error_time_);
    auth_error_time_ = base::TimeTicks();  // Reset auth_error_time_ to null.
  }

  if (last_auth_error_ != AUTH_ERROR_NONE)
    wizard_.Step(SyncSetupWizard::GAIA_LOGIN);
}

SyncBackendHost::StatusSummary ProfileSyncService::QuerySyncStatusSummary() {
  return backend_->GetStatusSummary();
}

SyncBackendHost::Status ProfileSyncService::QueryDetailedSyncStatus() {
  return backend_->GetDetailedStatus();
}

std::wstring ProfileSyncService::BuildSyncStatusSummaryText(
  const sync_api::SyncManager::Status::Summary& summary) {
  switch (summary) {
    case sync_api::SyncManager::Status::OFFLINE:
      return L"OFFLINE";
    case sync_api::SyncManager::Status::OFFLINE_UNSYNCED:
      return L"OFFLINE_UNSYNCED";
    case sync_api::SyncManager::Status::SYNCING:
      return L"SYNCING";
    case sync_api::SyncManager::Status::READY:
      return L"READY";
    case sync_api::SyncManager::Status::PAUSED:
      return L"PAUSED";
    case sync_api::SyncManager::Status::CONFLICT:
      return L"CONFLICT";
    case sync_api::SyncManager::Status::OFFLINE_UNUSABLE:
      return L"OFFLINE_UNUSABLE";
    case sync_api::SyncManager::Status::INVALID:  // fall through
    default:
      return L"UNKNOWN";
  }
}

std::wstring ProfileSyncService::GetLastSyncedTimeString() const {
  if (last_synced_time_.is_null())
    return l10n_util::GetString(IDS_SYNC_TIME_NEVER);

  base::TimeDelta last_synced = base::Time::Now() - last_synced_time_;

  if (last_synced < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetString(IDS_SYNC_TIME_JUST_NOW);

  return TimeFormat::TimeElapsed(last_synced);
}

string16 ProfileSyncService::GetAuthenticatedUsername() const {
  return backend_->GetAuthenticatedUsername();
}

void ProfileSyncService::OnUserSubmittedAuth(
    const std::string& username, const std::string& password) {
  last_attempted_user_email_ = username;
  is_auth_in_progress_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());

  base::TimeTicks start_time = base::TimeTicks::Now();
  backend_->Authenticate(username, password);
  UMA_HISTOGRAM_TIMES("Sync.AuthorizationTime",
                      base::TimeTicks::Now() - start_time);
}

void ProfileSyncService::OnUserAcceptedMergeAndSync() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationWithUITime",
                      base::TimeTicks::Now() - start_time);

  wizard_.Step(SyncSetupWizard::DONE);  // TODO(timsteele): error state?
  if (!merge_success) {
    LOG(ERROR) << "Model assocation failed.";
    OnUnrecoverableError();
    return;
  }

  change_processor_->Start(profile_->GetBookmarkModel(),
                           backend_->GetUserShareHandle());
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    // A sync dialog was aborted before authentication or merge acceptance.
    // Rollback.
    DisableForUser();
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::StartProcessingChangesIfReady() {
  BookmarkModel* model = profile_->GetBookmarkModel();

  DCHECK(!change_processor_->IsRunning());

  // First check if the subsystems are ready.  We can't proceed until they
  // both have finished loading.
  if (!model->IsLoaded())
    return;
  if (!backend_initialized_)
    return;

  // Show the sync merge warning dialog if needed.
  if (MergeAndSyncAcceptanceNeeded()) {
    ProfileSyncService::SyncEvent(MERGE_AND_SYNC_NEEDED);
    wizard_.Step(SyncSetupWizard::MERGE_AND_SYNC);
    return;
  }

  // We're ready to merge the models.
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationTime",
                      base::TimeTicks::Now() - start_time);

  wizard_.Step(SyncSetupWizard::DONE);  // TODO(timsteele): error state?
  if (!merge_success) {
    LOG(ERROR) << "Model assocation failed.";
    OnUnrecoverableError();
    return;
  }

  change_processor_->Start(profile_->GetBookmarkModel(),
                           backend_->GetUserShareHandle());
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  static LinearHistogram histogram("Sync.EventCodes", MIN_SYNC_EVENT_CODE,
                                   MAX_SYNC_EVENT_CODE - 1,
                                   MAX_SYNC_EVENT_CODE);
  histogram.SetFlags(kUmaTargetedHistogramFlag);
  histogram.Add(code);
}

bool ProfileSyncService::ShouldPushChanges() {
  // True only after all bootstrapping has succeeded: the bookmark model is
  // loaded, the sync backend is initialized, the two domains are
  // consistent with one another, and no unrecoverable error has transpired.
  return change_processor_->IsRunning();
}

#endif  // CHROME_PERSONALIZATION
