// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_blocked_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DummyContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  DummyContentSettingBubbleModel(ContentSettingsType content_type)
      : ContentSettingBubbleModel(NULL, NULL, content_type) {
    RadioGroup radio_group;
    radio_group.default_item = 0;
    radio_group.radio_items.resize(2);
    radio_group.is_mutable = true;
    set_radio_group(radio_group);
  }
};

class ContentBlockedBubbleControllerTest : public CocoaTest {
};

// Check that the bubble doesn't crash or leak for any settings type
TEST_F(ContentBlockedBubbleControllerTest, Init) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType settingsType = static_cast<ContentSettingsType>(i);

    scoped_nsobject<NSWindow> parent([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 800, 600)
                  styleMask:NSBorderlessWindowMask
                    backing:NSBackingStoreBuffered
           defer:NO]);
    [parent setReleasedWhenClosed:NO];
    if (DebugUtil::BeingDebugged())
      [parent.get() orderFront:nil];
    else
      [parent.get() orderBack:nil];

    ContentBlockedBubbleController* controller = [ContentBlockedBubbleController
        showForModel:new DummyContentSettingBubbleModel(settingsType)
        parentWindow:parent
         anchoredAt:NSMakePoint(50, 20)];
    EXPECT_TRUE(controller != nil);
    [controller showWindow:nil];
    [parent.get() close];
  }
}

}  // namespace


