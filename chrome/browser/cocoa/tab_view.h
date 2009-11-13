// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_VIEW_H_
#define CHROME_BROWSER_COCOA_TAB_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/background_gradient_view.h"

@class TabController, TabWindowController;

// A view that handles the event tracking (clicking and dragging) for a tab
// on the tab strip. Relies on an associated TabController to provide a
// target/action for selecting the tab.

@interface TabView : BackgroundGradientView {
 @private
  IBOutlet TabController* controller_;
  // TODO(rohitrao): Add this button to a CoreAnimation layer so we can fade it
  // in and out on mouseovers.
  IBOutlet NSButton* closeButton_;
  BOOL isClosing_;

  // Tracking area for close button mouseover images.
  scoped_nsobject<NSTrackingArea> closeTrackingArea_;

  // All following variables are valid for the duration of a drag.
  // These are released on mouseUp:
  BOOL moveWindowOnDrag_;  // Set if the only tab of a window is dragged.
  BOOL tabWasDragged_;  // Has the tab been dragged?
  BOOL draggingWithinTabStrip_;  // Did drag stay in the current tab strip?
  BOOL chromeIsVisible_;
  BOOL isMouseInside_;  // Is the mouse hovering over?
  CGFloat hoverAlpha_;  // How strong the mouse hover state is.
  NSTimeInterval lastHoverUpdate_;  // Time the hover value was last updated.
  NSPoint hoverPoint_;  // Current location of hover in view coords.

  NSTimeInterval tearTime_;  // Time since tear happened
  NSPoint tearOrigin_;  // Origin of the tear rect
  NSPoint dragOrigin_;  // Origin point of the drag
  // TODO(alcor): these references may need to be strong to avoid crashes
  // due to JS closing windows
  TabWindowController* sourceController_;  // weak. controller starting the drag
  NSWindow* sourceWindow_;  // weak. The window starting the drag
  NSRect sourceWindowFrame_;
  NSRect sourceTabFrame_;

  TabWindowController* draggedController_;  // weak. Controller being dragged.
  NSWindow* dragWindow_;  // weak. The window being dragged
  NSWindow* dragOverlay_;  // weak. The overlay being dragged

  TabWindowController* targetController_;  // weak. Controller being targeted
  NSCellStateValue state_;
}
@property(assign) NSCellStateValue state;
@property(assign, nonatomic) CGFloat hoverAlpha;

// Determines if the tab is in the process of animating closed. It may still
// be visible on-screen, but should not respond to/initiate any events.
@property(assign, nonatomic) BOOL isClosing;

// Enables/Disables tracking regions for the tab.
- (void)setTrackingEnabled:(BOOL)enabled;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_VIEW_H_
