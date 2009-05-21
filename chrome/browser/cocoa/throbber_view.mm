// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/throbber_view.h"

#include "base/logging.h"

const float kAnimationIntervalSeconds = 0.03;  // 30ms, same as windows

@interface ThrobberView(PrivateMethods)
- (void)animate;
@end

// A very simple object that is the target for the animation timer so that
// the view isn't. We do this to avoid retain cycles as the timer
// retains its target.
@interface TimerTarget : NSObject {
 @private
  ThrobberView* throbber_;  // Weak, owns us
}
- (id)initWithThrobber:(ThrobberView*)view;
@end

@implementation TimerTarget
- (id)initWithThrobber:(ThrobberView*)view {
  if ((self = [super init])) {
    throbber_ = view;
  }
  return self;
}

- (void)animate:(NSTimer*)timer {
  [throbber_ animate];
}
@end

@implementation ThrobberView

- (id)initWithFrame:(NSRect)frame image:(NSImage*)image {
  if ((self = [super initWithFrame:frame])) {
    // Ensure that the height divides evenly into the width. Cache the
    // number of frames in the animation for later.
    NSSize imageSize = [image size];
    DCHECK(imageSize.height && imageSize.width);
    if (!imageSize.height)
      return nil;
    DCHECK((int)imageSize.width % (int)imageSize.height == 0);
    numFrames_ = (int)imageSize.width / (int)imageSize.height;
    DCHECK(numFrames_);

    // First check if we have a bitmap image rep and use it, otherwise fall
    // back to creating one.
    NSBitmapImageRep* rep = [[image representations] objectAtIndex:0];
    if (![rep isKindOfClass:[NSBitmapImageRep class]]) {
      [image lockFocus];
      NSRect imageRect = NSMakeRect(0, 0, imageSize.width, imageSize.height);
      rep = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:imageRect]
                autorelease];
      [image unlockFocus];
    }
    image_.reset([[CIImage alloc] initWithBitmapImageRep:rep]);

    // Start a timer for the animation frames.
    target_.reset([[TimerTarget alloc] initWithThrobber:self]);
    timer_ =
        [NSTimer scheduledTimerWithTimeInterval:kAnimationIntervalSeconds
                                         target:target_.get()
                                       selector:@selector(animate:)
                                       userInfo:nil
                                        repeats:YES];
  }
  return self;
}

- (void)dealloc {
  [timer_ invalidate];
  [super dealloc];
}

- (BOOL)isOpaque {
  return YES;
}

- (void)removeFromSuperview {
  [timer_ invalidate];
  timer_ = nil;

  [super removeFromSuperview];
}

// Called when the TimerTarget gets tickled by our timer. Increment the frame
// counter and mark as needing display.
- (void)animate {
  animationFrame_ = ++animationFrame_ % numFrames_;
  [self setNeedsDisplay:YES];
}

// Overridden to draw the appropriate frame in the image strip.
- (void)drawRect:(NSRect)rect {
#if 0
  float imageDimension = [image_ extent].size.height;
  float xOffset = animationFrame_ * imageDimension;
  NSRect sourceImageRect =
      NSMakeRect(xOffset, 0, imageDimension, imageDimension);
  [image_ drawInRect:[self bounds]
             fromRect:sourceImageRect
            operation:NSCompositeSourceOver
             fraction:1.0];
#endif
}

@end
