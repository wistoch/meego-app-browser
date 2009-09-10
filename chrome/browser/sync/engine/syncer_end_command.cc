// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include "chrome/browser/sync/engine/syncer_end_command.h"

#include "chrome/browser/sync/engine/conflict_resolution_view.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_status.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/event_sys-inl.h"

namespace browser_sync {

SyncerEndCommand::SyncerEndCommand() {}
SyncerEndCommand::~SyncerEndCommand() {}

void SyncerEndCommand::ExecuteImpl(SyncerSession* session) {
  ConflictResolutionView conflict_view(session);
  conflict_view.increment_num_sync_cycles();
  SyncerStatus status(session);
  status.set_syncing(false);

  if (!session->ShouldSyncAgain()) {
    // This might be the first time we've fully completed a sync cycle.
    DCHECK(session->got_zero_updates());

    syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
    if (!dir.good()) {
      LOG(ERROR) << "Scoped dir lookup failed!";
      return;
    }

    // This gets persisted to the directory's backing store.
    dir->set_initial_sync_ended(true);
  }

  SyncerEvent event = { SyncerEvent::SYNC_CYCLE_ENDED };
  event.last_session = session;
  session->syncer_event_channel()->NotifyListeners(event);
}

}  // namespace browser_sync
