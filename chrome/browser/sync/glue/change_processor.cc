// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/change_processor.h"

#include "app/gfx/codec/png_codec.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace browser_sync {

void ChangeProcessor::Start(BookmarkModel* model, sync_api::UserShare* handle) {
  DCHECK(error_handler_ && model_associator_);
  DCHECK(!share_handle_ && !bookmark_model_);
  share_handle_ = handle;
  bookmark_model_ = model;
  DCHECK(model->IsLoaded());
  bookmark_model_->AddObserver(this);
  running_ = true;
}

void ChangeProcessor::Stop() {
  if (!running_)
    return;
  DCHECK(bookmark_model_);
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = NULL;
  share_handle_ = NULL;
  model_associator_ = NULL;
  running_ = false;
}

void ChangeProcessor::UpdateSyncNodeProperties(const BookmarkNode* src,
                                               BookmarkModel* model,
                                               sync_api::WriteNode* dst) {
  // Set the properties of the item.
  dst->SetIsFolder(src->is_folder());
  dst->SetTitle(src->GetTitle());
  dst->SetURL(src->GetURL());
  SetSyncNodeFavicon(src, model, dst);
}

// static
void ChangeProcessor::EncodeFavicon(const BookmarkNode* src,
                                    BookmarkModel* model,
                                    std::vector<unsigned char>* dst) {
  const SkBitmap& favicon = model->GetFavIcon(src);

  dst->clear();

  // Check for zero-dimension images.  This can happen if the favicon is
  // still being loaded.
  if (favicon.empty())
    return;

  // Re-encode the BookmarkNode's favicon as a PNG, and pass the data to the
  // sync subsystem.
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(favicon, false, dst))
    return;
}

void ChangeProcessor::RemoveOneSyncNode(sync_api::WriteTransaction* trans,
                                        const BookmarkNode* node) {
  sync_api::WriteNode sync_node(trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(node->id(), &sync_node)) {
    error_handler_->OnUnrecoverableError();
    return;
  }
  // This node should have no children.
  DCHECK(sync_node.GetFirstChildId() == sync_api::kInvalidId);
  // Remove association and delete the sync node.
  model_associator_->DisassociateIds(sync_node.GetId());
  sync_node.Remove();
}


void ChangeProcessor::RemoveSyncNodeHierarchy(const BookmarkNode* topmost) {
  sync_api::WriteTransaction trans(share_handle_);

  // Later logic assumes that |topmost| has been unlinked.
  DCHECK(!topmost->GetParent());

  // A BookmarkModel deletion event means that |node| and all its children were
  // deleted. Sync backend expects children to be deleted individually, so we do
  // a depth-first-search here.  At each step, we consider the |index|-th child
  // of |node|.  |index_stack| stores index values for the parent levels.
  std::stack<int> index_stack;
  index_stack.push(0);  // For the final pop.  It's never used.
  const BookmarkNode* node = topmost;
  int index = 0;
  while (node) {
    // The top of |index_stack| should always be |node|'s index.
    DCHECK(!node->GetParent() || (node->GetParent()->IndexOfChild(node) ==
      index_stack.top()));
    if (index == node->GetChildCount()) {
      // If we've processed all of |node|'s children, delete |node| and move
      // on to its successor.
      RemoveOneSyncNode(&trans, node);
      node = node->GetParent();
      index = index_stack.top() + 1;      // (top() + 0) was what we removed.
      index_stack.pop();
    } else {
      // If |node| has an unprocessed child, process it next after pushing the
      // current state onto the stack.
      DCHECK_LT(index, node->GetChildCount());
      index_stack.push(index);
      node = node->GetChild(index);
      index = 0;
    }
  }
  DCHECK(index_stack.empty());  // Nothing should be left on the stack.
}

void ChangeProcessor::BookmarkModelBeingDeleted(BookmarkModel* model) {
  DCHECK(!running_) << "BookmarkModel deleted while ChangeProcessor running.";
  bookmark_model_ = NULL;
}

void ChangeProcessor::BookmarkNodeAdded(BookmarkModel* model,
                                        const BookmarkNode* parent,
                                        int index) {
  DCHECK(running_);
  DCHECK(share_handle_);

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(share_handle_);

  CreateSyncNode(parent, model, index, &trans, model_associator_,
                 error_handler_);
}

