// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/model_changing_syncer_command.h"
#include "chrome/browser/sync/engine/syncproto.h"

namespace syncable {
class Id;
class WriteTransaction;
class MutableEntry;
}

namespace browser_sync {

class ProcessCommitResponseCommand : public ModelChangingSyncerCommand {
 public:

  ProcessCommitResponseCommand();
  virtual ~ProcessCommitResponseCommand();

  // ModelChangingSyncerCommand implementation.
  virtual void ModelChangingExecuteImpl(sessions::SyncSession* session);

 private:
  CommitResponse::RESPONSE_TYPE ProcessSingleCommitResponse(
      syncable::WriteTransaction* trans,
      const sync_pb::CommitResponse_EntryResponse& pb_server_entry,
      const syncable::Id& pre_commit_id, std::set<syncable::Id>*
      conflicting_new_directory_ids,
      std::set<syncable::Id>* deleted_folders);

  // Actually does the work of execute.
  void ProcessCommitResponse(sessions::SyncSession* session);

  void ProcessSuccessfulCommitResponse(syncable::WriteTransaction* trans,
      const CommitResponse_EntryResponse& server_entry,
      const syncable::Id& pre_commit_id, syncable::MutableEntry* local_entry,
      bool syncing_was_set, std::set<syncable::Id>* deleted_folders);

  void PerformCommitTimeNameAside(
      syncable::WriteTransaction* trans,
      const CommitResponse_EntryResponse& server_entry,
      syncable::MutableEntry* local_entry);

  DISALLOW_COPY_AND_ASSIGN(ProcessCommitResponseCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_
