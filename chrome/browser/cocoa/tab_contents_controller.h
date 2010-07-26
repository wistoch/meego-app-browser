// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
#pragma once

#include <Cocoa/Cocoa.h>

class TabContents;
class TabContentsCommandObserver;
class TabStripModel;

// A class that controls the web contents of a tab. It manages displaying the
// native view for a given TabContents and optionally its docked devtools in
// |contentsContainer_|.
// Note that just creating the class does not display the view in
// |contentsContainer_|. We defer inserting it until the box is the correct size
// to avoid multiple resize messages to the renderer. You must call
// |-ensureContentsVisible| to display the render widget host view.

@interface TabContentsController : NSViewController {
 @private
  TabContentsCommandObserver* observer_;  // nil if |commands_| is nil
  TabContents* contents_;  // weak

  IBOutlet NSSplitView* contentsContainer_;
}

// Create the contents of a tab represented by |contents| and loaded from the
// nib given by |name|.
- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents;

// Called when the tab contents is the currently selected tab and is about to be
// removed from the view hierarchy.
- (void)willBecomeUnselectedTab;

// Called when the tab contents is about to be put into the view hierarchy as
// the selected tab. Handles things such as ensuring the toolbar is correctly
// enabled.
- (void)willBecomeSelectedTab;

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible;

// Called when the tab contents is updated in some non-descript way (the
// notification from the model isn't specific). |updatedContents| could reflect
// an entirely new tab contents object.
- (void)tabDidChange:(TabContents*)updatedContents;

// Shows |devToolsContents| in a split view, or removes the bottom view in the
// split viewif |devToolsContents| is NULL.
// TODO(thakis): Either move this to tab_window or move infobar handling to here
// too -- http://crbug.com/31633 .
- (void)showDevToolsContents:(TabContents*)devToolsContents;

// Returns the height required by devtools and divider, or 0 if no devtools are
// docked to the tab.
- (CGFloat)devToolsHeight;
@end

#endif  // CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
