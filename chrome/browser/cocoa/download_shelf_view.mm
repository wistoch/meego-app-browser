// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_view.h"

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/GTMTheme.h"

@implementation DownloadShelfView

- (NSColor*)strokeColor {
  return [[self gtm_theme] strokeColorForStyle:GTMThemeStyleToolBar
                                         state:[[self window] isKeyWindow]];
}

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];

  GTMTheme* theme = [self gtm_theme];

  NSImage* backgroundImage = [theme backgroundImageForStyle:GTMThemeStyleToolBar
                                               state:GTMThemeStateActiveWindow];
  if (backgroundImage) {
    // We want our backgrounds for the shelf to be phased from the upper
    // left hand corner of the view.
    NSPoint phase = NSMakePoint(0, NSHeight([self bounds]));
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    NSColor* color = [NSColor colorWithPatternImage:backgroundImage];
    [color set];
    NSRectFill([self bounds]);
  } else {
    NSGradient* gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                             state:isKey];
    // TODO(avi) http://crbug.com/36485; base != window
    NSPoint startPoint = [self convertPointFromBase:NSMakePoint(0, 0)];
    NSPoint endPoint = [self convertPointFromBase:
        NSMakePoint(0, [self frame].size.height)];

    [gradient drawFromPoint:startPoint
                    toPoint:endPoint
                    options:NSGradientDrawsBeforeStartingLocation |
                            NSGradientDrawsAfterEndingLocation];
  }

  // Draw top stroke
  [[self strokeColor] set];
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMaxYEdge);
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

// Mouse down events on the download shelf should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

@end
