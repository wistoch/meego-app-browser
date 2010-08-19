// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "model_test_utils.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "googleurl/src/gurl.h"

namespace model_test_utils {

std::wstring ModelStringFromNode(const BookmarkNode* node) {
  // Since the children of the node are not available as a vector,
  // we'll just have to do it the hard way.
  int child_count = node->GetChildCount();
  std::wstring child_string;
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_folder()) {
      child_string += child->GetTitle() + L":[ " + ModelStringFromNode(child)
      + L"] ";
    } else {
      child_string += child->GetTitle() + L" ";
    }
  }
  return child_string;
}

// Helper function which does the actual work of creating the nodes for
// a particular level in the hierarchy.
std::wstring::size_type AddNodesFromString(BookmarkModel& model,
                               const BookmarkNode* node,
                               const std::wstring& model_string,
                               std::wstring::size_type start_pos) {
  DCHECK(node);
  int index = node->GetChildCount();
  static const std::wstring folder_tell(L":[");
  std::wstring::size_type end_pos = model_string.find(' ', start_pos);
  while (end_pos != std::wstring::npos) {
    std::wstring::size_type part_length = end_pos - start_pos;
    std::wstring node_name = model_string.substr(start_pos, part_length);
    // Are we at the end of a folder group?
    if (node_name != L"]") {
      // No, is it a folder?
      std::wstring tell;
      if (part_length > 2)
        tell = node_name.substr(part_length - 2, 2);
      if (tell == folder_tell) {
        node_name = node_name.substr(0, part_length - 2);
        const BookmarkNode* new_node =
            model.AddGroup(node, index, WideToUTF16Hack(node_name));
        end_pos = AddNodesFromString(model, new_node, model_string,
                                     end_pos + 1);
      } else {
        std::string url_string("http://");
        url_string += std::string(node_name.begin(), node_name.end());
        url_string += ".com";
        model.AddURL(node, index, node_name, GURL(url_string));
        ++end_pos;
      }
      ++index;
      start_pos = end_pos;
      end_pos = model_string.find(' ', start_pos);
    } else {
      ++end_pos;
      break;
    }
  }
  return end_pos;
}

void AddNodesFromModelString(BookmarkModel& model,
                             const BookmarkNode* node,
                             const std::wstring& model_string) {
  DCHECK(node);
  const std::wstring folder_tell(L":[");
  std::wstring::size_type start_pos = 0;
  std::wstring::size_type end_pos =
      AddNodesFromString(model, node, model_string, start_pos);
  DCHECK(end_pos == std::wstring::npos);
}

}  // namespace model_test_utils
