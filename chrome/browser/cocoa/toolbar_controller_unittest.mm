// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// An NSView that fakes out hitTest:.
@interface HitView : NSView {
  id hitTestReturn_;
}
@end

@implementation HitView

- (void)setHitTestReturn:(id)rtn {
  hitTestReturn_ = rtn;
}

- (NSView *)hitTest:(NSPoint)aPoint {
  return hitTestReturn_;
}

@end


namespace {

class ToolbarControllerTest : public CocoaTest {
 public:

  // Indexes that match the ordering returned by the private ToolbarController
  // |-toolbarViews| method.
  enum {
    kBackIndex, kForwardIndex, kReloadIndex, kHomeIndex, kStarIndex, kGoIndex,
    kPageIndex, kWrenchIndex, kLocationIndex,
    kBrowserActionContainerViewIndex
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
                                         browser:browser
                                  resizeDelegate:resizeDelegate_.get()]);
    EXPECT_TRUE([bar_ view]);
    NSView* parent = [test_window() contentView];
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
    EXPECT_EQ(updater->IsCommandEnabled(IDC_BOOKMARK_PAGE),
              [[views objectAtIndex:kStarIndex] isEnabled] ? true : false);
  }

  BrowserTestHelper helper_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  scoped_nsobject<ToolbarController> bar_;
};

TEST_VIEW(ToolbarControllerTest, [bar_ view])

// Test the initial state that everything is sync'd up
TEST_F(ToolbarControllerTest, InitialState) {
  CommandUpdater* updater = helper_.browser()->command_updater();
  CompareState(updater, [bar_ toolbarViews]);
}

// Make sure a "titlebar only" toolbar with location bar works.
TEST_F(ToolbarControllerTest, TitlebarOnly) {
  NSView* view = [bar_ view];

  [bar_ setHasToolbar:NO hasLocationBar:YES];
  EXPECT_NE(view, [bar_ view]);

  // Simulate a popup going fullscreen and back.
  NSView* superview = [view superview];
  // TODO(jrg): find a way to add an [NSAutoreleasePool drain] in
  // here.  I don't have access to the current
  // scoped_nsautorelease_pool to do it properly :-(
  [view removeFromSuperview];
  [superview addSubview:view];

  [bar_ setHasToolbar:YES hasLocationBar:YES];
  EXPECT_EQ(view, [bar_ view]);

  // Leave it off to make sure that's fine
  [bar_ setHasToolbar:NO hasLocationBar:YES];
}

// TODO(viettrungluu): make a version of above without location bar.

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

// Focus the location bar and make sure that it's the first responder.
TEST_F(ToolbarControllerTest, FocusLocation) {
  NSWindow* window = test_window();
  [window makeFirstResponder:[window contentView]];
  EXPECT_EQ([window firstResponder], [window contentView]);
  [bar_ focusLocationBar:YES];
  EXPECT_NE([window firstResponder], [window contentView]);
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  EXPECT_EQ([window firstResponder], [(id)locationBar currentEditor]);
}

TEST_F(ToolbarControllerTest, LoadingState) {
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

// Ensure that we don't toggle the buttons when we have a strip marked as not
// having the full toolbar. Also ensure that the location bar doesn't change
// size.
TEST_F(ToolbarControllerTest, DontToggleWhenNoToolbar) {
  [bar_ setHasToolbar:NO hasLocationBar:YES];
  NSView* homeButton = [[bar_ toolbarViews] objectAtIndex:kHomeIndex];
  NSView* pageButton = [[bar_ toolbarViews] objectAtIndex:kPageIndex];
  NSView* wrenchButton = [[bar_ toolbarViews] objectAtIndex:kWrenchIndex];
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  NSRect locationBarFrame = [locationBar frame];
  EXPECT_EQ([homeButton isHidden], YES);
  EXPECT_EQ([pageButton isHidden], YES);
  EXPECT_EQ([wrenchButton isHidden], YES);
  [bar_ showOptionalHomeButton];
  EXPECT_EQ([homeButton isHidden], YES);
  NSRect newLocationBarFrame = [locationBar frame];
  EXPECT_TRUE(NSEqualRects(locationBarFrame, newLocationBarFrame));
  [bar_ showOptionalPageWrenchButtons];
  EXPECT_EQ([pageButton isHidden], YES);
  EXPECT_EQ([wrenchButton isHidden], YES);
  newLocationBarFrame = [locationBar frame];
  EXPECT_TRUE(NSEqualRects(locationBarFrame, newLocationBarFrame));
}

TEST_F(ToolbarControllerTest, StarButtonInWindowCoordinates) {
  NSRect star = [bar_ starButtonInWindowCoordinates];
  NSRect all = [[[bar_ view] window] frame];

  // Make sure the star is completely inside the window rect
  EXPECT_TRUE(NSContainsRect(all, star));
}

TEST_F(ToolbarControllerTest, BubblePosition) {
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];

  // The window frame (in window base coordinates).
  NSRect all = [[[bar_ view] window] frame];
  // The frame of the location bar in window base coordinates.
  NSRect locationFrame =
      [locationBar convertRect:[locationBar bounds] toView:nil];
  // The frame of the location stack in window base coordinates.  The horizontal
  // coordinates here are used for the omnibox dropdown.
  gfx::Rect locationStackFrame = [bar_ locationStackBounds];

  // Make sure the location stack starts to the left of and ends to the right of
  // the location bar.
  EXPECT_LT(locationStackFrame.x(), NSMinX(locationFrame));
  EXPECT_GT(locationStackFrame.right(), NSMaxX(locationFrame));
}

TEST_F(ToolbarControllerTest, HoverButtonForEvent) {
  scoped_nsobject<HitView> view([[HitView alloc]
                                  initWithFrame:NSMakeRect(0,0,100,100)]);
  [bar_ setView:view];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:NSMakePoint(10,10)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0.0];

  // NOT a match.
  [view setHitTestReturn:bar_.get()];
  EXPECT_FALSE([bar_ hoverButtonForEvent:event]);

  // Not yet...
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [view setHitTestReturn:button];
  EXPECT_FALSE([bar_ hoverButtonForEvent:event]);

  // Now!
  scoped_nsobject<GradientButtonCell> cell([[GradientButtonCell alloc] init]);
  [button setCell:cell.get()];
  EXPECT_TRUE([bar_ hoverButtonForEvent:nil]);
}

}  // namespace