// static
int64 ChangeProcessor::CreateSyncNode(const BookmarkNode* parent,
    BookmarkModel* model, int index, sync_api::WriteTransaction* trans,
    ModelAssociator* associator, UnrecoverableErrorHandler* error_handler) {
  const BookmarkNode* child = parent->GetChild(index);
  DCHECK(child);

  // Create a WriteNode container to hold the new node.
  sync_api::WriteNode sync_child(trans);

  // Actually create the node with the appropriate initial position.
  if (!PlaceSyncNode(CREATE, parent, index, trans, &sync_child, associator,
                     error_handler)) {
    LOG(WARNING) << "Sync node creation failed; recovery unlikely";
    error_handler->OnUnrecoverableError();
    return sync_api::kInvalidId;
  }

  UpdateSyncNodeProperties(child, model, &sync_child);

  // Associate the ID from the sync domain with the bookmark node, so that we
  // can refer back to this item later.
  associator->AssociateIds(child->id(), sync_child.GetId());

  return sync_child.GetId();
}


void ChangeProcessor::BookmarkNodeRemoved(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index,
                                          const BookmarkNode* node) {
  DCHECK(running_);
  RemoveSyncNodeHierarchy(node);
}

void ChangeProcessor::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  DCHECK(running_);
  // We shouldn't see changes to the top-level nodes.
  if (node == model->GetBookmarkBarNode() || node == model->other_node()) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(share_handle_);

  // Lookup the sync node that's associated with |node|.
  sync_api::WriteNode sync_node(&trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(node->id(), &sync_node)) {
    error_handler_->OnUnrecoverableError();
    return;
  }

  UpdateSyncNodeProperties(node, model, &sync_node);

  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  DCHECK_EQ(model_associator_->GetBookmarkNodeFromSyncId(
            sync_node.GetParentId()),
            node->GetParent());
  // This node's index should be one more than the predecessor's index.
  DCHECK_EQ(node->GetParent()->IndexOfChild(node),
            CalculateBookmarkModelInsertionIndex(node->GetParent(),
                                                 &sync_node));
}


void ChangeProcessor::BookmarkNodeMoved(BookmarkModel* model,
                                        const BookmarkNode* old_parent,
                                        int old_index,
                                        const BookmarkNode* new_parent,
                                        int new_index) {
  DCHECK(running_);
  const BookmarkNode* child = new_parent->GetChild(new_index);
  // We shouldn't see changes to the top-level nodes.
  if (child == model->GetBookmarkBarNode() || child == model->other_node()) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(share_handle_);

  // Lookup the sync node that's associated with |child|.
  sync_api::WriteNode sync_node(&trans);
  if (!model_associator_->InitSyncNodeFromBookmarkId(child->id(), &sync_node)) {
    error_handler_->OnUnrecoverableError();
    return;
  }

  if (!PlaceSyncNode(MOVE, new_parent, new_index, &trans, &sync_node,
                     model_associator_, error_handler_)) {
    error_handler_->OnUnrecoverableError();
    return;
  }
}

void ChangeProcessor::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                const BookmarkNode* node) {
  DCHECK(running_);
  BookmarkNodeChanged(model, node);
}

void ChangeProcessor::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {

  // Acquire a scoped write lock via a transaction.
  sync_api::WriteTransaction trans(share_handle_);

  // The given node's children got reordered. We need to reorder all the
  // children of the corresponding sync node.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    sync_api::WriteNode sync_child(&trans);
    if (!model_associator_->InitSyncNodeFromBookmarkId(node->GetChild(i)->id(),
                                                       &sync_child)) {
      error_handler_->OnUnrecoverableError();
      return;
    }
    DCHECK_EQ(sync_child.GetParentId(),
              model_associator_->GetSyncIdFromBookmarkId(node->id()));

    if (!PlaceSyncNode(MOVE, node, i, &trans, &sync_child, model_associator_,
                       error_handler_)) {
      error_handler_->OnUnrecoverableError();
      return;
    }
  }
}

// static
bool ChangeProcessor::PlaceSyncNode(MoveOrCreate operation,
                                    const BookmarkNode* parent,
                                    int index,
                                    sync_api::WriteTransaction* trans,
                                    sync_api::WriteNode* dst,
                                    ModelAssociator* associator,
                                    UnrecoverableErrorHandler* error_handler) {
  sync_api::ReadNode sync_parent(trans);
  if (!associator->InitSyncNodeFromBookmarkId(parent->id(), &sync_parent)) {
    LOG(WARNING) << "Parent lookup failed";
    error_handler->OnUnrecoverableError();
    return false;
  }

  bool success = false;
  if (index == 0) {
    // Insert into first position.
    success = (operation == CREATE) ? dst->InitByCreation(sync_parent, NULL) :
                                      dst->SetPosition(sync_parent, NULL);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetId(), sync_parent.GetFirstChildId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_api::kInvalidId);
    }
  } else {
    // Find the bookmark model predecessor, and insert after it.
    const BookmarkNode* prev = parent->GetChild(index - 1);
    sync_api::ReadNode sync_prev(trans);
    if (!associator->InitSyncNodeFromBookmarkId(prev->id(), &sync_prev)) {
      LOG(WARNING) << "Predecessor lookup failed";
      return false;
    }
    success = (operation == CREATE) ?
        dst->InitByCreation(sync_parent, &sync_prev) :
        dst->SetPosition(sync_parent, &sync_prev);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_prev.GetId());
      DCHECK_EQ(dst->GetId(), sync_prev.GetSuccessorId());
    }
  }
  return success;
}

