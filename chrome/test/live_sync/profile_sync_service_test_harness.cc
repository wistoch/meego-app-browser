// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// The default value for min_timestamp_needed_ when we're not in the
// WAITING_FOR_UPDATES state.
static const int kMinTimestampNeededNone = -1;

// Simple class to implement a timeout using PostDelayedTask.  If it is not
// aborted before picked up by a message queue, then it asserts with the message
// provided.  This class is not thread safe.
class StateChangeTimeoutEvent
    : public base::RefCountedThreadSafe<StateChangeTimeoutEvent> {
 public:
  explicit StateChangeTimeoutEvent(ProfileSyncServiceTestHarness* caller,
                                   const std::string& message);

  // The entry point to the class from PostDelayedTask.
  void Callback();

  // Cancels the actions of the callback.  Returns true if success, false
  // if the callback has already timed out.
  bool Abort();

 private:
  friend class base::RefCountedThreadSafe<StateChangeTimeoutEvent>;

  ~StateChangeTimeoutEvent();

  bool aborted_;
  bool did_timeout_;

  // Due to synchronization of the IO loop, the caller will always be alive
  // if the class is not aborted.
  ProfileSyncServiceTestHarness* caller_;

  // Informative message to assert in the case of a timeout.
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(StateChangeTimeoutEvent);
};

StateChangeTimeoutEvent::StateChangeTimeoutEvent(
    ProfileSyncServiceTestHarness* caller,
    const std::string& message)
    : aborted_(false), did_timeout_(false), caller_(caller), message_(message) {
}

StateChangeTimeoutEvent::~StateChangeTimeoutEvent() {
}

void StateChangeTimeoutEvent::Callback() {
  if (!aborted_) {
    if (!caller_->RunStateChangeMachine()) {
      // Report the message.
      did_timeout_ = true;
      EXPECT_FALSE(aborted_) << message_;
      caller_->SignalStateComplete();
    }
  }
}

bool StateChangeTimeoutEvent::Abort() {
  aborted_ = true;
  caller_ = NULL;
  return !did_timeout_;
}

ProfileSyncServiceTestHarness::ProfileSyncServiceTestHarness(Profile* p,
    const std::string& username, const std::string& password, int id)
    : wait_state_(WAITING_FOR_ON_BACKEND_INITIALIZED),
      profile_(p), service_(NULL),
      last_timestamp_(0),
      min_timestamp_needed_(kMinTimestampNeededNone),
      username_(username), password_(password), id_(id) {
  // Ensure the profile has enough prefs registered for use by sync.
  if (!p->GetPrefs()->FindPreference(prefs::kAcceptLanguages))
    TabContents::RegisterUserPrefs(p->GetPrefs());
}

bool ProfileSyncServiceTestHarness::SetupSync() {
  syncable::ModelTypeSet synced_datatypes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
    synced_datatypes.insert(syncable::ModelTypeFromInt(i));
  }
  return SetupSync(synced_datatypes);
}

bool ProfileSyncServiceTestHarness::SetupSync(
    const syncable::ModelTypeSet& synced_datatypes) {
  // Initialize the sync client's profile sync service object.
  service_ = profile_->GetProfileSyncService("");
  if (service_ == NULL) {
    LOG(ERROR) << "SetupSync(): service_ is null.";
    return false;
  }

  // Subscribe sync client to notifications from the profile sync service.
  if (!service_->HasObserver(this))
    service_->AddObserver(this);

  // Authenticate sync client using GAIA credentials.
  service_->signin_.StartSignIn(username_, password_, "", "");

  // Wait for the OnBackendInitialized() callback.
  EXPECT_EQ(wait_state_, WAITING_FOR_ON_BACKEND_INITIALIZED);
  if (!AwaitStatusChangeWithTimeout(TestTimeouts::live_operation_timeout_ms(),
      "Waiting for OnBackendInitialized().")) {
    LOG(ERROR) << "OnBackendInitialized() not seen after "
               << TestTimeouts::live_operation_timeout_ms() / 1000
               << " seconds.";
    return false;
  }

  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything = (synced_datatypes.size() ==
      (syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE));
  service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Wait for initial sync cycle to complete.
  EXPECT_EQ(wait_state_, WAITING_FOR_INITIAL_SYNC);
  if (!AwaitStatusChangeWithTimeout(TestTimeouts::live_operation_timeout_ms(),
      "Waiting for initial sync cycle to complete.")) {
    LOG(ERROR) << "Initial sync cycle did not complete after "
               << TestTimeouts::live_operation_timeout_ms() / 1000
               << " seconds.";
    return false;
  }

  return true;
}

