// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#include "chrome/browser/sync/engine/sync_process_state.h"

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

using std::map;
using std::set;
using std::vector;

namespace browser_sync {

SyncProcessState::SyncProcessState(const SyncProcessState& counts)
    : connection_manager_(counts.connection_manager_),
      account_name_(counts.account_name_),
      dirman_(counts.dirman_),
      resolver_(counts.resolver_),
      model_safe_worker_(counts.model_safe_worker_),
      syncer_event_channel_(counts.syncer_event_channel_) {
  *this = counts;
}

SyncProcessState::SyncProcessState(syncable::DirectoryManager* dirman,
                                   PathString account_name,
                                   ServerConnectionManager* connection_manager,
                                   ConflictResolver* const resolver,
                                   SyncerEventChannel* syncer_event_channel,
                                   ModelSafeWorker* model_safe_worker)
    : num_sync_cycles_(0),
      silenced_until_(0),
      connection_manager_(connection_manager),
      account_name_(account_name),
      dirman_(dirman),
      resolver_(resolver),
      model_safe_worker_(model_safe_worker),
      syncer_event_channel_(syncer_event_channel),
      error_rate_(0),
      current_sync_timestamp_(0),
      servers_latest_timestamp_(0),
      syncing_(false),
      invalid_store_(false),
      syncer_stuck_(false),
      error_commits_(0),
      conflicting_commits_(0),
      stalled_commits_(0),
      consecutive_problem_get_updates_(0),
      consecutive_problem_commits_(0),
      consecutive_transient_error_commits_(0),
      consecutive_errors_(0),
      successful_commits_(0),
      dirty_(false),
      auth_dirty_(false),
      auth_failed_(false) {
  syncable::ScopedDirLookup dir(dirman_, account_name_);

  // The directory must be good here.
  LOG_IF(ERROR, !dir.good());
  syncing_ = !dir->initial_sync_ended();

  // If we have never synced then we are invalid until made otherwise.
  set_invalid_store((dir->last_sync_timestamp() <= 0));
}

SyncProcessState& SyncProcessState::operator=(const SyncProcessState& counts) {
  if (this == &counts) {
    return *this;
  }
  CleanupSets();
  num_sync_cycles_ = counts.num_sync_cycles_;
  silenced_until_ = counts.silenced_until_;
  error_rate_ = counts.error_rate_;
  current_sync_timestamp_ = counts.current_sync_timestamp_;
  servers_latest_timestamp_ = counts.servers_latest_timestamp_;
  error_commits_ = counts.error_commits_;
  stalled_commits_ = counts.stalled_commits_;
  conflicting_commits_ = counts.conflicting_commits_;
  consecutive_problem_get_updates_ =
      counts.consecutive_problem_get_updates_;
  consecutive_problem_commits_ =
      counts.consecutive_problem_commits_;
  consecutive_transient_error_commits_ =
      counts.consecutive_transient_error_commits_;
  consecutive_errors_ = counts.consecutive_errors_;
  conflicting_item_ids_ = counts.conflicting_item_ids_;
  blocked_item_ids_ = counts.blocked_item_ids_;
  successful_commits_ = counts.successful_commits_;
  syncer_stuck_ = counts.syncer_stuck_;

  // TODO(chron): Is it safe to set these?
  //
  // Pointers:
  //
  // connection_manager_
  // account_name_
  // dirman_
  // model_safe_worker_
  // syncer_event_channel_
  //
  // Status members:
  // syncing_
  // invalid_store_
  // syncer_stuck_
  // got_zero_updates_
  // dirty_
  // auth_dirty_
  // auth_failed_

  for (set<ConflictSet*>::const_iterator it =
           counts.ConflictSetsBegin();
       counts.ConflictSetsEnd() != it; ++it) {
    const ConflictSet* old_set = *it;
    ConflictSet* const new_set = new ConflictSet(*old_set);
    conflict_sets_.insert(new_set);

    for (ConflictSet::const_iterator setit = new_set->begin();
         new_set->end() != setit; ++setit) {
      id_to_conflict_set_[*setit] = new_set;
    }
  }
  return *this;
}

// Status maintenance functions.
void SyncProcessState::set_invalid_store(const bool val) {
  UpdateDirty(val != invalid_store_);
  invalid_store_ = val;
}

void SyncProcessState::set_syncer_stuck(const bool val) {
  UpdateDirty(val != syncer_stuck_);
  syncer_stuck_ = val;
}

void SyncProcessState::set_syncing(const bool val) {
  UpdateDirty(val != syncing_);
  syncing_ = val;
}

// Returns true if got zero updates has been set on the directory.
bool SyncProcessState::IsShareUsable() const {
  syncable::ScopedDirLookup dir(dirman(), account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return false;
  }
  return dir->initial_sync_ended();
}

void SyncProcessState::set_current_sync_timestamp(const int64 val) {
  UpdateDirty(val != current_sync_timestamp_);
  current_sync_timestamp_ = val;
}

void SyncProcessState::set_servers_latest_timestamp(const int64 val) {
  UpdateDirty(val != servers_latest_timestamp_);
  servers_latest_timestamp_ = val;
}

void SyncProcessState::set_error_commits(const int val) {
  UpdateDirty(val != error_commits_);
  error_commits_ = val;
}

void SyncProcessState::set_stalled_commits(const int val) {
  UpdateDirty(val != conflicting_commits_);
  conflicting_commits_ = val;
}

void SyncProcessState::set_conflicting_commits(const int val) {
  UpdateDirty(val != stalled_commits_);
  stalled_commits_ = val;
}

// WEIRD COUNTER functions.
void SyncProcessState::increment_consecutive_problem_get_updates() {
  UpdateDirty(true);
  ++consecutive_problem_get_updates_;
}

void SyncProcessState::zero_consecutive_problem_get_updates() {
  UpdateDirty(0 != consecutive_problem_get_updates_);
  consecutive_problem_get_updates_ = 0;
}

void SyncProcessState::increment_consecutive_problem_commits() {
  UpdateDirty(true);
  ++consecutive_problem_commits_;
}

void SyncProcessState::zero_consecutive_problem_commits() {
  UpdateDirty(0 != consecutive_problem_commits_);
  consecutive_problem_commits_ = 0;
}

void SyncProcessState::increment_consecutive_transient_error_commits_by(
    int value) {
  UpdateDirty(0 != value);
  consecutive_transient_error_commits_ += value;
}

void SyncProcessState::zero_consecutive_transient_error_commits() {
  UpdateDirty(0 != consecutive_transient_error_commits_);
  consecutive_transient_error_commits_ = 0;
}

void SyncProcessState::increment_consecutive_errors_by(int value) {
  UpdateDirty(0 != value);
  consecutive_errors_ += value;
}

void SyncProcessState::zero_consecutive_errors() {
  UpdateDirty(0 != consecutive_errors_);
  consecutive_errors_ = 0;
}

void SyncProcessState::increment_successful_commits() {
  UpdateDirty(true);
  ++successful_commits_;
}

void SyncProcessState::zero_successful_commits() {
  UpdateDirty(0 != successful_commits_);
  successful_commits_ = 0;
}

// Methods for managing error rate tracking.
void SyncProcessState::TallyNewError() {
  UpdateDirty(true);
  error_rate_ += (65536 - error_rate_) >> 2;
}

void SyncProcessState::TallyBigNewError() {
  UpdateDirty(true);
  error_rate_ += (65536 - error_rate_) >> 2;
}

void SyncProcessState::ForgetOldError() {
  error_rate_ -= error_rate_ >> 2;
}

void SyncProcessState::CheckErrorRateTooHigh() {
  UpdateDirty(error_rate_ > ERROR_THRESHOLD);
}


void SyncProcessState::MergeSets(const syncable::Id& id1,
                                 const syncable::Id& id2) {
  // There are no single item sets, we just leave those entries == 0
  vector<syncable::Id>* set1 = id_to_conflict_set_[id1];
  vector<syncable::Id>* set2 = id_to_conflict_set_[id2];
  vector<syncable::Id>* rv = 0;
  if (0 == set1 && 0 == set2) {
    // Neither item currently has a set so we build one.
    rv = new vector<syncable::Id>();
    rv->push_back(id1);
    if (id1 != id2) {
      rv->push_back(id2);
    } else {
      LOG(WARNING) << "[BUG] Attempting to merge two identical conflict ids.";
    }
    conflict_sets_.insert(rv);
  } else if (0 == set1) {
    // Add the item to the existing set.
    rv = set2;
    rv->push_back(id1);
  } else if (0 == set2) {
    // Add the item to the existing set.
    rv = set1;
    rv->push_back(id2);
  } else if (set1 == set2) {
    // It's the same set already.
    return;
  } else {
    // Merge the two sets.
    rv = set1;
    // Point all the second sets id's back to the first.
    vector<syncable::Id>::iterator i;
    for (i = set2->begin() ; i != set2->end() ; ++i) {
      id_to_conflict_set_[*i] = rv;
    }
    // Copy the second set to the first.
    rv->insert(rv->end(), set2->begin(), set2->end());
    conflict_sets_.erase(set2);
    delete set2;
  }
  id_to_conflict_set_[id1] = id_to_conflict_set_[id2] = rv;
}

void SyncProcessState::CleanupSets() {
  // Clean up all the sets.
  set<ConflictSet*>::iterator i;
  for (i = conflict_sets_.begin(); i != conflict_sets_.end(); i++) {
    delete *i;
  }
  conflict_sets_.clear();
  id_to_conflict_set_.clear();
}

SyncProcessState::~SyncProcessState() {
  CleanupSets();
}

void SyncProcessState::AuthFailed() {
  // Dirty if the last one DIDN'T fail.
  UpdateAuthDirty(true != auth_failed_);
  auth_failed_ = true;
}

void SyncProcessState::AuthSucceeded() {
  // Dirty if the last one DID fail.
  UpdateAuthDirty(false != auth_failed_);
  auth_failed_ = false;
}

}  // namespace browser_sync