// Determine the bookmark model index to which a node must be moved so that
// predecessor of the node (in the bookmark model) matches the predecessor of
// |source| (in the sync model).
// As a precondition, this assumes that the predecessor of |source| has been
// updated and is already in the correct position in the bookmark model.
int ChangeProcessor::CalculateBookmarkModelInsertionIndex(
    const BookmarkNode* parent,
    const sync_api::BaseNode* child_info) const {
  DCHECK(parent);
  DCHECK(child_info);
  int64 predecessor_id = child_info->GetPredecessorId();
  // A return ID of kInvalidId indicates no predecessor.
  if (predecessor_id == sync_api::kInvalidId)
    return 0;

  // Otherwise, insert after the predecessor bookmark node.
  const BookmarkNode* predecessor =
      model_associator_->GetBookmarkNodeFromSyncId(predecessor_id);
  DCHECK(predecessor);
  DCHECK_EQ(predecessor->GetParent(), parent);
  return parent->IndexOfChild(predecessor) + 1;
}

// ApplyModelChanges is called by the sync backend after changes have been made
// to the sync engine's model.  Apply these changes to the browser bookmark
// model.
void ChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!running_)
    return;
  // A note about ordering.  Sync backend is responsible for ordering the change
  // records in the following order:
  //
  // 1. Deletions, from leaves up to parents.
  // 2. Existing items with synced parents & predecessors.
  // 3. New items with synced parents & predecessors.
  // 4. Items with parents & predecessors in the list.
  // 5. Repeat #4 until all items are in the list.
  //
  // "Predecessor" here means the previous item within a given folder; an item
  // in the first position is always said to have a synced predecessor.
  // For the most part, applying these changes in the order given will yield
  // the correct result.  There is one exception, however: for items that are
  // moved away from a folder that is being deleted, we will process the delete
  // before the move.  Since deletions in the bookmark model propagate from
  // parent to child, we must move them to a temporary location.
  BookmarkModel* model = bookmark_model_;

  // We are going to make changes to the bookmarks model, but don't want to end
  // up in a feedback loop, so remove ourselves as an observer while applying
  // changes.
  model->RemoveObserver(this);

  // A parent to hold nodes temporarily orphaned by parent deletion.  It is
  // lazily created inside the loop.
  const BookmarkNode* foster_parent = NULL;
  for (int i = 0; i < change_count; ++i) {
    const BookmarkNode* dst =
        model_associator_->GetBookmarkNodeFromSyncId(changes[i].id);
    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if ((dst == model->GetBookmarkBarNode()) || (dst == model->other_node()))
      continue;
    if (changes[i].action ==
        sync_api::SyncManager::ChangeRecord::ACTION_DELETE) {
      // Deletions should always be at the front of the list.
      DCHECK(i == 0 || changes[i-1].action == changes[i].action);
      // Children of a deleted node should not be deleted; they may be
      // reparented by a later change record.  Move them to a temporary place.
      DCHECK(dst) << "Could not find node to be deleted";
      const BookmarkNode* parent = dst->GetParent();
      if (dst->GetChildCount()) {
        if (!foster_parent) {
          foster_parent = model->AddGroup(model->other_node(),
                                          model->other_node()->GetChildCount(),
                                          std::wstring());
        }
        for (int i = dst->GetChildCount() - 1; i >= 0; --i) {
          model->Move(dst->GetChild(i), foster_parent,
                      foster_parent->GetChildCount());
        }
      }
      DCHECK_EQ(dst->GetChildCount(), 0) << "Node being deleted has children";
      model->Remove(parent, parent->IndexOfChild(dst));
      dst = NULL;
      model_associator_->DisassociateIds(changes[i].id);
    } else {
      DCHECK_EQ((changes[i].action ==
          sync_api::SyncManager::ChangeRecord::ACTION_ADD), (dst == NULL))
          << "ACTION_ADD should be seen if and only if the node is unknown.";

      sync_api::ReadNode src(trans);
      if (!src.InitByIdLookup(changes[i].id)) {
        LOG(ERROR) << "ApplyModelChanges was passed a bad ID";
        error_handler_->OnUnrecoverableError();
        return;
      }

      CreateOrUpdateBookmarkNode(&src, model);
    }
  }
  // Clean up the temporary node.
  if (foster_parent) {
    // There should be no nodes left under the foster parent.
    DCHECK_EQ(foster_parent->GetChildCount(), 0);
    model->Remove(foster_parent->GetParent(),
                  foster_parent->GetParent()->IndexOfChild(foster_parent));
    foster_parent = NULL;
  }

  // We are now ready to hear about bookmarks changes again.
  model->AddObserver(this);
}

