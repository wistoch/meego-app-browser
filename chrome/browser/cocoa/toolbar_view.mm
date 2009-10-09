// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_view.h"

@implementation ToolbarView

// Prevent mouse down events from moving the parent window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)drawRect:(NSRect)rect {
  // The toolbar's background pattern is phased relative to the
  // tab strip view's background pattern.
  NSPoint phase = [self gtm_themePatternPhase];
  [[NSGraphicsContext currentContext] setPatternPhase:phase];
  [self drawBackground];
}

@end
