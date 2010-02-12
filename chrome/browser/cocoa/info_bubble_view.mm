// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/info_bubble_view.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/GTMTheme.h"

// TODO(andybons): confirm constants with UI dudes.
extern const CGFloat kBubbleArrowHeight = 8.0;
extern const CGFloat kBubbleArrowWidth = 15.0;
extern const CGFloat kBubbleArrowXOffset = 10.0;
extern const CGFloat kBubbleCornerRadius = 8.0;

@implementation InfoBubbleView

@synthesize arrowLocation = arrowLocation_;
@synthesize bubbleType = bubbleType_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    arrowLocation_ = kTopLeft;
    bubbleType_ = kGradientInfoBubble;
  }

  return self;
}

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.height -= kBubbleArrowHeight;
  NSBezierPath* bezier = [NSBezierPath bezierPath];
  rect.size.height -= kBubbleArrowHeight;

  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds
                                  xRadius:kBubbleCornerRadius
                                  yRadius:kBubbleCornerRadius];

  // Add the bubble arrow.
  CGFloat dX = 0;
  switch (arrowLocation_) {
    case kTopLeft:
      dX = kBubbleArrowXOffset;
      break;
    case kTopRight:
      dX = NSWidth(bounds) - kBubbleArrowXOffset - kBubbleArrowWidth;
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += dX;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth/2.0,
                                  arrowStart.y + kBubbleArrowHeight)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];

  // Then fill the inside depending on the type of bubble.
  if (bubbleType_ == kGradientInfoBubble) {
    GTMTheme *theme = [GTMTheme defaultTheme];
    NSGradient *gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                             state:NO];
    [gradient drawInBezierPath:bezier angle:0.0];
  } else if (bubbleType_ == kWhiteInfoBubble) {
    [[NSColor whiteColor] set];
    [bezier fill];
  }
}

@end