// Create a bookmark node corresponding to |src| if one is not already
// associated with |src|.
const BookmarkNode* ChangeProcessor::CreateOrUpdateBookmarkNode(
    sync_api::BaseNode* src,
    BookmarkModel* model) {
  const BookmarkNode* parent =
      model_associator_->GetBookmarkNodeFromSyncId(src->GetParentId());
  if (!parent) {
    DLOG(WARNING) << "Could not find parent of node being added/updated."
      << " Node title: " << src->GetTitle()
      << ", parent id = " << src->GetParentId();
    return NULL;
  }
  int index = CalculateBookmarkModelInsertionIndex(parent, src);
  const BookmarkNode* dst = model_associator_->GetBookmarkNodeFromSyncId(
      src->GetId());
  if (!dst) {
    dst = CreateBookmarkNode(src, parent, model, index);
    model_associator_->AssociateIds(dst->id(), src->GetId());
  } else {
    // URL and is_folder are not expected to change.
    // TODO(ncarter): Determine if such changes should be legal or not.
    DCHECK_EQ(src->GetIsFolder(), dst->is_folder());

    // Handle reparenting and/or repositioning.
    model->Move(dst, parent, index);

    // Handle title update and URL changes due to possible conflict resolution
    // that can happen if both a local user change and server change occur
    // within a sufficiently small time interval.
    const BookmarkNode* old_dst = dst;
    dst = bookmark_utils::ApplyEditsWithNoGroupChange(model, parent,
        BookmarkEditor::EditDetails(dst),
        src->GetTitle(),
        src->GetIsFolder() ? GURL() : src->GetURL(),
        NULL);  // NULL because we don't need a BookmarkEditor::Handler.
    if (dst != old_dst) {  // dst was replaced with a new node with new URL.
      model_associator_->DisassociateIds(src->GetId());
      model_associator_->AssociateIds(dst->id(), src->GetId());
    }
    SetBookmarkFavicon(src, dst, model->profile());
  }

  return dst;
}

// static
// Creates a bookmark node under the given parent node from the given sync
// node. Returns the newly created node.
const BookmarkNode* ChangeProcessor::CreateBookmarkNode(
    sync_api::BaseNode* sync_node,
    const BookmarkNode* parent,
    BookmarkModel* model,
    int index) {
  DCHECK(parent);
  DCHECK(index >= 0 && index <= parent->GetChildCount());

  const BookmarkNode* node;
  if (sync_node->GetIsFolder()) {
    node = model->AddGroup(parent, index, sync_node->GetTitle());
  } else {
    node = model->AddURL(parent, index,
                         sync_node->GetTitle(), sync_node->GetURL());
    SetBookmarkFavicon(sync_node, node, model->profile());
  }
  return node;
}

// static
// Sets the favicon of the given bookmark node from the given sync node.
bool ChangeProcessor::SetBookmarkFavicon(
    sync_api::BaseNode* sync_node,
    const BookmarkNode* bookmark_node,
    Profile* profile) {
  size_t icon_size = 0;
  const unsigned char* icon_bytes = sync_node->GetFaviconBytes(&icon_size);
  if (!icon_size || !icon_bytes)
    return false;

  // Registering a favicon requires that we provide a source URL, but we
  // don't know where these came from.  Currently we just use the
  // destination URL, which is not correct, but since the favicon URL
  // is used as a key in the history's thumbnail DB, this gives us a value
  // which does not collide with others.
  GURL fake_icon_url = bookmark_node->GetURL();

  std::vector<unsigned char> icon_bytes_vector(icon_bytes,
                                               icon_bytes + icon_size);

  HistoryService* history =
      profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  FaviconService* favicon_service =
      profile->GetFaviconService(Profile::EXPLICIT_ACCESS);

  history->AddPage(bookmark_node->GetURL());
  favicon_service->SetFavicon(bookmark_node->GetURL(),
                              fake_icon_url,
                              icon_bytes_vector);

  return true;
}

// static
void ChangeProcessor::SetSyncNodeFavicon(
    const BookmarkNode* bookmark_node,
    BookmarkModel* model,
    sync_api::WriteNode* sync_node) {
  std::vector<unsigned char> favicon_bytes;
  EncodeFavicon(bookmark_node, model, &favicon_bytes);
  if (!favicon_bytes.empty())
    sync_node->SetFaviconBytes(&favicon_bytes[0], favicon_bytes.size());
}

}  // namespace browser_sync
