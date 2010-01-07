// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// The standard close button for our Mac UI which is the "x"
// that changes to the red circle with the "x" when you hover over it.
// At this time it is used by the popup blocker, download bar, info bar
// and tabs.
@interface HoverCloseButton : NSButton {
  // Tracking area for close button mouseover images.
  scoped_nsobject<NSTrackingArea> closeTrackingArea_;
}

// Enables or disables the |NSTrackingRect|s for the button.
- (void)setTrackingEnabled:(BOOL)enabled;

// Sets up the button's images, tracking areas, and accessibility info
// when instantiated via initWithFrame or awakeFromNib.
- (void)commonInit;

// Checks to see whether the mouse is in the button's bounds and update
// the image in case it gets out of sync.  This occurs when you close a
// tab so the tab to the left of it takes its place, and drag the button
// without moving the mouse before you press the button down.
- (void)checkImageState;

@end
