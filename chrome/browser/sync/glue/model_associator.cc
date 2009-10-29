// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/model_associator.h"

#include <stack>

#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"

namespace browser_sync {

// The sync protocol identifies top-level entities by means of well-known tags,
// which should not be confused with titles.  Each tag corresponds to a
// singleton instance of a particular top-level node in a user's share; the
// tags are consistent across users. The tags allow us to locate the specific
// folders whose contents we care about synchronizing, without having to do a
// lookup by name or path.  The tags should not be made user-visible.
// For example, the tag "bookmark_bar" represents the permanent node for
// bookmarks bar in Chrome. The tag "other_bookmarks" represents the permanent
// folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing,
// the sync server) to create these tagged nodes when initializing sync
// for the first time for a user.  Thus, once the backend finishes
// initializing, the ProfileSyncService can rely on the presence of tagged
// nodes.
//
// TODO(ncarter): Pull these tags from an external protocol specification
// rather than hardcoding them here.
static const wchar_t* kOtherBookmarksTag = L"other_bookmarks";
static const wchar_t* kBookmarkBarTag = L"bookmark_bar";

// Bookmark comparer for map of bookmark nodes.
class BookmarkComparer {
 public:
  // Compares the two given nodes and returns whether node1 should appear
  // before node2 in strict weak ordering.
  bool operator()(const BookmarkNode* node1,
                  const BookmarkNode* node2) const {
    DCHECK(node1);
    DCHECK(node2);

    // Keep folder nodes before non-folder nodes.
    if (node1->is_folder() != node2->is_folder())
      return node1->is_folder();

    int result = node1->GetTitle().compare(node2->GetTitle());
    if (result != 0)
      return result < 0;

    result = node1->GetURL().spec().compare(node2->GetURL().spec());
    if (result != 0)
      return result < 0;

    return false;
  }
};

// Provides the following abstraction: given a parent bookmark node, find best
// matching child node for many sync nodes.
class BookmarkNodeFinder {
 public:
  // Creates an instance with the given parent bookmark node.
  explicit BookmarkNodeFinder(const BookmarkNode* parent_node);

  // Finds best matching node for the given sync node.
  // Returns the matching node if one exists; NULL otherwise. If a matching
  // node is found, it's removed for further matches.
  const BookmarkNode* FindBookmarkNode(const sync_api::BaseNode& sync_node);

 private:
  typedef std::multiset<const BookmarkNode*, BookmarkComparer> BookmarkNodesSet;

  const BookmarkNode* parent_node_;
  BookmarkNodesSet child_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeFinder);
};

BookmarkNodeFinder::BookmarkNodeFinder(const BookmarkNode* parent_node)
    : parent_node_(parent_node) {
  for (int i = 0; i < parent_node_->GetChildCount(); ++i) {
    child_nodes_.insert(parent_node_->GetChild(i));
  }
}

const BookmarkNode* BookmarkNodeFinder::FindBookmarkNode(
    const sync_api::BaseNode& sync_node) {
  // Create a bookmark node from the given sync node.
  BookmarkNode temp_node(GURL(sync_node.GetURL()));
  temp_node.SetTitle(UTF16ToWide(sync_node.GetTitle()));
  if (sync_node.GetIsFolder())
    temp_node.set_type(BookmarkNode::FOLDER);
  else
    temp_node.set_type(BookmarkNode::URL);

  const BookmarkNode* result = NULL;
  BookmarkNodesSet::iterator iter = child_nodes_.find(&temp_node);
  if (iter != child_nodes_.end()) {
    result = *iter;
    // Remove the matched node so we don't match with it again.
    child_nodes_.erase(iter);
  }

  return result;
}

// Helper class to build an index of bookmark nodes by their IDs.
class BookmarkNodeIdIndex {
 public:
  BookmarkNodeIdIndex() { }
  ~BookmarkNodeIdIndex() { }

  // Adds the given bookmark node and all its descendants to the ID index.
  // Does nothing if node is NULL.
  void AddAll(const BookmarkNode* node);

  // Finds the bookmark node with the given ID.
  // Returns NULL if none exists with the given id.
  const BookmarkNode* Find(int64 id) const;

  // Returns the count of nodes in the index.
  size_t count() const { return node_index_.size(); }

 private:
  typedef base::hash_map<int64, const BookmarkNode*> BookmarkIdMap;
  // Map that holds nodes indexed by their ids.
  BookmarkIdMap node_index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeIdIndex);
};

void BookmarkNodeIdIndex::AddAll(const BookmarkNode* node) {
  if (!node)
    return;

  node_index_[node->id()] = node;

  if (!node->is_folder())
    return;

  for (int i = 0; i < node->GetChildCount(); ++i)
    AddAll(node->GetChild(i));
}

