// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_entry.h"

class ProfileSyncService;

namespace browser_sync {

class AutofillChangeProcessor;
class UnrecoverableErrorHandler;

extern const char kAutofillTag[];

// Contains all model association related logic:
// * Algorithm to associate autofill model and sync model.
// * Persisting model associations and loading them back.
// We do not check if we have local data before this run; we always
// merge and sync.
class AutofillModelAssociator
    : public PerDataTypeAssociatorInterface<AutofillKey, AutofillKey> {
 public:
  static syncable::ModelType model_type() { return syncable::AUTOFILL; }
  AutofillModelAssociator(ProfileSyncService* sync_service,
                          UnrecoverableErrorHandler* error_handler);
  virtual ~AutofillModelAssociator() { }

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Clears all associations.
  virtual bool DisassociateModels();

  // Returns whether the sync model has nodes other than the permanent tagged
  // nodes.
  virtual bool SyncModelHasUserCreatedNodes();

  // Returns whether the autofill model has any user-defined autofills.
  virtual bool ChromeModelHasUserCreatedNodes();

  // Not implemented.
  virtual const AutofillKey* GetChromeNodeFromSyncId(int64 sync_id) {
    return NULL;
  }

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(AutofillKey node_id,
                                        sync_api::BaseNode* sync_node) {
    return false;
  }

  // Returns the sync id for the given autofill name, or sync_api::kInvalidId
  // if the autofill name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(AutofillKey node_id);

  // Associates the given autofill name with the given sync id.
  virtual void Associate(const AutofillKey* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  static std::string KeyToTag(const string16& name, const string16& value);
  static bool MergeTimestamps(const sync_pb::AutofillSpecifics& autofill,
                              const std::vector<base::Time>& timestamps,
                              std::vector<base::Time>* new_timestamps);

 private:
  typedef std::map<AutofillKey, int64> AutofillToSyncIdMap;
  typedef std::map<int64, AutofillKey> SyncIdToAutofillMap;
  typedef std::set<int64> DirtyAssociationsSyncIds;

  // Posts a task to persist dirty associations.
  void PostPersistAssociationsTask();

  // Persists all dirty associations.
  void PersistAssociations();

  ProfileSyncService* sync_service_;
  UnrecoverableErrorHandler* error_handler_;
  int64 autofill_node_id_;

  AutofillToSyncIdMap id_map_;
  SyncIdToAutofillMap id_map_inverse_;
  // Stores the IDs of dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Used to post PersistAssociation tasks to the current message loop and
  // guarantees no invocations can occur if |this| has been deleted. (This
  // allows this class to be non-refcounted).
  ScopedRunnableMethodFactory<AutofillModelAssociator> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
