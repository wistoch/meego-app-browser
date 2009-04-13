// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

@class BookmarkView;
@class GrowBoxView;

class BookmarkModel;
class TabContents;
class TabContentsCommandObserver;
class TabStripModel;
@class ToolbarView;

// A class that controls the web contents of a tab. It manages displaying the
// native view for a given TabContents in |contentsBox_|.

@interface TabContentsController : NSViewController {
 @private
  TabContentsCommandObserver* observer_;  // nil if |commands_| is nil
  TabContents* contents_;  // weak

  BookmarkModel* bookmarkModel_;  // weak; one per window

  // TODO(jrg): write a BookmarkView
  IBOutlet ToolbarView* /* BookmarkView* */ bookmarkView_;

  IBOutlet NSBox* contentsBox_;
  IBOutlet GrowBoxView* growBox_;

  // The contents box will have an offset if shrunk to make room for
  // the bookmark bar.
  BOOL contentsBoxHasOffset_;
}

// Create the contents of a tab represented by |contents| and loaded from the
// nib given by |name|.
- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents
        bookmarkModel:(BookmarkModel*)bookmarkModel;

// Take this view (toolbar and web contents) full screen
- (IBAction)fullScreen:(id)sender;

// Called when the tab contents is about to be put into the view hierarchy as
// the selected tab. Handles things such as ensuring the toolbar is correctly
// enabled.
- (void)willBecomeSelectedTab;

// Called when the tab contents is updated in some non-descript way (the
// notification from the model isn't specific). |updatedContents| could reflect
// an entirely new tab contents object.
- (void)tabDidChange:(TabContents*)updatedContents;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of this tab.
- (NSRect)growBoxRect;

// Change the visibility state of the bookmark bar.
- (void)toggleBookmarkBar:(BOOL)enable;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_