void ProfileSyncServiceTestHarness::SignalStateCompleteWithNextState(
    WaitState next_state) {
  wait_state_ = next_state;
  SignalStateComplete();
}

void ProfileSyncServiceTestHarness::SignalStateComplete() {
  MessageLoopForUI::current()->Quit();
}

bool ProfileSyncServiceTestHarness::RunStateChangeMachine() {
  WaitState original_wait_state = wait_state_;
  switch (wait_state_) {
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      LogClientInfo("WAITING_FOR_ON_BACKEND_INITIALIZED");
      if (service()->sync_initialized()) {
        // The sync backend is initialized. Start waiting for the first sync
        // cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_INITIAL_SYNC);
      }
      break;
    }
    case WAITING_FOR_INITIAL_SYNC: {
      LogClientInfo("WAITING_FOR_INITIAL_SYNC");
      if (IsSynced()) {
        // The first sync cycle is now complete. We can start running tests.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SYNC_TO_FINISH: {
      LogClientInfo("WAITING_FOR_SYNC_TO_FINISH");
      if (!IsSynced()) {
        // The client is not yet fully synced. Continue waiting.
        if (!GetStatus().server_reachable) {
          // The client cannot reach the sync server because the network is
          // disabled. There is no need to wait anymore.
          SignalStateCompleteWithNextState(SERVER_UNREACHABLE);
        }
        break;
      }
      GetUpdatedTimestamp();
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_UPDATES: {
      LogClientInfo("WAITING_FOR_UPDATES");
      if (!IsSynced() || GetUpdatedTimestamp() < min_timestamp_needed_) {
        // The client is not yet fully synced. Continue waiting until the client
        // is at the required minimum timestamp.
        break;
      }
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case SERVER_UNREACHABLE: {
      LogClientInfo("SERVER_UNREACHABLE");
      if (GetStatus().server_reachable) {
        // The client was offline due to the network being disabled, but is now
        // back online. Wait for the pending sync cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_SYNC_TO_FINISH);
      }
      break;
    }
    case FULLY_SYNCED: {
      // The client is online and fully synced. There is nothing to do.
      LogClientInfo("FULLY_SYNCED");
      break;
    }
    case WAITING_FOR_PASSPHRASE_ACCEPTED: {
      LogClientInfo("WAITING_FOR_PASSPHRASE_ACCEPTED");
      if (!service()->observed_passphrase_required())
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case SYNC_DISABLED: {
      // Syncing is disabled for the client. There is nothing to do.
      LogClientInfo("SYNC_DISABLED");
      break;
    }
    default:
      // Invalid state during observer callback which may be triggered by other
      // classes using the the UI message loop.  Defer to their handling.
      break;
  }
  return original_wait_state != wait_state_;
}

void ProfileSyncServiceTestHarness::OnStateChanged() {
  RunStateChangeMachine();
}

bool ProfileSyncServiceTestHarness::AwaitPassphraseAccepted() {
  LogClientInfo("AwaitPassphraseAccepted");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }
  if (!service()->observed_passphrase_required())
    return true;
  wait_state_ = WAITING_FOR_PASSPHRASE_ACCEPTED;
  return AwaitStatusChangeWithTimeout(
      TestTimeouts::live_operation_timeout_ms(),
      "Waiting for passphrase accepted.");
}

bool ProfileSyncServiceTestHarness::AwaitSyncCycleCompletion(
    const std::string& reason) {
  LogClientInfo("AwaitSyncCycleCompletion");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }
  if (!IsSynced()) {
    if (wait_state_ == SERVER_UNREACHABLE) {
      // Client was offline; wait for it to go online, and then wait for sync.
      AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
      EXPECT_EQ(wait_state_, WAITING_FOR_SYNC_TO_FINISH);
      return AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
    } else {
      EXPECT_TRUE(service()->sync_initialized());
      wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
      AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
      if (wait_state_ == FULLY_SYNCED) {
        // Client is online; sync was successful.
        return true;
      } else if (wait_state_ == SERVER_UNREACHABLE){
        // Client is offline; sync was unsuccessful.
        return false;
      } else {
        LOG(ERROR) << "Invalid wait state:" << wait_state_;
        return false;
      }
    }
  } else {
    // Client is already synced; don't wait.
    GetUpdatedTimestamp();
    return true;
  }
}

bool ProfileSyncServiceTestHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceTestHarness* partner) {
  LogClientInfo("AwaitMutualSyncCycleCompletion");
  if (!AwaitSyncCycleCompletion("Sync cycle completion on active client."))
    return false;
  return partner->WaitUntilTimestampIsAtLeast(last_timestamp_,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceTestHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceTestHarness*>& partners) {
  LogClientInfo("AwaitGroupSyncCycleCompletion");
  if (!AwaitSyncCycleCompletion("Sync cycle completion on active client."))
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceTestHarness*>::iterator it =
      partners.begin(); it != partners.end(); ++it) {
    if ((this != *it) && ((*it)->wait_state_ != SYNC_DISABLED)) {
      return_value = return_value &&
          (*it)->WaitUntilTimestampIsAtLeast(last_timestamp_,
          "Sync cycle completion on partner client.");
    }
  }
  return return_value;
}

