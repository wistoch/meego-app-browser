// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/tab_controller_target.h"

@class TabStripView;

class Browser;
class TabStripModelObserverBridge;
class TabStripModel;
class TabContents;
class ToolbarModel;

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// When a new tab is created, we create a TabController which manages loading
// the contents, including toolbar, from a separate nib file. This controller
// then handles replacing the contentView of the window. As tabs are switched,
// the single child of the contentView is swapped around to hold the contents
// (toolbar and all) representing that tab.

@interface TabStripController : NSObject <TabControllerTarget> {
 @private
  TabContents* currentTab_;   // weak, tab for which we're showing state
  TabStripView* tabView_;  // weak
  NSView* switchView_;  // weak
  NSButton* newTabButton_;  // weak, obtained from the nib.
  scoped_ptr<TabStripModelObserverBridge> bridge_;
  TabStripModel* tabModel_;  // weak
  // access to the TabContentsControllers (which own the parent view
  // for the toolbar and associated tab contents) given an index. This needs
  // to be kept in the same order as the tab strip's model as we will be
  // using its index from the TabStripModelObserver calls.
  scoped_nsobject<NSMutableArray> tabContentsArray_;
  // an array of TabControllers which manage the actual tab views. As above,
  // this is kept in the same order as the tab strip model.
  scoped_nsobject<NSMutableArray> tabArray_;
}

// Initialize the controller with a view and browser that contains
// everything else we'll need. |switchView| is the view whose contents get
// "switched" every time the user switches tabs. The children of this view
// will be released, so if you want them to stay around, make sure
// you have retained them.
- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
           browser:(Browser*)browser;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Given a tab view in the strip, return its index. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
