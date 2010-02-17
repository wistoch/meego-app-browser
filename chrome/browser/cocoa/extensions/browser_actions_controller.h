// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class Browser;
@class BrowserActionButton;
@class BrowserActionsContainerView;
class Extension;
@class ExtensionPopupController;
class ExtensionToolbarModel;
class ExtensionsServiceObserverBridge;
class Profile;

extern const CGFloat kBrowserActionButtonPadding;

extern NSString* const kBrowserActionsChangedNotification;

@interface BrowserActionsController : NSObject {
 @private
  // Reference to the current browser. Weak.
  Browser* browser_;

  // The view from Toolbar.xib we'll be rendering our browser actions in. Weak.
  BrowserActionsContainerView* containerView_;

  // The current profile. Weak.
  Profile* profile_;

  // The model that tracks the order of the toolbar icons. Weak.
  ExtensionToolbarModel* toolbarModel_;

  // The observer for the ExtensionsService we're getting events from.
  scoped_ptr<ExtensionsServiceObserverBridge> observer_;

  // A dictionary of Extension ID -> BrowserActionButton pairs representing the
  // buttons present in the container view. The ID is a string unique to each
  // extension.
  scoped_nsobject<NSMutableDictionary> buttons_;

  // The order of the BrowserActionButton objects within the dictionary.
  scoped_nsobject<NSMutableArray> buttonOrder_;
}

@property(readonly, nonatomic) BrowserActionsContainerView* containerView;

// Initializes the controller given the current browser and container view that
// will hold the browser action buttons.
- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container;

// Creates and appends any existing browser action buttons present within the
// extensions service to the toolbar.
- (void)createButtons;

// Returns the ideal (not current) width to fit all visible extensions and other
// UI elements in the container nicely.
- (CGFloat)idealContainerWidth;

// Update the display of all buttons.
- (void)update;

// Returns the current number of browser action buttons within the container,
// whether or not they are displayed.
- (NSUInteger)buttonCount;

// Returns the current number of browser action buttons displayed in the
// container.
- (NSUInteger)visibleButtonCount;

// Executes the action designated by the extension.
- (void)browserActionClicked:(BrowserActionButton*)sender;

// Returns the NSView for the action button associated with an extension.
- (NSView*)browserActionViewForExtension:(Extension*)extension;

@end  // @interface BrowserActionsController

@interface BrowserActionsController(TestingAPI)
- (NSButton*)buttonWithIndex:(int)index;
@end

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
