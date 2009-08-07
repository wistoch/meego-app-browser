// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ToolbarControllerTest : public testing::Test {
 public:

  // Indexes that match the ordering returned by the private ToolbarController
  // |-toolbarViews| method.
  enum {
    kBackIndex, kForwardIndex, kReloadIndex, kHomeIndex, kStarIndex, kGoIndex,
    kPageIndex, kWrenchIndex, kLocationIndex,
  };

  ToolbarControllerTest() {
    Browser* browser = helper_.browser();
    CommandUpdater* updater = browser->command_updater();
    // The default state for the commands is true, set a couple to false to
    // ensure they get picked up correct on initialization
    updater->UpdateCommandEnabled(IDC_BACK, false);
    updater->UpdateCommandEnabled(IDC_FORWARD, false);
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    bar_.reset(
        [[ToolbarController alloc] initWithModel:browser->toolbar_model()
                                        commands:browser->command_updater()
                                         profile:helper_.profile()
                                  resizeDelegate:resizeDelegate_.get()
                                bookmarkDelegate:nil]);
    EXPECT_TRUE([bar_ view]);
    NSView* parent = [cocoa_helper_.window() contentView];
    [parent addSubview:[bar_ view]];
  }

  // Make sure the enabled state of the view is the same as the corresponding
  // command in the updater. The views are in the declaration order of outlets.
  void CompareState(CommandUpdater* updater, NSArray* views) {
    EXPECT_EQ(updater->IsCommandEnabled(IDC_BACK),
              [[views objectAtIndex:kBackIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_FORWARD),
              [[views objectAtIndex:kForwardIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_RELOAD),
              [[views objectAtIndex:kReloadIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_HOME),
              [[views objectAtIndex:kHomeIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_STAR),
              [[views objectAtIndex:kStarIndex] isEnabled] ? true : false);
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  scoped_nsobject<ToolbarController> bar_;
};

// Test the initial state that everything is sync'd up
TEST_F(ToolbarControllerTest, InitialState) {
  CommandUpdater* updater = helper_.browser()->command_updater();
  CompareState(updater, [bar_ toolbarViews]);
}

// Make sure awakeFromNib created a bookmarkBarController
TEST_F(ToolbarControllerTest, AwakeFromNibCreatesBMBController) {
  EXPECT_TRUE([bar_ bookmarkBarController]);
}

// Make sure a "titlebar only" toolbar works
TEST_F(ToolbarControllerTest, TitlebarOnly) {
  NSView* view = [bar_ view];
  EXPECT_TRUE([bar_ bookmarkBarController]);

  [bar_ setHasToolbar:NO];
  EXPECT_NE(view, [bar_ view]);
  EXPECT_FALSE([bar_ bookmarkBarController]);

  // Simulate a popup going fullscreen and back.
  NSView* superview = [view superview];
  // TODO(jrg): find a way to add an [NSAutoreleasePool drain] in
  // here.  I don't have access to the current
  // scoped_nsautorelease_pool to do it properly :-(
  [view removeFromSuperview];
  [superview addSubview:view];

  [bar_ setHasToolbar:YES];
  EXPECT_EQ(view, [bar_ view]);
  EXPECT_TRUE([bar_ bookmarkBarController]);

  // Leave it off to make sure that's fine
  [bar_ setHasToolbar:NO];
}


// Make some changes to the enabled state of a few of the buttons and ensure
// that we're still in sync.
TEST_F(ToolbarControllerTest, UpdateEnabledState) {
  CommandUpdater* updater = helper_.browser()->command_updater();
  EXPECT_FALSE(updater->IsCommandEnabled(IDC_BACK));
  EXPECT_FALSE(updater->IsCommandEnabled(IDC_FORWARD));
  updater->UpdateCommandEnabled(IDC_BACK, true);
  updater->UpdateCommandEnabled(IDC_FORWARD, true);
  CompareState(updater, [bar_ toolbarViews]);
}

TEST_F(ToolbarControllerTest, StarredState) {
  // TODO(pinkerton): I'm not sure how to test this, as the only difference
  // in internal state is in the image used. I tried using the name of the
  // image on the button but it doesn't seem to stick to the NSImage, even
  // when explicitly set.
}

// Focus the location bar and make sure that it's the first responder.
TEST_F(ToolbarControllerTest, FocusLocation) {
  NSWindow* window = cocoa_helper_.window();
  [window makeFirstResponder:[window contentView]];
  EXPECT_EQ([window firstResponder], [window contentView]);
  [bar_ focusLocationBar];
  EXPECT_NE([window firstResponder], [window contentView]);
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  EXPECT_EQ([window firstResponder], [(id)locationBar currentEditor]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(ToolbarControllerTest, Display) {
  [[bar_ view] display];
}

TEST_F(ToolbarControllerTest, LoadingState) {
  // TODO(pinkerton): Same problem testing this as the starred state above.

  // In its initial state, the go button has a tag of IDC_GO. When loading,
  // it should be IDC_STOP.
  NSButton* go = [[bar_ toolbarViews] objectAtIndex:kGoIndex];
  EXPECT_EQ([go tag], IDC_GO);
  [bar_ setIsLoading:YES];
  EXPECT_EQ([go tag], IDC_STOP);
  [bar_ setIsLoading:NO];
  EXPECT_EQ([go tag], IDC_GO);
}

// Check that toggling the state of the home button changes the visible
// state of the home button and moves the other buttons accordingly.
TEST_F(ToolbarControllerTest, ToggleHome) {
  PrefService* prefs = helper_.profile()->GetPrefs();
  bool showHome = prefs->GetBoolean(prefs::kShowHomeButton);
  NSView* homeButton = [[bar_ toolbarViews] objectAtIndex:kHomeIndex];
  EXPECT_EQ(showHome, ![homeButton isHidden]);

  NSView* starButton = [[bar_ toolbarViews] objectAtIndex:kStarIndex];
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  NSRect originalStarFrame = [starButton frame];
  NSRect originalLocationBarFrame = [locationBar frame];

  // Toggle the pref and make sure the button changed state and the other
  // views moved.
  prefs->SetBoolean(prefs::kShowHomeButton, !showHome);
  EXPECT_EQ(showHome, [homeButton isHidden]);
  EXPECT_NE(NSMinX(originalStarFrame), NSMinX([starButton frame]));
  EXPECT_NE(NSMinX(originalLocationBarFrame), NSMinX([locationBar frame]));
  EXPECT_NE(NSWidth(originalLocationBarFrame), NSWidth([locationBar frame]));
}

TEST_F(ToolbarControllerTest, TogglePageWrench) {
  PrefService* prefs = helper_.profile()->GetPrefs();
  bool showButtons = prefs->GetBoolean(prefs::kShowPageOptionsButtons);
  NSView* pageButton = [[bar_ toolbarViews] objectAtIndex:kPageIndex];
  NSView* wrenchButton = [[bar_ toolbarViews] objectAtIndex:kWrenchIndex];
  EXPECT_EQ(showButtons, ![pageButton isHidden]);
  EXPECT_EQ(showButtons, ![wrenchButton isHidden]);

  NSView* goButton = [[bar_ toolbarViews] objectAtIndex:kGoIndex];
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  NSRect originalGoFrame = [goButton frame];
  NSRect originalLocationBarFrame = [locationBar frame];

  // Toggle the pref and make sure the buttons changed state and the other
  // views moved (or in the case of the location bar, it changed width).
  prefs->SetBoolean(prefs::kShowPageOptionsButtons, !showButtons);
  EXPECT_EQ(showButtons, [pageButton isHidden]);
  EXPECT_EQ(showButtons, [wrenchButton isHidden]);
  EXPECT_NE(NSMinX(originalGoFrame), NSMinX([goButton frame]));
  EXPECT_NE(NSWidth(originalLocationBarFrame), NSWidth([locationBar frame]));
}

TEST_F(ToolbarControllerTest, BookmarkBarResizes) {
  NSView* bookmarkBarView = [[bar_ bookmarkBarController] view];
  ASSERT_EQ(0, NSHeight([bookmarkBarView frame]));
  [resizeDelegate_ setHeight:-1];

  // Resize the bookmarkbar to 30px.  The toolbar should ask the delegate to
  // resize to 64px.
  [bar_ resizeView:bookmarkBarView newHeight:30];
  EXPECT_EQ(64, [resizeDelegate_ height]);
  EXPECT_EQ(30, NSHeight([bookmarkBarView frame]));

  // Resize the bookmarkbar back to 0px.  Toolbar should be at 39px.
  [bar_ resizeView:bookmarkBarView newHeight:0];
  EXPECT_EQ(39, [resizeDelegate_ height]);
  EXPECT_EQ(0, NSHeight([bookmarkBarView frame]));

  // Resize the bookmarkbar to 5px.  Toolbar should stay at 39px.
  [resizeDelegate_ setHeight:-1];
  [bar_ resizeView:bookmarkBarView newHeight:5];
  EXPECT_EQ(39, [resizeDelegate_ height]);
  EXPECT_EQ(5, NSHeight([bookmarkBarView frame]));

  // Resize the bookmarkbar to 6px.  Toolbar should grow to 40px.
  [bar_ resizeView:bookmarkBarView newHeight:6];
  EXPECT_EQ(40, [resizeDelegate_ height]);
  EXPECT_EQ(6, NSHeight([bookmarkBarView frame]));
}

}  // namespace
