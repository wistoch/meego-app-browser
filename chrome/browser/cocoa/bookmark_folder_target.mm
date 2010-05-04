// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_folder_target.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

NSString* kBookmarkButtonDragType = @"ChromiumBookmarkButtonDragType";

@implementation BookmarkFolderTarget

- (id)initWithController:(id<BookmarkButtonControllerProtocol>)controller {
  if ((self = [super init])) {
    controller_ = controller;
  }
  return self;
}

// This IBAction is called when the user clicks (mouseUp, really) on a
// "folder" bookmark button.  (In this context, "Click" does not
// include right-click to open a context menu which follows a
// different path).  Scenarios when folder X is clicked:
//  *Predicate*        *Action*
//  (nothing)          Open Folder X
//  Folder X open      Close folder X
//  Folder Y open      Close Y, open X
//  Cmd-click          Open All with proper disposition
//
//  Note complication in which a click-drag engages drag and drop, not
//  a click-to-open.  Thus the path to get here is a little twisted.
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  DCHECK(sender);
  // Watch out for a modifier click.  For example, command-click
  // should open all.
  //
  // NOTE: we cannot use [[sender cell] mouseDownFlags] because we
  // thwart the normal mouse click mechanism to make buttons
  // draggable.  Thus we must use [NSApp currentEvent].
  DCHECK([sender bookmarkNode]->is_folder());
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (disposition == NEW_BACKGROUND_TAB) {
    [controller_ closeAllBookmarkFolders];
    [controller_ openAll:[sender bookmarkNode] disposition:disposition];
    return;
  }

  // If click on same folder, close it and be done.
  // Else we clicked on a different folder so more work to do.
  if ([[controller_ folderController] parentButton] == sender) {
    [controller_ closeBookmarkFolder:controller_];
    return;
  }

  [controller_ addNewFolderControllerWithParentButton:sender];
}

- (void)copyBookmarkNode:(const BookmarkNode*)node
            toPasteboard:(NSPasteboard*)pboard {
  if (!node) {
    NOTREACHED();
    return;
  }

  if (node->is_folder()) {
    // TODO(viettrungluu): I'm not sure what we should do, so just declare the
    // "additional" types we're given for now. Maybe we want to add a list of
    // URLs? Would we then have to recurse if there were subfolders?
    // In the meanwhile, we *must* set it to a known state. (If this survives to
    // a 10.6-only release, it can be replaced with |-clearContents|.)
    [pboard declareTypes:[NSArray array] owner:nil];
  } else {
    const std::string spec = node->GetURL().spec();
    NSString* url = base::SysUTF8ToNSString(spec);
    NSString* title = base::SysWideToNSString(node->GetTitle());
    [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array]
                                              owner:nil];
    [pboard setDataForURL:url title:title];
  }
}

- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button {
  if (const BookmarkNode* node = [button bookmarkNode]) {
    // Put the bookmark information into the pasteboard, and then write our own
    // data for |kBookmarkButtonDragType|.
    [self copyBookmarkNode:node toPasteboard:pboard];
    [pboard addTypes:[NSArray arrayWithObject:kBookmarkButtonDragType]
               owner:nil];
    [pboard setData:[NSData dataWithBytes:&button length:sizeof(button)]
            forType:kBookmarkButtonDragType];
  } else {
    NOTREACHED();
  }
}

@end
