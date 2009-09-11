// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/get_commit_ids_command.h"

#include <set>
#include <utility>
#include <vector>

#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::set;
using std::vector;

namespace browser_sync {

GetCommitIdsCommand::GetCommitIdsCommand(int commit_batch_size)
    : requested_commit_batch_size_(commit_batch_size) {}

GetCommitIdsCommand::~GetCommitIdsCommand() {}

void GetCommitIdsCommand::ExecuteImpl(SyncerSession* session) {
  // Gather the full set of unsynced items and store it in the session. They
  // are not in the correct order for commit.
  syncable::Directory::UnsyncedMetaHandles all_unsynced_handles;
  SyncerUtil::GetUnsyncedEntries(session->write_transaction(),
                                     &all_unsynced_handles);
  session->set_unsynced_handles(all_unsynced_handles);

  BuildCommitIds(session);

  const vector<syncable::Id>& verified_commit_ids =
      ordered_commit_set_.GetCommitIds();

  for (size_t i = 0; i < verified_commit_ids.size(); i++)
    LOG(INFO) << "Debug commit batch result:" << verified_commit_ids[i];

  session->set_commit_ids(verified_commit_ids);
}

void GetCommitIdsCommand::AddUncommittedParentsAndTheirPredecessors(
    syncable::BaseTransaction* trans,
    syncable::Id parent_id) {
  using namespace syncable;
  OrderedCommitSet item_dependencies;

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.ServerKnows()) {
    Entry parent(trans, GET_BY_ID, parent_id);
    CHECK(parent.good()) << "Bad user-only parent in item path.";
    int64 handle = parent.Get(META_HANDLE);
    if (ordered_commit_set_.HaveCommitItem(handle) ||
        item_dependencies.HaveCommitItem(handle)) {
      break;
    }
    if (!AddItemThenPredecessors(trans, &parent, IS_UNSYNCED,
                                 &item_dependencies)) {
      break;  // Parent was already present in the set.
    }
    parent_id = parent.Get(PARENT_ID);
  }

  // Reverse what we added to get the correct order.
  ordered_commit_set_.AppendReverse(item_dependencies);
}

bool GetCommitIdsCommand::AddItem(syncable::Entry* item,
                                  OrderedCommitSet* result) {
  int64 item_handle = item->Get(syncable::META_HANDLE);
  if (result->HaveCommitItem(item_handle) ||
      ordered_commit_set_.HaveCommitItem(item_handle)) {
    return false;
  }
  result->AddCommitItem(item_handle, item->Get(syncable::ID));
  return true;
}

bool GetCommitIdsCommand::AddItemThenPredecessors(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    OrderedCommitSet* result) {
  if (!AddItem(item, result))
    return false;
  if (item->Get(syncable::IS_DEL))
    return true;  // Deleted items have no predecessors.

  syncable::Id prev_id = item->Get(syncable::PREV_ID);
  while (!prev_id.IsRoot()) {
    syncable::Entry prev(trans, syncable::GET_BY_ID, prev_id);
    CHECK(prev.good()) << "Bad id when walking predecessors.";
    if (!prev.Get(inclusion_filter))
      break;
    if (!AddItem(&prev, result))
      break;
    prev_id = prev.Get(syncable::PREV_ID);
  }
  return true;
}

void GetCommitIdsCommand::AddPredecessorsThenItem(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter) {
  OrderedCommitSet item_dependencies;
  AddItemThenPredecessors(trans, item, inclusion_filter, &item_dependencies);

  // Reverse what we added to get the correct order.
  ordered_commit_set_.AppendReverse(item_dependencies);
}

bool GetCommitIdsCommand::IsCommitBatchFull() {
  return ordered_commit_set_.Size() >= requested_commit_batch_size_;
}

void GetCommitIdsCommand::AddCreatesAndMoves(SyncerSession* session) {
  // Add moves and creates, and prepend their uncommitted parents.
  for (CommitMetahandleIterator iterator(session, &ordered_commit_set_);
      !IsCommitBatchFull() && iterator.Valid();
      iterator.Increment()) {
    int64 metahandle = iterator.Current();

    syncable::Entry entry(session->write_transaction(),
                          syncable::GET_BY_HANDLE,
                          metahandle);
    if (!entry.Get(syncable::IS_DEL)) {
      AddUncommittedParentsAndTheirPredecessors(
          session->write_transaction(), entry.Get(syncable::PARENT_ID));
      AddPredecessorsThenItem(session->write_transaction(), &entry,
                              syncable::IS_UNSYNCED);
    }
  }

  // It's possible that we overcommitted while trying to expand dependent
  // items.  If so, truncate the set down to the allowed size.
  ordered_commit_set_.Truncate(requested_commit_batch_size_);
}

