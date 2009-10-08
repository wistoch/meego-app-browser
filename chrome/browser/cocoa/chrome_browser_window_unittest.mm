// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

NSEvent* KeyEvent(const NSUInteger flags, const NSUInteger keyCode) {
  return [NSEvent keyEventWithType:NSKeyDown
                          location:NSZeroPoint
                     modifierFlags:flags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:@""
       charactersIgnoringModifiers:@""
                         isARepeat:NO
                          keyCode:keyCode];
}

class ChromeBrowserWindowTest : public PlatformTest {
 public:
  ChromeBrowserWindowTest() {
    // Create a window.
    const NSUInteger mask = NSTitledWindowMask | NSClosableWindowMask |
        NSMiniaturizableWindowMask | NSResizableWindowMask;
    window_.reset([[ChromeBrowserWindow alloc]
                    initWithContentRect:NSMakeRect(0, 0, 800, 600)
                              styleMask:mask
                                backing:NSBackingStoreBuffered
                                  defer:NO]);
    if (DebugUtil::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }
  }

  // Returns a canonical snapshot of the window.
  NSData* WindowContentsAsTIFF() {
    NSRect frame([window_ frame]);
    frame.origin = [window_ convertScreenToBase:frame.origin];

    NSData* pdfData = [window_ dataWithPDFInsideRect:frame];

    // |pdfData| can differ for windows which look the same, so make it
    // canonical.
    NSImage* image = [[[NSImage alloc] initWithData:pdfData] autorelease];
    return [image TIFFRepresentation];
  }

  CocoaNoWindowTestHelper cocoa_helper_;
  scoped_nsobject<ChromeBrowserWindow> window_;
};

// Baseline test that the window creates, displays, closes, and
// releases.
TEST_F(ChromeBrowserWindowTest, ShowAndClose) {
  [window_ display];
}

// Verify that the window intercepts a particular key event and
// forwards it to [delegate executeCommand:].  Assume that other
// CommandForKeyboardShortcut() will work the same for the rest.
TEST_F(ChromeBrowserWindowTest, PerformKeyEquivalentForwardToExecuteCommand) {
  NSEvent *event = KeyEvent(NSCommandKeyMask, kVK_ANSI_1);

  id delegate = [OCMockObject mockForClass:[BrowserWindowController class]];
  // -stub to satisfy the DCHECK.
  BOOL yes = YES;
  [[[delegate stub] andReturnValue:OCMOCK_VALUE(yes)]
    isKindOfClass:[BrowserWindowController class]];
  [[delegate expect] executeCommand:IDC_SELECT_TAB_0];

  [window_ setDelegate:delegate];
  [window_ performKeyEquivalent:event];

  // Don't wish to mock all the way down...
  [window_ setDelegate:nil];
  [delegate verify];
}

// Verify that an unhandled shortcut does not get forwarded via
// -executeCommand:.
// TODO(shess) Think of a way to test that it is sent to the
// superclass.
TEST_F(ChromeBrowserWindowTest, PerformKeyEquivalentNoForward) {
  NSEvent *event = KeyEvent(0, 0);

  id delegate = [OCMockObject mockForClass:[BrowserWindowController class]];
  // -stub to satisfy the DCHECK.
  BOOL yes = YES;
  [[[delegate stub] andReturnValue:OCMOCK_VALUE(yes)]
    isKindOfClass:[BrowserWindowController class]];

  [window_ setDelegate:delegate];
  [window_ performKeyEquivalent:event];

  // Don't wish to mock all the way down...
  [window_ setDelegate:nil];
  [delegate verify];
}

// Test that undocumented title-hiding API we're using does the job.
TEST_F(ChromeBrowserWindowTest, DoesHideTitle) {
  // The -display calls are not strictly necessary, but they do
  // make it easier to see what's happening when debugging (without
  // them the changes are never flushed to the screen).

  [window_ setTitle:@""];
  [window_ display];
  NSData* emptyTitleData = WindowContentsAsTIFF();

  [window_ setTitle:@"This is a title"];
  [window_ display];
  NSData* thisTitleData = WindowContentsAsTIFF();

  // The default window with a title should look different from the
  // window with an emtpy title.
  EXPECT_FALSE([emptyTitleData isEqualToData:thisTitleData]);

  [window_ setShouldHideTitle:YES];
  [window_ setTitle:@""];
  [window_ display];
  [window_ setTitle:@"This is a title"];
  [window_ display];
  NSData* hiddenTitleData = WindowContentsAsTIFF();

  // With our magic setting, the window with a title should look the
  // same as the window with an empty title.
  EXPECT_TRUE([window_ _isTitleHidden]);
  EXPECT_TRUE([emptyTitleData isEqualToData:hiddenTitleData]);
}

}  // namespace
