// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The sync process consists of a sequence of sync cycles, each of which
// (hopefully) moves the client into closer synchronization with the server.
// While SyncCycleState holds state that is pertinent to a single sync cycle,
// this data structure holds state that must be passed from cycle to cycle.
//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNC_PROCESS_STATE_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNC_PROCESS_STATE_H_

#include <map>
#include <set>
#include <string>
#include <utility>  // for pair<>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/port.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace browser_sync {

class ConflictResolver;
class ModelSafeWorker;

class SyncProcessState {
  FRIEND_TEST(SyncerSyncProcessState, MergeSetsTest);
  FRIEND_TEST(SyncerTest, CopySyncProcessState);
 public:
  ~SyncProcessState();
  SyncProcessState(
      syncable::DirectoryManager* dirman,
      std::string account_name,
      ServerConnectionManager* connection_manager,
      ConflictResolver* const resolver,
      SyncerEventChannel* syncer_event_channel,
      ModelSafeWorker* model_safe_worker);

  // Intentionally not 'explicit' b/c it's a copy ctor:
  SyncProcessState(const SyncProcessState& counts);
  SyncProcessState& operator=(const SyncProcessState& that);

  std::string account_name() const { return account_name_; }

  syncable::DirectoryManager* dirman() const { return dirman_; }

  ServerConnectionManager* connection_manager() const {
    return connection_manager_;
  }

  ConflictResolver* resolver() const { return resolver_; }

  ModelSafeWorker* model_safe_worker() { return model_safe_worker_; }

  SyncerEventChannel* syncer_event_channel() const {
    return syncer_event_channel_;
  }

  // Functions that deal with conflict set stuff.
  IdToConflictSetMap::const_iterator IdToConflictSetFind(
      const syncable::Id& the_id) const {
    return id_to_conflict_set_.find(the_id);
  }

  IdToConflictSetMap::const_iterator IdToConflictSetBegin() const {
    return id_to_conflict_set_.begin();
  }

  IdToConflictSetMap::const_iterator IdToConflictSetEnd() const {
    return id_to_conflict_set_.end();
  }

  IdToConflictSetMap::size_type IdToConflictSetSize() const {
    return id_to_conflict_set_.size();
  }

  const ConflictSet* IdToConflictSetGet(const syncable::Id& the_id) {
    return id_to_conflict_set_[the_id];
  }

  std::set<ConflictSet*>::const_iterator ConflictSetsBegin() const {
    return conflict_sets_.begin();
  }

  std::set<ConflictSet*>::const_iterator ConflictSetsEnd() const {
    return conflict_sets_.end();
  }

  std::set<ConflictSet*>::size_type ConflictSetsSize() const {
    return conflict_sets_.size();
  }

  void MergeSets(const syncable::Id& set1, const syncable::Id& set2);

  void CleanupSets();
  // END conflict set functions

  // Item id set manipulation functions.
  bool HasConflictingItems() const {
    return !conflicting_item_ids_.empty();
  }

  int ConflictingItemsSize() const {
    return conflicting_item_ids_.size();
  }

  void AddConflictingItem(const syncable::Id& the_id) {
    std::pair<std::set<syncable::Id>::iterator, bool> ret =
        conflicting_item_ids_.insert(the_id);
    UpdateDirty(ret.second);
  }

  void EraseConflictingItem(std::set<syncable::Id>::iterator it) {
    UpdateDirty(true);
    conflicting_item_ids_.erase(it);
  }

  void EraseConflictingItem(const syncable::Id& the_id) {
    int items_erased = conflicting_item_ids_.erase(the_id);
    UpdateDirty(0 != items_erased);
  }

  std::set<syncable::Id>::iterator ConflictingItemsBegin() {
    return conflicting_item_ids_.begin();
  }

  std::set<syncable::Id>::iterator ConflictingItemsEnd() {
    return conflicting_item_ids_.end();
  }

  // END item id set manipulation functions

  // Assorted other state info.
  // DEPRECATED: USE ConflictingItemsSize.
  int conflicting_updates() const { return conflicting_item_ids_.size(); }

  base::TimeTicks silenced_until() const { return silenced_until_; }
  void set_silenced_until(const base::TimeTicks& val);

  // Info that is tracked purely for status reporting.

  // During inital sync these two members can be used to measure sync progress.
  int64 current_sync_timestamp() const { return current_sync_timestamp_; }

  int64 num_server_changes_remaining() const { return num_server_changes_remaining_; }

  void set_current_sync_timestamp(const int64 val);

  void set_num_server_changes_remaining(const int64 val);

  bool invalid_store() const { return invalid_store_; }

  void set_invalid_store(const bool val);

  bool syncer_stuck() const { return syncer_stuck_; }

