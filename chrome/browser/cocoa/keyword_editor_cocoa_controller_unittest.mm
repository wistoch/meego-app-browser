// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface FakeKeywordEditorController : KeywordEditorCocoaController {
 @public
  BOOL changed_;
}
- (KeywordEditorModelObserver*)observer;
@end

@implementation FakeKeywordEditorController
- (void)modelChanged {
  changed_ = YES;
}
- (KeywordEditorModelObserver*)observer {
  return observer_.get();
}
@end

// TODO(rsesek): Figure out a good way to test this class (crbug.com/21640).

namespace {

class KeywordEditorCocoaControllerTest : public PlatformTest {
 public:
  void SetUp() {
    TestingProfile* profile =
        static_cast<TestingProfile*>(browser_helper_.profile());
    profile->CreateTemplateURLModel();
    controller_.reset(
        [[FakeKeywordEditorController alloc] initWithProfile:profile]);
  }

  // Helper to count the keyword editors.
  NSUInteger CountKeywordEditors() {
    base::ScopedNSAutoreleasePool pool;
    NSUInteger count = 0;
    for (NSWindow* window in [NSApp windows]) {
      id controller = [window windowController];
      if ([controller isKindOfClass:[KeywordEditorCocoaController class]]) {
        ++count;
      }
    }
    return count;
  }

  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  scoped_nsobject<FakeKeywordEditorController> controller_;
};

TEST_F(KeywordEditorCocoaControllerTest, TestModelChanged) {
  EXPECT_FALSE(controller_.get()->changed_);
  KeywordEditorModelObserver* observer = [controller_ observer];
  observer->OnTemplateURLModelChanged();
  EXPECT_TRUE(controller_.get()->changed_);
}

// Test that the window shows correctly, and the controller is
// released correctly.
TEST_F(KeywordEditorCocoaControllerTest, ShowAndCloseWindow) {
  // |controller_| is the only reference.
  EXPECT_EQ([controller_.get() retainCount], 1U);

  // TODO(shess): This test verifies that it leaks no windows.  Work
  // to push this expectation up into the unit testing framework.

  const NSUInteger initial_window_count([[NSApp windows] count]);

  // Explicit autorelease pool here because [NSApp windows] returns an
  // autorelease immutable NSArray, which otherwise pins the window.
  {
    base::ScopedNSAutoreleasePool pool;

    // -showWindow: brings up the window (which retains
    // |controller_|).
    [controller_.get() showWindow:nil];
    EXPECT_EQ([[NSApp windows] count], initial_window_count+1);

    // In regular usage, our scoped reference would not exist and
    // |controller_| would manage itself once -showWindow: is called.
    // This means that we need another reference to balance things
    // out.
    [controller_.get() retain];

    // Closing the window should leave us with the single reference.
    [controller_.get() close];
  }

  // |controller_| still has a handle on the window, drop the last
  // reference so we can check that we didn't leak a window.
  EXPECT_EQ([controller_.get() retainCount], 1U);
  controller_.reset();

  // All created windows should be gone.
  EXPECT_EQ([[NSApp windows] count], initial_window_count);
}

// Test that +showKeywordEditor brings up the existing editor and
// creates one if needed.
TEST_F(KeywordEditorCocoaControllerTest, ShowKeywordEditor) {
  // No outstanding editors.
  Profile* profile(browser_helper_.profile());
  KeywordEditorCocoaController* sharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(nil == sharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 0U);

  const NSUInteger initial_window_count([[NSApp windows] count]);

  // The window unwinds using -autorelease, so we need to introduce an
  // autorelease pool to really test whether it went away or not.
  {
    base::ScopedNSAutoreleasePool pool;

    // +showKeywordEditor: creates a new controller.
    [KeywordEditorCocoaController showKeywordEditor:profile];
    sharedInstance =
        [KeywordEditorCocoaController sharedInstanceForProfile:profile];
    EXPECT_TRUE(sharedInstance);
    EXPECT_EQ(CountKeywordEditors(), 1U);

    // Another call doesn't create another controller.
    [KeywordEditorCocoaController showKeywordEditor:profile];
    EXPECT_TRUE(sharedInstance ==
        [KeywordEditorCocoaController sharedInstanceForProfile:profile]);
    EXPECT_EQ(CountKeywordEditors(), 1U);

    [sharedInstance close];
  }

  // No outstanding editors.
  sharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(nil == sharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 0U);

  // Windows we created should be gone.
  EXPECT_EQ([[NSApp windows] count], initial_window_count);

  // Get a new editor, should be different from the previous one.
  [KeywordEditorCocoaController showKeywordEditor:profile];
  KeywordEditorCocoaController* newSharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(sharedInstance != newSharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 1U);
  [newSharedInstance close];
}

}  // namespace
