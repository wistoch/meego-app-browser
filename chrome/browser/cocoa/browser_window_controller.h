// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/tab_window_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#import "chrome/browser/cocoa/view_resizer.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
class ConstrainedWindowMac;
@class DownloadShelfController;
@class ExtensionShelfController;
@class FindBarCocoaController;
@class GTMWindowSheetController;
@class InfoBarContainerController;
class LocationBar;
class StatusBubble;
class TabContents;
@class TabContentsController;
@class TabStripController;
class TabStripModelObserverBridge;
@class TabStripView;
@class ToolbarController;
@class TitlebarController;

@interface BrowserWindowController :
  TabWindowController<NSUserInterfaceValidations,
                      BookmarkURLOpener,
                      BookmarkBubbleControllerDelegate,
                      ViewResizer,
                      GTMThemeDelegate> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc). We hold a strong ref to the
  // window so that it will live after the NSWindowController dealloc has run
  // (which happens *before* these scoped pointers are torn down). Keeping it
  // alive ensures that weak view or window pointers remain valid through
  // their destruction sequence.
  scoped_ptr<Browser> browser_;
  scoped_nsobject<NSWindow> window_;
  scoped_nsobject<NSWindow> fullscreen_window_;
  scoped_ptr<TabStripModelObserverBridge> tabObserver_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
  scoped_nsobject<ToolbarController> toolbarController_;
  scoped_nsobject<TitlebarController> titlebarController_;
  scoped_nsobject<TabStripController> tabStripController_;
  scoped_nsobject<FindBarCocoaController> findBarCocoaController_;
  scoped_nsobject<InfoBarContainerController> infoBarContainerController_;
  scoped_ptr<StatusBubble> statusBubble_;
  scoped_nsobject<DownloadShelfController> downloadShelfController_;
  scoped_nsobject<ExtensionShelfController> extensionShelfController_;
  scoped_nsobject<BookmarkBarController> bookmarkBarController_;
  scoped_nsobject<BookmarkBubbleController> bookmarkBubbleController_;
  scoped_nsobject<GTMTheme> theme_;
  BOOL ownsBrowser_;  // Only ever NO when testing
  BOOL fullscreen_;
  CGFloat verticalOffsetForStatusBubble_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// Call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Access the C++ bridge object representing the location bar.
- (LocationBar*)locationBar;

// Access the C++ bridge object representing the status bubble for the window.
- (StatusBubble*)statusBubble;

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

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

- (BOOL)isBookmarkBarVisible;

// Called after the visibility perf changed.
- (void)updateBookmarkBarVisibility;

- (BOOL)isDownloadShelfVisible;

// Lazily creates the download shelf in visible state if it doesn't exist yet.
- (DownloadShelfController*)downloadShelf;

// Retains the given FindBarCocoaController and adds its view to this
// browser window.  Must only be called once per
// BrowserWindowController.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// Enters (or exits) fullscreen mode.
- (void)setFullscreen:(BOOL)fullscreen;

// Returns fullscreen state.
- (BOOL)isFullscreen;

// The user changed the theme.
- (void)userChangedTheme;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_dll_resource.h" file.
- (void)executeCommand:(int)command;

// Delegate method for the status bubble to query about its vertical offset.
- (float)verticalOffsetForStatusBubble;

// Show the bookmark bubble (e.g. user just clicked on the STAR)
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyBookmarked;

// Returns the (lazily created) window sheet controller of this window. Used
// for the per-tab sheets.
- (GTMWindowSheetController*)sheetController;

- (void)attachConstrainedWindow:(ConstrainedWindowMac*)window;
- (void)removeConstrainedWindow:(ConstrainedWindowMac*)window;

@end


@interface BrowserWindowController(TestingAPI)

// Put the incognito badge on the browser and adjust the tab strip
// accordingly.
- (void)installIncognitoBadge;

// Allows us to initWithBrowser withOUT taking ownership of the browser.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt;

// Return an autoreleased NSWindow suitable for fullscreen use.
- (NSWindow*)fullscreenWindow;

// Return a point suitable for the topLeft for a bookmark bubble.
- (NSPoint)topLeftForBubble;

@end  // BrowserWindowController(TestingAPI)

#endif  // CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
