// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock ServerConnectionManager class for use in client unit tests.

#ifndef CHROME_TEST_SYNC_ENGINE_MOCK_SERVER_CONNECTION_H_
#define CHROME_TEST_SYNC_ENGINE_MOCK_SERVER_CONNECTION_H_

#include <bitset>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/closure.h"

namespace syncable {
class DirectoryManager;
class ScopedDirLookup;
}
namespace browser_sync {
struct HttpResponse;
}

class MockConnectionManager : public browser_sync::ServerConnectionManager {
 public:
  class MidCommitObserver {
   public:
    virtual void Observe() = 0;
  };

  MockConnectionManager(syncable::DirectoryManager* dirmgr,
                        const std::string& name);
  virtual ~MockConnectionManager();

  // Overridden ServerConnectionManager functions.
  virtual bool PostBufferToPath(const PostBufferParams*,
      const string& path,
      const string& auth_token,
      browser_sync::ScopedServerStatusWatcher* watcher);

  virtual bool IsServerReachable();
  virtual bool IsUserAuthenticated();

  // Control of commit response.
  void SetMidCommitCallback(Closure* callback);
  void SetMidCommitObserver(MidCommitObserver* observer);

  // Set this if you want commit to perform commit time rename. Will request
  // that the client renames all commited entries, prepending this string.
  void SetCommitTimeRename(string prepend);

  // Control of get updates response. All updates set will only be returned
  // once. This mock object doesn't simulate a changelist, it simulates server
  // responses.
  void ResetUpdates();
  // Generic versions of AddUpdate functions. Tests using these function should
  // compile for both the int64 and string id based versions of the server.
  // The SyncEntity returned is only valid until the Sync is completed
  // (e.g. with SyncShare.) It allows to add further entity properties before
  // sync, using AddUpdateExtendedAttributes.
  sync_pb::SyncEntity* AddUpdateDirectory(syncable::Id id,
                                          syncable::Id parent_id,
                                          string name,
                                          int64 version,
                                          int64 sync_ts);
  sync_pb::SyncEntity* AddUpdateBookmark(syncable::Id id,
                                         syncable::Id parent_id,
                                         string name,
                                         int64 version,
                                         int64 sync_ts);
  // Versions of the AddUpdate functions that accept integer IDs.
  sync_pb::SyncEntity* AddUpdateDirectory(int id,
                                          int parent_id,
                                          string name,
                                          int64 version,
                                          int64 sync_ts);
  sync_pb::SyncEntity* AddUpdateBookmark(int id,
                                         int parent_id,
                                         string name,
                                         int64 version,
                                         int64 sync_ts);
  // New protocol versions of the AddUpdate functions.
  sync_pb::SyncEntity* AddUpdateDirectory(string id,
                                          string parent_id,
                                          string name,
                                          int64 version,
                                          int64 sync_ts);
  sync_pb::SyncEntity* AddUpdateBookmark(string id,
                                         string parent_id,
                                         string name,
                                         int64 version,
                                         int64 sync_ts);
  void AddUpdateExtendedAttributes(sync_pb::SyncEntity* ent,
                                   std::string* xattr_key,
                                   syncable::Blob* xattr_value,
                                   int xattr_count);

  void SetLastUpdateDeleted();
  void SetLastUpdateServerTag(const string& tag);
  void SetLastUpdateClientTag(const string& tag);
  void SetLastUpdateOriginatorFields(const string& client_id,
                                     const string& entry_id);
  void SetLastUpdatePosition(int64 position_in_parent);
  void SetNewTimestamp(int64 ts);
  void SetChangesRemaining(int64 timestamp);

  // For AUTHENTICATE responses.
  void SetAuthenticationResponseInfo(const std::string& valid_auth_token,
                                     const std::string& user_display_name,
                                     const std::string& user_display_email,
                                     const std::string& user_obfuscated_id);

  void FailNextPostBufferToPathCall() { fail_next_postbuffer_ = true; }

  // A visitor class to allow a test to change some monitoring state atomically
  // with the action of overriding the response codes sent back to the Syncer
  // (for example, so you can say "ThrottleNextRequest, and assert no more
  // requests are made once throttling is in effect" in one step.
  class ResponseCodeOverrideRequestor {
   public:
    // Called with response_code_override_lock_ acquired.
    virtual void OnOverrideComplete() = 0;
  };
  void ThrottleNextRequest(ResponseCodeOverrideRequestor* visitor);
  void FailWithAuthInvalid(ResponseCodeOverrideRequestor* visitor);
  void StopFailingWithAuthInvalid(ResponseCodeOverrideRequestor* visitor);
  void FailNonPeriodicGetUpdates() { fail_non_periodic_get_updates_ = true; }

  // Simple inspectors.
  bool client_stuck() const { return client_stuck_; }

  sync_pb::ClientCommand* GetNextClientCommand();

  const std::vector<syncable::Id>& committed_ids() const {
    return committed_ids_;
  }
  const std::vector<sync_pb::CommitMessage*>& commit_messages() const {
    return commit_messages_;
  }
  // Retrieve the last sent commit message.
  const sync_pb::CommitMessage& last_sent_commit() const;

