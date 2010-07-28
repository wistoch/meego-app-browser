// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
#pragma once

#include <vector>

#include "app/combobox_model.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// Model for the combobox showing the list of folders to choose from. The
// list always contains the bookmark bar, other node and parent. The list
// also contains an extra item that shows the text 'Choose another folder...'.
class RecentlyUsedFoldersComboModel : public ComboboxModel {
 public:
  RecentlyUsedFoldersComboModel(BookmarkModel* model, const BookmarkNode* node);

  // Overridden from ComboboxModel:
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // Returns the node at the specified |index|.
  const BookmarkNode* GetNodeAt(int index);

  // Returns the index of the original parent folder.
  int node_parent_index() const { return node_parent_index_; }

 private:
  // Removes |node| from |nodes_|. Does nothing if |node| is not in |nodes_|.
  void RemoveNode(const BookmarkNode* node);

  std::vector<const BookmarkNode*> nodes_;
  int node_parent_index_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyUsedFoldersComboModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