// static
bool ProfileSyncServiceTestHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceTestHarness*>& clients) {
  VLOG(1) << "AwaitQuiescence.";
  bool return_value = true;
  for (std::vector<ProfileSyncServiceTestHarness*>::iterator it =
      clients.begin(); it != clients.end(); ++it) {
    if ((*it)->wait_state_ != SYNC_DISABLED)
      return_value = return_value &&
          (*it)->AwaitGroupSyncCycleCompletion(clients);
  }
  return return_value;
}

bool ProfileSyncServiceTestHarness::WaitUntilTimestampIsAtLeast(
    int64 timestamp, const std::string& reason) {
  LogClientInfo("WaitUntilTimestampIsAtLeast");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }
  min_timestamp_needed_ = timestamp;
  if (GetUpdatedTimestamp() < min_timestamp_needed_) {
    wait_state_ = WAITING_FOR_UPDATES;
    return AwaitStatusChangeWithTimeout(
        TestTimeouts::live_operation_timeout_ms(), reason);
  } else {
    return true;
  }
}

bool ProfileSyncServiceTestHarness::AwaitStatusChangeWithTimeout(
    int timeout_milliseconds,
    const std::string& reason) {
  LogClientInfo("AwaitStatusChangeWithTimeout");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoopForUI* loop = MessageLoopForUI::current();
  loop->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(timeout_signal.get(),
                        &StateChangeTimeoutEvent::Callback),
      timeout_milliseconds);
  LogClientInfo("Before RunMessageLoop");
  ui_test_utils::RunMessageLoop();
  LogClientInfo("After RunMessageLoop");
  return timeout_signal->Abort();
}

ProfileSyncService::Status ProfileSyncServiceTestHarness::GetStatus() {
  EXPECT_FALSE(service() == NULL) << "GetStatus(): service() is NULL.";
  return service()->QueryDetailedSyncStatus();
}

bool ProfileSyncServiceTestHarness::IsSynced() {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  // TODO(rsimha): Remove additional checks of snap->has_more_to_sync and
  // snap->unsynced_count once http://crbug.com/48989 is fixed.
  return (service() &&
          snap &&
          ServiceIsPushingChanges() &&
          GetStatus().notifications_enabled &&
          !service()->backend()->HasUnsyncedItems() &&
          !snap->has_more_to_sync &&
          snap->unsynced_count == 0);
}

const SyncSessionSnapshot*
    ProfileSyncServiceTestHarness::GetLastSessionSnapshot() const {
  EXPECT_FALSE(service_ == NULL) << "Sync service has not yet been set up.";
  if (service_->backend()) {
    return service_->backend()->GetLastSessionSnapshot();
  }
  return NULL;
}