  void set_conflict_all_commits(bool value) {
    conflict_all_commits_ = value;
  }
  void set_next_new_id(int value) {
    next_new_id_ = value;
  }
  void set_conflict_n_commits(int value) {
    conflict_n_commits_ = value;
  }

  void set_use_legacy_bookmarks_protocol(bool value) {
    use_legacy_bookmarks_protocol_ = value;
  }

  // Retrieve the number of GetUpdates requests that the mock server has
  // seen since the last time this function was called.  Can be used to
  // verify that a GetUpdates actually did or did not happen after running
  // the syncer.
  int GetAndClearNumGetUpdatesRequests() {
    int result = num_get_updates_requests_;
    num_get_updates_requests_ = 0;
    return result;
  }

  // Expect that GetUpdates will request exactly the types indicated in
  // the bitset.
  void ExpectGetUpdatesRequestTypes(
      std::bitset<syncable::MODEL_TYPE_COUNT> expected_filter) {
    expected_filter_ = expected_filter;
  }

 private:
  sync_pb::SyncEntity* AddUpdateFull(syncable::Id id, syncable::Id parentid,
                                     string name, int64 version,
                                     int64 sync_ts,
                                     bool is_dir);
  sync_pb::SyncEntity* AddUpdateFull(string id, string parentid, string name,
                                     int64 version, int64 sync_ts,
                                     bool is_dir);
  // Functions to handle the various types of server request.
  void ProcessGetUpdates(sync_pb::ClientToServerMessage* csm,
                         sync_pb::ClientToServerResponse* response);
  void ProcessAuthenticate(sync_pb::ClientToServerMessage* csm,
                           sync_pb::ClientToServerResponse* response,
                           const std::string& auth_token);
  void ProcessCommit(sync_pb::ClientToServerMessage* csm,
                     sync_pb::ClientToServerResponse* response_buffer);

  void AddDefaultBookmarkData(sync_pb::SyncEntity* entity, bool is_folder);

  // Locate the most recent update message for purpose of alteration.
  sync_pb::SyncEntity* GetMutableLastUpdate();

  // Determine if one entry in a commit should be rejected with a conflict.
  bool ShouldConflictThisCommit();

  // Generate a numeric position_in_parent value.  We use a global counter
  // that only decreases; this simulates new objects always being added to the
  // front of the ordering.
  int64 GeneratePositionInParent() {
    return next_position_in_parent_--;
  }

  // Determine whether an EntitySpecifics filter (like that sent in
  // GetUpdates.requested_types) indicates that a particular ModelType
  // should be included.
  bool IsModelTypePresentInSpecifics(const sync_pb::EntitySpecifics& filter,
      syncable::ModelType value);

  // All IDs that have been committed.
  std::vector<syncable::Id> committed_ids_;

  // Control of when/if we return conflicts.
  bool conflict_all_commits_;
  int conflict_n_commits_;

  // Commit messages we've sent
  std::vector<sync_pb::CommitMessage*> commit_messages_;

  // The next id the mock will return to a commit.
  int next_new_id_;

  // The store birthday we send to the client.
  string store_birthday_;
  bool store_birthday_sent_;
  bool client_stuck_;
  string commit_time_rename_prepended_string_;

  // Fail on the next call to PostBufferToPath().
  bool fail_next_postbuffer_;

  // Our directory.
  syncable::DirectoryManager* directory_manager_;
  std::string directory_name_;

  // The updates we'll return to the next request.
  sync_pb::GetUpdatesResponse updates_;
  Closure* mid_commit_callback_;
  MidCommitObserver* mid_commit_observer_;

  // The AUTHENTICATE response we'll return for auth requests.
  sync_pb::AuthenticateResponse auth_response_;
  // What we use to determine if we should return SUCCESS or BAD_AUTH_TOKEN.
  std::string valid_auth_token_;

  // Whether we are faking a server mandating clients to throttle requests.
  // Protected by |response_code_override_lock_|.
  bool throttling_;

  // Whether we are failing all requests by returning
  // ClientToServerResponse::AUTH_INVALID.
  // Protected by |response_code_override_lock_|.
  bool fail_with_auth_invalid_;

  Lock response_code_override_lock_;

  // True if we are only accepting GetUpdatesCallerInfo::PERIODIC requests.
  bool fail_non_periodic_get_updates_;

  scoped_ptr<sync_pb::ClientCommand> client_command_;

  // The next value to use for the position_in_parent property.
  int64 next_position_in_parent_;

  // The default is to use the newer sync_pb::BookmarkSpecifics-style protocol.
  // If this option is set to true, then the MockConnectionManager will
  // use the older sync_pb::SyncEntity_BookmarkData-style protocol.
  bool use_legacy_bookmarks_protocol_;

  std::bitset<syncable::MODEL_TYPE_COUNT> expected_filter_;

  int num_get_updates_requests_;

  DISALLOW_COPY_AND_ASSIGN(MockConnectionManager);
};

#endif  // CHROME_TEST_SYNC_ENGINE_MOCK_SERVER_CONNECTION_H_
