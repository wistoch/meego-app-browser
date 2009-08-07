// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/view_resizer_pong.h"

@implementation ViewResizerPong

@synthesize height = height_;

- (void)resizeView:(NSView*)view newHeight:(float)height {
  [self setHeight:height];

  // Set the view's height and width, in case it uses that as important state.
  [view setFrame:NSMakeRect(100, 100, 250, height)];
}
@end
