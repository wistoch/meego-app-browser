// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser object. Handles
// interactions between Cocoa and the cross-platform code. Each window has a
// single set of toolbars (main toolbar, bookmark bar, etc) and, by virtue of
// being a TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/tab_window_controller.h"
#import "chrome/browser/cocoa/toolbar_view.h"

class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
class LocationBar;
class TabContents;
@class TabContentsController;
@class TabStripController;
class TabStripModelObserverBridge;
@class TabStripView;
@class ToolbarController;

@interface BrowserWindowController :
    TabWindowController<NSUserInterfaceValidations> {
 @private
  scoped_nsobject<TabStripController> tabStripController_;
  scoped_nsobject<ToolbarController> toolbarController_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<TabStripModelObserverBridge> tabObserver_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow;

// Get the C++ bridge object representing the location bar for the current tab.
- (LocationBar*)locationBar;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Called to tell the selected tab to update its loading state.
- (void)setIsLoading:(BOOL)isLoading;

// Called to start/stop the loading animations.
- (void)updateLoadingAnimations:(BOOL)animate;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

- (BOOL)isBookmarkBarVisible;

- (void)toggleBookmarkBar;

@end

#endif  // CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