void GetCommitIdsCommand::AddDeletes(SyncerSession* session) {
  set<syncable::Id> legal_delete_parents;

  for (CommitMetahandleIterator iterator(session, &ordered_commit_set_);
      !IsCommitBatchFull() && iterator.Valid();
      iterator.Increment()) {
    int64 metahandle = iterator.Current();

    syncable::Entry entry(session->write_transaction(),
                          syncable::GET_BY_HANDLE,
                          metahandle);

    if (entry.Get(syncable::IS_DEL)) {
      syncable::Entry parent(session->write_transaction(),
                             syncable::GET_BY_ID,
                             entry.Get(syncable::PARENT_ID));
      // If the parent is deleted and unsynced, then any children of that
      // parent don't need to be added to the delete queue.
      //
      // Note: the parent could be synced if there was an update deleting a
      // folder when we had a deleted all items in it.
      // We may get more updates, or we may want to delete the entry.
      if (parent.good() &&
          parent.Get(syncable::IS_DEL) &&
          parent.Get(syncable::IS_UNSYNCED)) {
        // However, if an entry is moved, these rules can apply differently.
        //
        // If the entry was moved, then the destination parent was deleted,
        // then we'll miss it in the roll up. We have to add it in manually.
        // TODO(chron): Unit test for move / delete cases:
        // Case 1: Locally moved, then parent deleted
        // Case 2: Server moved, then locally issue recursive delete.
        if (entry.Get(syncable::ID).ServerKnows() &&
            entry.Get(syncable::PARENT_ID) !=
                entry.Get(syncable::SERVER_PARENT_ID)) {
          LOG(INFO) << "Inserting moved and deleted entry, will be missed by"
            " delete roll." << entry.Get(syncable::ID);

          ordered_commit_set_.AddCommitItem(metahandle,
                                            entry.Get(syncable::ID));
        }

        // Skip this entry since it's a child of a parent that will be
        // deleted. The server will unroll the delete and delete the
        // child as well.
        continue;
      }

      legal_delete_parents.insert(entry.Get(syncable::PARENT_ID));
    }
  }

  // We could store all the potential entries with a particular parent during
  // the above scan, but instead we rescan here. This is less efficient, but
  // we're dropping memory alloc/dealloc in favor of linear scans of recently
  // examined entries.
  //
  // Scan through the UnsyncedMetaHandles again. If we have a deleted
  // entry, then check if the parent is in legal_delete_parents.
  //
  // Parent being in legal_delete_parents means for the child:
  //   a recursive delete is not currently happening (no recent deletes in same
  //     folder)
  //   parent did expect at least one old deleted child
  //   parent was not deleted

  for (CommitMetahandleIterator iterator(session, &ordered_commit_set_);
      !IsCommitBatchFull() && iterator.Valid();
      iterator.Increment()) {
    int64 metahandle = iterator.Current();
    syncable::MutableEntry entry(session->write_transaction(),
                                 syncable::GET_BY_HANDLE,
                                 metahandle);
    if (entry.Get(syncable::IS_DEL)) {
      syncable::Id parent_id = entry.Get(syncable::PARENT_ID);
      if (legal_delete_parents.count(parent_id)) {
        ordered_commit_set_.AddCommitItem(metahandle, entry.Get(syncable::ID));
      }
    }
  }
}

void GetCommitIdsCommand::BuildCommitIds(SyncerSession* session) {
  // Commits follow these rules:
  // 1. Moves or creates are preceded by needed folder creates, from
  //    root to leaf.  For folders whose contents are ordered, moves
  //    and creates appear in order.
  // 2. Moves/Creates before deletes.
  // 3. Deletes, collapsed.
  // We commit deleted moves under deleted items as moves when collapsing
  // delete trees.

  // Add moves and creates, and prepend their uncommitted parents.
  AddCreatesAndMoves(session);

  // Add all deletes.
  AddDeletes(session);
}

}  // namespace browser_sync
