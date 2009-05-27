// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/history/snippet.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkModel;
class BookmarkNode;
class PageNavigator;
class PrefService;
class Profile;

namespace views {
class DropTargetEvent;
}

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks: bookmark manager, bookmark bar view ...
namespace bookmark_utils {

// Calculates the drop operation given |source_operations| and the ideal
// set of drop operations (|operations|). This prefers the following ordering:
// COPY, LINK then MOVE.
int PreferredDropOperation(int source_operations, int operations);

// Returns the drag operations for the specified node.
int BookmarkDragOperation(BookmarkNode* node);

// Returns the preferred drop operation on a bookmark menu/bar.
// |parent| is the parent node the drop is to occur on and |index| the index the
// drop is over.
int BookmarkDropOperation(Profile* profile,
			  const views::DropTargetEvent& event,
			  const BookmarkDragData& data,
			  BookmarkNode* parent,
			  int index);

// Performs a drop of bookmark data onto |parent_node| at |index|. Returns the
// type of drop the resulted.
int PerformBookmarkDrop(Profile* profile,
			const BookmarkDragData& data,
			BookmarkNode* parent_node,
			int index);

// Returns true if the bookmark data can be dropped on |drop_parent| at
// |index|. A drop from a separate profile is always allowed, where as
// a drop from the same profile is only allowed if none of the nodes in
// |data| are an ancestor of |drop_parent| and one of the nodes isn't already
// a child of |drop_parent| at |index|.
bool IsValidDropLocation(Profile* profile,
                         const BookmarkDragData& data,
                         BookmarkNode* drop_parent,
                         int index);

// Clones drag data, adding newly created nodes to |parent| starting at
// |index_to_add_at|.
void CloneDragData(BookmarkModel* model,
                   const std::vector<BookmarkDragData::Element>& elements,
                   BookmarkNode* parent,
                   int index_to_add_at);

// Recursively opens all bookmarks. |initial_disposition| dictates how the
// first URL is opened, all subsequent URLs are opened as background tabs.
// |navigator| is used to open the URLs. If |navigator| is NULL the last
// tabbed browser with the profile |profile| is used. If there is no browser
// with the specified profile a new one is created.
void OpenAll(gfx::NativeView parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition);

// Convenience for opening a single BookmarkNode.
void OpenAll(gfx::NativeView parent,
             Profile* profile,
             PageNavigator* navigator,
             BookmarkNode* node,
             WindowOpenDisposition initial_disposition);

// Copies nodes onto the clipboard. If |remove_nodes| is true the nodes are
// removed after copied to the clipboard. The nodes are copied in such a way
// that if pasted again copies are made.
void CopyToClipboard(BookmarkModel* model,
                     const std::vector<BookmarkNode*>& nodes,
                     bool remove_nodes);

// Pastes from the clipboard. The new nodes are added to |parent|, unless
// |parent| is null in which case this does nothing. The nodes are inserted
// at |index|. If |index| is -1 the nodes are added to the end.
void PasteFromClipboard(BookmarkModel* model,
                        BookmarkNode* parent,
                        int index);

// Returns true if the user can copy from the pasteboard.
bool CanPasteFromClipboard(BookmarkNode* node);

// Returns a vector containing up to |max_count| of the most recently modified
// groups. This never returns an empty vector.
std::vector<BookmarkNode*> GetMostRecentlyModifiedGroups(BookmarkModel* model,
                                                         size_t max_count);

// Returns the most recently added bookmarks. This does not return groups,
// only nodes of type url.
void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<BookmarkNode*>* nodes);

// Used by GetBookmarksMatchingText to return a matching node and the location
// of the match in the title.
struct TitleMatch {
  BookmarkNode* node;

  // Location of the matching words in the title of the node.
  Snippet::MatchPositions match_positions;
};

// Returns true if |n1| was added more recently than |n2|.
bool MoreRecentlyAdded(BookmarkNode* n1, BookmarkNode* n2);

// Returns up to |max_count| bookmarks from |model| whose url or title contains
// the text |text|.
void GetBookmarksContainingText(BookmarkModel* model,
                                const std::wstring& text,
                                size_t max_count,
                                std::vector<BookmarkNode*>* nodes);

// Returns true if |node|'s url or title contains the string |text|.
bool DoesBookmarkContainText(BookmarkNode* node, const std::wstring& text);

// Modifies a bookmark node (assuming that there's no magic that needs to be
// done regarding moving from one folder to another).
void ApplyEditsWithNoGroupChange(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 BookmarkNode* node,
                                 const std::wstring& new_title,
                                 const GURL& new_url,
                                 BookmarkEditor::Handler* handler);

// Modifies a bookmark node assuming that the parent of the node may have
// changed and the node will need to be removed and reinserted.
void ApplyEditsWithPossibleGroupChange(BookmarkModel* model,
                                       BookmarkNode* new_parent,
                                       BookmarkNode* node,
                                       const std::wstring& new_title,
                                       const GURL& new_url,
                                       BookmarkEditor::Handler* handler);

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs.  This is a preference modifier, not a visual modifier.
void ToggleWhenVisible(Profile* profile);

// Register user prefs for BookmarkBar, BookmarkView, ...
void RegisterUserPrefs(PrefService* prefs);

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
//
// NOTE: treat this as a const. It is not const as various tests change the
// value.
extern int num_urls_before_prompting;

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