void ProfileSyncServiceTestHarness::EnableSyncForDatatype(
    syncable::ModelType datatype) {
  LogClientInfo("EnableSyncForDatatype");
  syncable::ModelTypeSet synced_datatypes;
  if (wait_state_ == SYNC_DISABLED) {
    wait_state_ = WAITING_FOR_ON_BACKEND_INITIALIZED;
    synced_datatypes.insert(datatype);
    EXPECT_TRUE(SetupSync(synced_datatypes))
        << "Reinitialization of Client " << id_ << " failed.";
  } else {
    EXPECT_FALSE(service() == NULL)
        << "EnableSyncForDatatype(): service() is null.";
    service()->GetPreferredDataTypes(&synced_datatypes);
    syncable::ModelTypeSet::iterator it = synced_datatypes.find(
        syncable::ModelTypeFromInt(datatype));
    if (it == synced_datatypes.end()) {
      synced_datatypes.insert(syncable::ModelTypeFromInt(datatype));
      service()->OnUserChoseDatatypes(false, synced_datatypes);
      wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
      AwaitSyncCycleCompletion("Waiting for datatype configuration.");
      VLOG(1) << "EnableSyncForDatatype(): Enabled sync for datatype "
              << syncable::ModelTypeToString(datatype) << " on Client " << id_;
    } else {
      VLOG(1) << "EnableSyncForDatatype(): Sync already enabled for datatype "
              << syncable::ModelTypeToString(datatype) << " on Client " << id_;
    }
  }
}

void ProfileSyncServiceTestHarness::DisableSyncForDatatype(
    syncable::ModelType datatype) {
  LogClientInfo("DisableSyncForDatatype");
  syncable::ModelTypeSet synced_datatypes;
  EXPECT_FALSE(service() == NULL)
      << "DisableSyncForDatatype(): service() is null.";
  service()->GetPreferredDataTypes(&synced_datatypes);
  syncable::ModelTypeSet::iterator it = synced_datatypes.find(datatype);
  if (it != synced_datatypes.end()) {
    synced_datatypes.erase(it);
    service()->OnUserChoseDatatypes(false, synced_datatypes);
    AwaitSyncCycleCompletion("Waiting for datatype configuration.");
    VLOG(1) << "DisableSyncForDatatype(): Disabled sync for datatype "
            << syncable::ModelTypeToString(datatype) << " on Client " << id_;
  } else {
    VLOG(1) << "DisableSyncForDatatype(): Sync already disabled for datatype "
            << syncable::ModelTypeToString(datatype) << " on Client " << id_;
  }
}

void ProfileSyncServiceTestHarness::EnableSyncForAllDatatypes() {
  LogClientInfo("EnableSyncForAllDatatypes");
  if (wait_state_ == SYNC_DISABLED) {
    wait_state_ = WAITING_FOR_ON_BACKEND_INITIALIZED;
    EXPECT_TRUE(SetupSync())
        << "Reinitialization of Client " << id_ << " failed.";
  } else {
    syncable::ModelTypeSet synced_datatypes;
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
        i < syncable::MODEL_TYPE_COUNT; ++i) {
      synced_datatypes.insert(syncable::ModelTypeFromInt(i));
    }
    EXPECT_FALSE(service() == NULL)
        << "EnableSyncForAllDatatypes(): service() is null.";
    service()->OnUserChoseDatatypes(true, synced_datatypes);
    wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
    AwaitSyncCycleCompletion("Waiting for datatype configuration.");
    VLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes on "
               "Client " << id_;
  }
}

void ProfileSyncServiceTestHarness::DisableSyncForAllDatatypes() {
  LogClientInfo("DisableSyncForAllDatatypes");
  EXPECT_FALSE(service() == NULL)
      << "EnableSyncForAllDatatypes(): service() is null.";
  service()->DisableForUser();
  wait_state_ = SYNC_DISABLED;
  VLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all datatypes on "
             "Client " << id_;
}

int64 ProfileSyncServiceTestHarness::GetUpdatedTimestamp() {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  EXPECT_FALSE(snap == NULL) << "GetUpdatedTimestamp(): Sync snapshot is NULL.";
  EXPECT_LE(last_timestamp_, snap->max_local_timestamp);
  last_timestamp_ = snap->max_local_timestamp;
  return last_timestamp_;
}

void ProfileSyncServiceTestHarness::LogClientInfo(std::string message) {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  if (snap) {
    VLOG(1) << "Client " << id_ << ": " << message
            << ": max_local_timestamp: " << snap->max_local_timestamp
            << ", has_more_to_sync: " << snap->has_more_to_sync
            << ", unsynced_count: " << snap->unsynced_count
            << ", has_unsynced_items: "
            << service()->backend()->HasUnsyncedItems()
            << ", notifications_enabled: "
            << GetStatus().notifications_enabled
            << ", service_is_pushing_changes: " << ServiceIsPushingChanges();
  } else {
    VLOG(1) << "Client " << id_ << ": " << message
            << ": Sync session snapshot not available.";
  }
}
