// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/accelerated_plugin_view_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface UnderlayCountingWindow : NSWindow {
 @private
  int underlayCount_;
}
@property (readonly, nonatomic) int underlayCount;
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;
@end

@implementation UnderlayCountingWindow
@synthesize underlayCount = underlayCount_;

- (void)underlaySurfaceAdded {
  ++underlayCount_;
}

- (void)underlaySurfaceRemoved {
  --underlayCount_;
}
@end

class AcceleratedPluginViewTest : public PlatformTest {
 protected:
  AcceleratedPluginViewTest() {}

  UnderlayCountingWindow* StubUnderlayWindow() {
    UnderlayCountingWindow* window = [[[UnderlayCountingWindow alloc]
        initWithContentRect:NSMakeRect(20, 20, 300, 200)
                  styleMask:NSBorderlessWindowMask
                    backing:NSBackingStoreBuffered
                      defer:NO] autorelease];
    [window orderFront:nil];
    return window;
  }

  AcceleratedPluginView* StubAcceleratedPluginView() {
    // It truns out the rwhv and the plugin handle are not necessary for
    // this test, and AcceleratedPluginView doesn't crash without them.
    AcceleratedPluginView* view = [[[AcceleratedPluginView alloc]
        initWithRenderWidgetHostViewMac:NULL
                           pluginHandle:0] autorelease];
    return view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratedPluginViewTest);
};

TEST_F(AcceleratedPluginViewTest, Basic) {
}

TEST_F(AcceleratedPluginViewTest, BasicAdding) {
  AcceleratedPluginView* view = StubAcceleratedPluginView();

  UnderlayCountingWindow* window = StubUnderlayWindow();
  EXPECT_EQ(0, [window underlayCount]);

  [[window contentView] addSubview:view];
  EXPECT_EQ(1, [window underlayCount]);

  [view removeFromSuperview];
  EXPECT_EQ(0, [window underlayCount]);
}

TEST_F(AcceleratedPluginViewTest, MoveBetweenWindows) {
  AcceleratedPluginView* view = StubAcceleratedPluginView();

  UnderlayCountingWindow* window1 = StubUnderlayWindow();
  UnderlayCountingWindow* window2 = StubUnderlayWindow();
  EXPECT_EQ(0, [window1 underlayCount]);
  EXPECT_EQ(0, [window2 underlayCount]);

  [[window1 contentView] addSubview:view];
  EXPECT_EQ(1, [window1 underlayCount]);
  EXPECT_EQ(0, [window2 underlayCount]);

  [[window2 contentView] addSubview:view];
  EXPECT_EQ(0, [window1 underlayCount]);
  EXPECT_EQ(1, [window2 underlayCount]);
}