  void set_syncer_stuck(const bool val);

  bool syncing() const { return syncing_; }

  void set_syncing(const bool val);

  bool IsShareUsable() const;

  int conflicting_commits() const { return conflicting_commits_; }

  void set_conflicting_commits(const int val);

  // WEIRD COUNTER manipulation functions.
  int consecutive_problem_get_updates() const {
    return consecutive_problem_get_updates_;
  }

  void increment_consecutive_problem_get_updates();

  void zero_consecutive_problem_get_updates();

  int consecutive_problem_commits() const {
    return consecutive_problem_commits_;
  }

  void increment_consecutive_problem_commits();

  void zero_consecutive_problem_commits();

  int consecutive_transient_error_commits() const {
    return consecutive_transient_error_commits_;
  }

  void increment_consecutive_transient_error_commits_by(int value);

  void zero_consecutive_transient_error_commits();

  int consecutive_errors() const { return consecutive_errors_; }

  void increment_consecutive_errors_by(int value);

  void zero_consecutive_errors();

  int successful_commits() const { return successful_commits_; }

  void increment_successful_commits();

  void zero_successful_commits();
  // end WEIRD COUNTER manipulation functions.

  // Methods for tracking authentication state.
  void AuthFailed();

  // Returns true if this object has been modified since last SetClean() call.
  bool IsDirty() const { return dirty_; }

  // Call to tell this status object that its new state has been seen.
  void SetClean() { dirty_ = false; }

  // Returns true if auth status has been modified since last SetClean() call.
  bool IsAuthDirty() const { return auth_dirty_; }

  // Call to tell this status object that its auth state has been seen.
  void SetAuthClean() { auth_dirty_ = false; }

 private:
  // For testing.
  SyncProcessState()
      : connection_manager_(NULL),
        account_name_(PSTR("")),
        dirman_(NULL),
        resolver_(NULL),
        model_safe_worker_(NULL),
        syncer_event_channel_(NULL),
        current_sync_timestamp_(0),
        num_server_changes_remaining_(0),
        syncing_(false),
        invalid_store_(false),
        syncer_stuck_(false),
        conflicting_commits_(0),
        consecutive_problem_get_updates_(0),
        consecutive_problem_commits_(0),
        consecutive_transient_error_commits_(0),
        consecutive_errors_(0),
        successful_commits_(0),
        dirty_(false),
        auth_dirty_(false),
        auth_failed_(false) {}

  ServerConnectionManager* connection_manager_;
  const std::string account_name_;
  syncable::DirectoryManager* const dirman_;
  ConflictResolver* const resolver_;
  ModelSafeWorker* const model_safe_worker_;

  // For sending notifications from sync commands out to observers of the
  // Syncer.
  SyncerEventChannel* syncer_event_channel_;

  // TODO(sync): move away from sets if it makes more sense.
  std::set<syncable::Id> conflicting_item_ids_;
  std::map<syncable::Id, ConflictSet*> id_to_conflict_set_;
  std::set<ConflictSet*> conflict_sets_;

  // When we're over bandwidth quota, we don't update until past this time.
  base::TimeTicks silenced_until_;

  // Status information, as opposed to state info that may also be exposed for
  // status reporting purposes.
  int64 current_sync_timestamp_;  // During inital sync these two members
  int64 num_server_changes_remaining_;  // Can be used to measure sync progress.

  // There remains sync state updating in:
  //   CommitUnsyncedEntries
  bool syncing_;

  // True when we get such an INVALID_STORE error from the server.
  bool invalid_store_;
  // True iff we're stuck. User should contact support.
  bool syncer_stuck_;
  // counts of various commit return values.
  int error_commits_;
  int conflicting_commits_;

  // WEIRD COUNTERS
  // Two variables that track the # on consecutive problem requests.
  // consecutive_problem_get_updates_ resets when we get any updates (not on
  // pings) and increments whenever the request fails.
  int consecutive_problem_get_updates_;
  // consecutive_problem_commits_ resets whenever we commit any number of items
  // and increments whenever all commits fail for any reason.
  int consecutive_problem_commits_;
  // number of commits hitting transient errors since the last successful
  // commit.
  int consecutive_transient_error_commits_;
  // Incremented when get_updates fails, commit fails, and when hitting
  // transient errors. When any of these succeed, this counter is reset.
  // TODO(chron): Reduce number of weird counters we use.
  int consecutive_errors_;
  int successful_commits_;

  bool dirty_;
  bool auth_dirty_;
  bool auth_failed_;

  void UpdateDirty(bool new_info) { dirty_ |= new_info; }

  void UpdateAuthDirty(bool new_info) { auth_dirty_ |= new_info; }
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNC_PROCESS_STATE_H_
