// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_tree_browser_cell.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

@implementation BookmarkTreeBrowserCell

@synthesize matrix = matrix_;
@synthesize target = target_;
@synthesize action = action_;

- (const BookmarkNode*)bookmarkNode {
  return bookmarkNode_;
}

- (void)setBookmarkNode:(const BookmarkNode*)bookmarkNode {
  bookmarkNode_ = bookmarkNode;
}

@end
