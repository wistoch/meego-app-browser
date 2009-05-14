// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface BrowserWindowController (ExposedForTesting)
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;
@end

class BrowserWindowControllerTest : public testing::Test {
  virtual void SetUp() {
    controller_.reset([[BrowserWindowController alloc]
                        initWithBrowser:browser_helper_.browser()
                          takeOwnership:NO]);
  }

 public:
  // Order is very important here.  We want the controller deleted
  // before the pool, and want the pool deleted before
  // BrowserTestHelper.
  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  base::ScopedNSAutoreleasePool pool_;
  scoped_nsobject<BrowserWindowController> controller_;
};

TEST_F(BrowserWindowControllerTest, TestSaveWindowPosition) {
  PrefService* prefs = browser_helper_.profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  ASSERT_TRUE(prefs->GetDictionary(prefs::kBrowserWindowPlacement) == NULL);

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.
  [controller_ saveWindowPositionToPrefs:prefs];
  EXPECT_TRUE(prefs->GetDictionary(prefs::kBrowserWindowPlacement) != NULL);
}

/* TODO(???): test other methods of BrowserWindowController */