const BookmarkNode* BookmarkNodeIdIndex::Find(int64 id) const {
  BookmarkIdMap::const_iterator iter = node_index_.find(id);
  return iter == node_index_.end() ? NULL : iter->second;
}

ModelAssociator::ModelAssociator(ProfileSyncService* sync_service)
    : sync_service_(sync_service),
      task_pending_(false) {
  DCHECK(sync_service_);
}

void ModelAssociator::ClearAll() {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
}

int64 ModelAssociator::GetSyncIdFromBookmarkId(int64 node_id) const {
  BookmarkIdToSyncIdMap::const_iterator iter = id_map_.find(node_id);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

bool ModelAssociator::GetBookmarkIdFromSyncId(int64 sync_id,
                                              int64* node_id) const {
  SyncIdToBookmarkIdMap::const_iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return false;
  *node_id = iter->second;
  return true;
}

bool ModelAssociator::InitSyncNodeFromBookmarkId(
    int64 node_id,
    sync_api::BaseNode* sync_node) {
  DCHECK(sync_node);
  int64 sync_id = GetSyncIdFromBookmarkId(node_id);
  if (sync_id == sync_api::kInvalidId)
    return false;
  if (!sync_node->InitByIdLookup(sync_id))
    return false;
  DCHECK(sync_node->GetId() == sync_id);
  return true;
}

const BookmarkNode* ModelAssociator::GetBookmarkNodeFromSyncId(int64 sync_id) {
  int64 node_id;
  if (!GetBookmarkIdFromSyncId(sync_id, &node_id))
    return false;
  BookmarkModel* model = sync_service_->profile()->GetBookmarkModel();
  // TODO(munjal): Fix issue 26141.
  return model->GetNodeByID(node_id);
}

void ModelAssociator::AssociateIds(int64 node_id, int64 sync_id) {
  DCHECK_NE(sync_id, sync_api::kInvalidId);
  DCHECK(id_map_.find(node_id) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[node_id] = sync_id;
  id_map_inverse_[sync_id] = node_id;
  dirty_associations_sync_ids_.insert(sync_id);
  PostPersistAssociationsTask();
}

void ModelAssociator::DisassociateIds(int64 sync_id) {
  SyncIdToBookmarkIdMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second);
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool ModelAssociator::BookmarkModelHasUserCreatedNodes() const {
  BookmarkModel* model = sync_service_->profile()->GetBookmarkModel();
  DCHECK(model->IsLoaded());
  return model->GetBookmarkBarNode()->GetChildCount() > 0 ||
         model->other_node()->GetChildCount() > 0;
}

bool ModelAssociator::SyncModelHasUserCreatedNodes() {
  int64 bookmark_bar_sync_id;
  if (!GetSyncIdForTaggedNode(WideToUTF16(kBookmarkBarTag),
                              &bookmark_bar_sync_id)) {
    sync_service_->OnUnrecoverableError();
    return false;
  }
  int64 other_bookmarks_sync_id;
  if (!GetSyncIdForTaggedNode(WideToUTF16(kOtherBookmarksTag),
                              &other_bookmarks_sync_id)) {
    sync_service_->OnUnrecoverableError();
    return false;
  }

  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());

  sync_api::ReadNode bookmark_bar_node(&trans);
  if (!bookmark_bar_node.InitByIdLookup(bookmark_bar_sync_id)) {
    sync_service_->OnUnrecoverableError();
    return false;
  }

  sync_api::ReadNode other_bookmarks_node(&trans);
  if (!other_bookmarks_node.InitByIdLookup(other_bookmarks_sync_id)) {
    sync_service_->OnUnrecoverableError();
    return false;
  }

  // Sync model has user created nodes if either one of the permanent nodes
  // has children.
  return bookmark_bar_node.GetFirstChildId() != sync_api::kInvalidId ||
         other_bookmarks_node.GetFirstChildId() != sync_api::kInvalidId;
}

bool ModelAssociator::NodesMatch(const BookmarkNode* bookmark,
                                 const sync_api::BaseNode* sync_node) const {
  if (bookmark->GetTitle() != UTF16ToWide(sync_node->GetTitle()))
    return false;
  if (bookmark->is_folder() != sync_node->GetIsFolder())
    return false;
  if (bookmark->is_url()) {
    if (bookmark->GetURL() != GURL(sync_node->GetURL()))
      return false;
  }
  // Don't compare favicons here, because they are not really
  // user-updated and we don't have versioning information -- a site changing
  // its favicon shouldn't result in a bookmark mismatch.
  return true;
}

bool ModelAssociator::AssociateTaggedPermanentNode(
    const BookmarkNode* permanent_node,
    const string16 &tag) {
  // Do nothing if |permanent_node| is already initialized and associated.
  int64 sync_id = GetSyncIdFromBookmarkId(permanent_node->id());
  if (sync_id != sync_api::kInvalidId)
    return true;
  if (!GetSyncIdForTaggedNode(tag, &sync_id))
    return false;

  AssociateIds(permanent_node->id(), sync_id);
  return true;
}

bool ModelAssociator::GetSyncIdForTaggedNode(const string16& tag,
                                             int64* sync_id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool ModelAssociator::AssociateModels() {
  // Try to load model associations from persisted associations first. If that
  // succeeds, we don't need to run the complex model matching algorithm.
  if (LoadAssociations())
    return true;

  ClearAll();

  // We couldn't load model associations from persisted associations. So build
  // them.
  return BuildAssociations();
}

bool ModelAssociator::BuildAssociations() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each sync node for the current sync parent node, find the best
  //   matching bookmark node under the corresponding bookmark parent node.
  //   If no matching node is found, create a new bookmark node in the same
  //   position as the corresponding sync node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding sync node.
  // * When all children sync nodes are done, add the extra children bookmark
  //   nodes to the sync parent node.
  //
  // This algorithm will do a good job of merging when folder names are a good
  // indicator of the two folders being the same. It will handle reordering and
  // new node addition very well (without creating duplicates).
  // This algorithm will not do well if the folder name has changes but the
  // children under them are all the same.

  BookmarkModel* model = sync_service_->profile()->GetBookmarkModel();
  DCHECK(model->IsLoaded());

  // To prime our association, we associate the top-level nodes, Bookmark Bar
  // and Other Bookmarks.
  if (!AssociateTaggedPermanentNode(model->other_node(),
                                    WideToUTF16(kOtherBookmarksTag))) {
    sync_service_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create top-level nodes.  Possibly we "
               << "are running against an out-of-date server?";
    return false;
  }
  if (!AssociateTaggedPermanentNode(model->GetBookmarkBarNode(),
                                    WideToUTF16(kBookmarkBarTag))) {
    sync_service_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create top-level nodes.  Possibly we "
               << "are running against an out-of-date server?";
    return false;
  }
  int64 bookmark_bar_sync_id = GetSyncIdFromBookmarkId(
      model->GetBookmarkBarNode()->id());
  DCHECK(bookmark_bar_sync_id != sync_api::kInvalidId);
  int64 other_bookmarks_sync_id = GetSyncIdFromBookmarkId(
      model->other_node()->id());
  DCHECK(other_bookmarks_sync_id != sync_api::kInvalidId);

  std::stack<int64> dfs_stack;
  dfs_stack.push(other_bookmarks_sync_id);
  dfs_stack.push(bookmark_bar_sync_id);

  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());

  while (!dfs_stack.empty()) {
    int64 sync_parent_id = dfs_stack.top();
    dfs_stack.pop();

    sync_api::ReadNode sync_parent(&trans);
    if (!sync_parent.InitByIdLookup(sync_parent_id)) {
      sync_service_->OnUnrecoverableError();
      return false;
    }
    // Only folder nodes are pushed on to the stack.
    DCHECK(sync_parent.GetIsFolder());

    const BookmarkNode* parent_node = GetBookmarkNodeFromSyncId(sync_parent_id);
    DCHECK(parent_node->is_folder());

    BookmarkNodeFinder node_finder(parent_node);

    int index = 0;
    int64 sync_child_id = sync_parent.GetFirstChildId();
    while (sync_child_id != sync_api::kInvalidId) {
      sync_api::WriteNode sync_child_node(&trans);
      if (!sync_child_node.InitByIdLookup(sync_child_id)) {
        sync_service_->OnUnrecoverableError();
        return false;
      }

      const BookmarkNode* child_node = NULL;
      child_node = node_finder.FindBookmarkNode(sync_child_node);
      if (child_node) {
        model->Move(child_node, parent_node, index);
        // Set the favicon for bookmark node from sync node or vice versa.
        if (ChangeProcessor::SetBookmarkFavicon(&sync_child_node,
            child_node, sync_service_->profile())) {
          ChangeProcessor::SetSyncNodeFavicon(child_node, model,
                                              &sync_child_node);
        }
      } else {
        // Create a new bookmark node for the sync node.
        child_node = ChangeProcessor::CreateBookmarkNode(&sync_child_node,
            parent_node, model, index);
      }
      AssociateIds(child_node->id(), sync_child_id);
      if (sync_child_node.GetIsFolder())
        dfs_stack.push(sync_child_id);

      sync_child_id = sync_child_node.GetSuccessorId();
      ++index;
    }

    // At this point all the children nodes of the parent sync node have
    // corresponding children in the parent bookmark node and they are all in
    // the right positions: from 0 to index - 1.
    // So the children starting from index in the parent bookmark node are the
    // ones that are not present in the parent sync node. So create them.
    for (int i = index; i < parent_node->GetChildCount(); ++i) {
      sync_child_id = ChangeProcessor::CreateSyncNode(parent_node, model, i,
          &trans, this, sync_service_);
      if (parent_node->GetChild(i)->is_folder())
        dfs_stack.push(sync_child_id);
    }
  }
  return true;
}

void ModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (task_pending_)
    return;
  task_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ModelAssociator::PersistAssociations));
}

void ModelAssociator::PersistAssociations() {
  DCHECK(task_pending_);
  task_pending_ = false;

  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  DirtyAssociationsSyncIds::iterator iter;
  for (iter = dirty_associations_sync_ids_.begin();
       iter != dirty_associations_sync_ids_.end();
       ++iter) {
    int64 sync_id = *iter;
    sync_api::WriteNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(sync_id)) {
      sync_service_->OnUnrecoverableError();
      return;
    }
    int64 node_id;
    if (GetBookmarkIdFromSyncId(sync_id, &node_id))
      sync_node.SetExternalId(node_id);
    else
      NOTREACHED();
  }
  dirty_associations_sync_ids_.clear();
}

bool ModelAssociator::LoadAssociations() {
  BookmarkModel* model = sync_service_->profile()->GetBookmarkModel();
  DCHECK(model->IsLoaded());
  // If the bookmarks changed externally, our previous associations may not be
  // valid; so return false.
  if (model->file_changed())
    return false;

  // Our persisted associations should be valid. Try to populate id association
  // maps using persisted associations.  Note that the unit tests will
  // create the tagged nodes on demand, and the order in which we probe for
  // them here will impact their positional ordering in that case.
  int64 bookmark_bar_id;
  if (!GetSyncIdForTaggedNode(WideToUTF16(kBookmarkBarTag), &bookmark_bar_id)) {
    // We should always be able to find the permanent nodes.
    sync_service_->OnUnrecoverableError();
    return false;
  }
  int64 other_bookmarks_id;
  if (!GetSyncIdForTaggedNode(WideToUTF16(kOtherBookmarksTag),
      &other_bookmarks_id)) {
    // We should always be able to find the permanent nodes.
    sync_service_->OnUnrecoverableError();
    return false;
  }

  // Build a bookmark node ID index since we are going to repeatedly search for
  // bookmark nodes by their IDs.
  BookmarkNodeIdIndex id_index;
  id_index.AddAll(model->GetBookmarkBarNode());
  id_index.AddAll(model->other_node());

  std::stack<int64> dfs_stack;
  dfs_stack.push(other_bookmarks_id);
  dfs_stack.push(bookmark_bar_id);

  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());

  // Count total number of nodes in sync model so that we can compare that
  // with the total number of nodes in the bookmark model.
  size_t sync_node_count = 0;
  while (!dfs_stack.empty()) {
    int64 parent_id = dfs_stack.top();
    dfs_stack.pop();
    ++sync_node_count;
    sync_api::ReadNode sync_parent(&trans);
    if (!sync_parent.InitByIdLookup(parent_id)) {
      sync_service_->OnUnrecoverableError();
      return false;
    }

    int64 external_id = sync_parent.GetExternalId();
    if (external_id == 0)
      return false;

    const BookmarkNode* node = id_index.Find(external_id);
    if (!node)
      return false;

    // Don't try to call NodesMatch on permanent nodes like bookmark bar and
    // other bookmarks. They are not expected to match.
    if (node != model->GetBookmarkBarNode() &&
        node != model->other_node() &&
        !NodesMatch(node, &sync_parent))
      return false;

    AssociateIds(external_id, sync_parent.GetId());

    // Add all children of the current node to the stack.
    int64 child_id = sync_parent.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      dfs_stack.push(child_id);
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id)) {
        sync_service_->OnUnrecoverableError();
        return false;
      }
      child_id = child_node.GetSuccessorId();
    }
  }
  DCHECK(dfs_stack.empty());

  // It's possible that the number of nodes in the bookmark model is not the
  // same as number of nodes in the sync model. This can happen when the sync
  // model doesn't get a chance to persist its changes, for example when
  // Chrome does not shut down gracefully. In such cases we can't trust the
  // loaded associations.
  return sync_node_count == id_index.count();
}

}  // namespace browser_sync
