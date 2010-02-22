// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/browser_window_controller_private.h"

#include "base/mac_util.h"
#import "base/scoped_nsobject.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_theme_provider.h"
#import "chrome/browser/cocoa/browser_theme_provider_init.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/fast_resize_view.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#import "chrome/browser/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/cocoa/fullscreen_controller.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/pref_names.h"


namespace {

// Insets for the location bar, used when the full toolbar is hidden.
// TODO(viettrungluu): We can argue about the "correct" insetting; I like the
// following best, though arguably 0 inset is better/more correct.
const CGFloat kLocBarLeftRightInset = 1;
const CGFloat kLocBarTopInset = 0;
const CGFloat kLocBarBottomInset = 1;

// The amount by which the floating bar is offset downwards (to avoid the menu)
// in fullscreen mode.
const CGFloat kFullscreenVerticalBarOffset = 14;

}  // end namespace


@implementation BrowserWindowController(Private)

- (void)saveWindowPositionIfNeeded {
  if (browser_ != BrowserList::GetLastActive())
    return;

  if (!g_browser_process || !g_browser_process->local_state() ||
      !browser_->ShouldSaveWindowPlacement())
    return;

  [self saveWindowPositionToPrefs:g_browser_process->local_state()];
}

- (void)saveWindowPositionToPrefs:(PrefService*)prefs {
  // If we're in fullscreen mode, save the position of the regular window
  // instead.
  NSWindow* window = [self isFullscreen] ? savedRegularWindow_ : [self window];

  // Window positions are stored relative to the origin of the primary monitor.
  NSRect monitorFrame = [[[NSScreen screens] objectAtIndex:0] frame];
  NSScreen* windowScreen = [window screen];

  // |windowScreen| can be nil (for example, if the monitor arrangement was
  // changed while in fullscreen mode).  If we see a nil screen, return without
  // saving.
  // TODO(rohitrao): We should just not save anything for fullscreen windows.
  // http://crbug.com/36479.
  if (!windowScreen)
    return;

  // Start with the window's frame, which is in virtual coordinates.
  // Do some y twiddling to flip the coordinate system.
  gfx::Rect bounds(NSRectToCGRect([window frame]));
  bounds.set_y(monitorFrame.size.height - bounds.y() - bounds.height());

  // We also need to save the current work area, in flipped coordinates.
  gfx::Rect workArea(NSRectToCGRect([windowScreen visibleFrame]));
  workArea.set_y(monitorFrame.size.height - workArea.y() - workArea.height());

  DictionaryValue* windowPreferences = prefs->GetMutableDictionary(
      browser_->GetWindowPlacementKey().c_str());
  windowPreferences->SetInteger(L"left", bounds.x());
  windowPreferences->SetInteger(L"top", bounds.y());
  windowPreferences->SetInteger(L"right", bounds.right());
  windowPreferences->SetInteger(L"bottom", bounds.bottom());
  windowPreferences->SetBoolean(L"maximized", false);
  windowPreferences->SetBoolean(L"always_on_top", false);
  windowPreferences->SetInteger(L"work_area_left", workArea.x());
  windowPreferences->SetInteger(L"work_area_top", workArea.y());
  windowPreferences->SetInteger(L"work_area_right", workArea.right());
  windowPreferences->SetInteger(L"work_area_bottom", workArea.bottom());
}

- (NSRect)window:(NSWindow*)window
willPositionSheet:(NSWindow*)sheet
       usingRect:(NSRect)defaultSheetRect {
  // Position the sheet as follows:
  //  - If the bookmark bar is hidden or shown as a bubble (on the NTP when the
  //    bookmark bar is disabled), position the sheet immediately below the
  //    normal toolbar.
  //  - If the bookmark bar is shown (attached to the normal toolbar), position
  //    the sheet below the bookmark bar.
  //  - If the bookmark bar is currently animating, position the sheet according
  //    to where the bar will be when the animation ends.
  switch ([bookmarkBarController_ visualState]) {
    case bookmarks::kShowingState: {
      NSRect bookmarkBarFrame = [[bookmarkBarController_ view] frame];
      defaultSheetRect.origin.y = bookmarkBarFrame.origin.y;
      break;
    }
    case bookmarks::kHiddenState:
    case bookmarks::kDetachedState: {
      NSRect toolbarFrame = [[toolbarController_ view] frame];
      defaultSheetRect.origin.y = toolbarFrame.origin.y;
      break;
    }
    case bookmarks::kInvalidState:
    default:
      NOTREACHED();
  }
  return defaultSheetRect;
}

- (void)setTheme {
  ThemeProvider* theme_provider = browser_->profile()->GetThemeProvider();
  BrowserThemeProvider* browser_theme_provider =
     static_cast<BrowserThemeProvider*>(theme_provider);
  if (browser_theme_provider) {
    bool offtheRecord = browser_->profile()->IsOffTheRecord();
    GTMTheme* theme =
        [GTMTheme themeWithBrowserThemeProvider:browser_theme_provider
                                 isOffTheRecord:offtheRecord];
    theme_.reset([theme retain]);
  }
}

- (void)layoutSubviews {
  // With the exception of the tab strip, the subviews which we lay out are
  // subviews of the content view, so we mainly work in the content view's
  // coordinate system. Note, however, that the content view's coordinate system
  // and the window's base coordinate system should coincide.
  NSWindow* window = [self window];
  NSView* contentView = [window contentView];
  NSRect contentBounds = [contentView bounds];
  CGFloat minY = NSMinY(contentBounds);
  CGFloat width = NSWidth(contentBounds);

  // Suppress title drawing if necessary.
  if ([window respondsToSelector:@selector(setShouldHideTitle:)])
    [(id)window setShouldHideTitle:![self hasTitleBar]];

  BOOL isFullscreen = [self isFullscreen];
  CGFloat floatingBarHeight = [self floatingBarHeight];
  CGFloat yOffset = floor(
      isFullscreen ? (1 - floatingBarShownFraction_) * floatingBarHeight : 0);
  CGFloat maxY = NSMaxY(contentBounds) + yOffset;
  CGFloat startMaxY = maxY;

  if ([self hasTabStrip]) {
    // If we need to lay out the tab strip, replace |maxY| and |startMaxY| with
    // higher values, and then lay out the tab strip.
    startMaxY = maxY = NSHeight([window frame]) + yOffset;
    maxY = [self layoutTabStripAtMaxY:maxY width:width fullscreen:isFullscreen];
  }

  // Sanity-check |maxY|.
  DCHECK_GE(maxY, minY);
  DCHECK_LE(maxY, NSMaxY(contentBounds) + yOffset);

  // Place the toolbar at the top of the reserved area.
  maxY = [self layoutToolbarAtMaxY:maxY width:width];

  // If we're not displaying the bookmark bar below the infobar, then it goes
  // immediately below the toolbar.
  BOOL placeBookmarkBarBelowInfoBar = [self placeBookmarkBarBelowInfoBar];
  if (!placeBookmarkBarBelowInfoBar)
    maxY = [self layoutBookmarkBarAtMaxY:maxY width:width];

  // The floating bar backing view doesn't actually add any height.
  [self layoutFloatingBarBackingViewAtY:maxY
                                  width:width
                                 height:floatingBarHeight
                             fullscreen:isFullscreen];

  [fullscreenController_ overlayFrameChanged:[floatingBarBackingView_ frame]];

  // Place the find bar immediately below the toolbar/attached bookmark bar. In
  // fullscreen mode, it hangs off the top of the screen when the bar is hidden.
  [findBarCocoaController_ positionFindBarViewAtMaxY:maxY maxWidth:width];

  // If in fullscreen mode, reset |maxY| to top of screen, so that the floating
  // bar slides over the things which appear to be in the content area.
  if (isFullscreen)
    maxY = NSMaxY(contentBounds);

  // Also place the infobar container immediate below the toolbar, except in
  // fullscreen mode in which case it's at the top of the visual content area.
  maxY = [self layoutInfoBarAtMaxY:maxY width:width];

  // If the bookmark bar is detached, place it next in the visual content area.
  if (placeBookmarkBarBelowInfoBar)
    maxY = [self layoutBookmarkBarAtMaxY:maxY width:width];

  // Place the download shelf, if any, at the bottom of the view.
  minY = [self layoutDownloadShelfAtMinY:minY width:width];

  // Finally, the content area takes up all of the remaining space.
  [self layoutTabContentAreaAtMinY:minY maxY:maxY width:width];

  // Place the status bubble at the bottom of the content area.
  verticalOffsetForStatusBubble_ = minY;

  // Normally, we don't need to tell the toolbar whether or not to show the
  // divider, but things break down during animation.
  [toolbarController_
      setDividerOpacity:[bookmarkBarController_ toolbarDividerOpacity]];
}

- (CGFloat)floatingBarHeight {
  if (![self isFullscreen])
    return 0;

  CGFloat totalHeight = kFullscreenVerticalBarOffset;

  if ([self hasTabStrip])
    totalHeight += NSHeight([[self tabStripView] frame]);

  if ([self hasToolbar]) {
    totalHeight += NSHeight([[toolbarController_ view] frame]);
  } else if ([self hasLocationBar]) {
    totalHeight += NSHeight([[toolbarController_ view] frame]) +
        kLocBarTopInset + kLocBarBottomInset;
  }

  if (![self placeBookmarkBarBelowInfoBar])
    totalHeight += NSHeight([[bookmarkBarController_ view] frame]);

  return totalHeight;
}

- (CGFloat)layoutTabStripAtMaxY:(CGFloat)maxY
                          width:(CGFloat)width
                     fullscreen:(BOOL)fullscreen {
  // Nothing to do if no tab strip.
  if (![self hasTabStrip])
    return maxY;

  NSView* tabStripView = [self tabStripView];
  CGFloat tabStripHeight = NSHeight([tabStripView frame]);
  // In fullscreen mode, push the tab strip down so that the main menu (which
  // also slides down) doesn't run it over.
  if (fullscreen)
    maxY -= kFullscreenVerticalBarOffset;
  maxY -= tabStripHeight;
  [tabStripView setFrame:NSMakeRect(0, maxY, width, tabStripHeight)];

  // Set indentation.
  [tabStripController_ setIndentForControls:(fullscreen ? 0 :
      [[tabStripController_ class] defaultIndentForControls])];

  // TODO(viettrungluu): Seems kind of bad -- shouldn't |-layoutSubviews| do
  // this? Moreover, |-layoutTabs| will try to animate....
  [tabStripController_ layoutTabs];

  return maxY;
}

- (CGFloat)layoutToolbarAtMaxY:(CGFloat)maxY width:(CGFloat)width {
  NSView* toolbarView = [toolbarController_ view];
  NSRect toolbarFrame = [toolbarView frame];
  if ([self hasToolbar]) {
    // The toolbar is present in the window, so we make room for it.
    DCHECK(![toolbarView isHidden]);
    toolbarFrame.origin.x = 0;
    toolbarFrame.origin.y = maxY - NSHeight(toolbarFrame);
    toolbarFrame.size.width = width;
    maxY -= NSHeight(toolbarFrame);
  } else {
    if ([self hasLocationBar]) {
      // Location bar is present with no toolbar. Put a border of
      // |kLocBar...Inset| pixels around the location bar.
      // TODO(viettrungluu): This is moderately ridiculous. The toolbar should
      // really be aware of what its height should be (the way the toolbar
      // compression stuff is currently set up messes things up).
      DCHECK(![toolbarView isHidden]);
      toolbarFrame.origin.x = kLocBarLeftRightInset;
      toolbarFrame.origin.y = maxY - NSHeight(toolbarFrame) - kLocBarTopInset;
      toolbarFrame.size.width = width - 2 * kLocBarLeftRightInset;
      maxY -= kLocBarTopInset + NSHeight(toolbarFrame) + kLocBarBottomInset;
    } else {
      DCHECK([toolbarView isHidden]);
    }
  }
  [toolbarView setFrame:toolbarFrame];
  return maxY;
}

- (BOOL)placeBookmarkBarBelowInfoBar {
  // If we are currently displaying the NTP detached bookmark bar or animating
  // to/from it (from/to anything else), we display the bookmark bar below the
  // infobar.
  return [bookmarkBarController_ isInState:bookmarks::kDetachedState] ||
      [bookmarkBarController_ isAnimatingToState:bookmarks::kDetachedState] ||
      [bookmarkBarController_ isAnimatingFromState:bookmarks::kDetachedState];
}

- (CGFloat)layoutBookmarkBarAtMaxY:(CGFloat)maxY width:(CGFloat)width {
  NSView* bookmarkBarView = [bookmarkBarController_ view];
  NSRect bookmarkBarFrame = [bookmarkBarView frame];
  BOOL oldHidden = [bookmarkBarView isHidden];
  BOOL newHidden = ![self isBookmarkBarVisible];
  if (oldHidden != newHidden)
    [bookmarkBarView setHidden:newHidden];
  bookmarkBarFrame.origin.y = maxY - NSHeight(bookmarkBarFrame);
  bookmarkBarFrame.size.width = width;
  [bookmarkBarView setFrame:bookmarkBarFrame];
  maxY -= NSHeight(bookmarkBarFrame);

  // TODO(viettrungluu): Does this really belong here? Calling it shouldn't be
  // necessary in the non-NTP case.
  [bookmarkBarController_ layoutSubviews];

  return maxY;
}

- (void)layoutFloatingBarBackingViewAtY:(CGFloat)y
                                  width:(CGFloat)width
                                 height:(CGFloat)height
                             fullscreen:(BOOL)fullscreen {
  // Only display when in fullscreen mode.
  if (fullscreen) {
    DCHECK(floatingBarBackingView_.get());
    BOOL aboveBookmarkBar = [self placeBookmarkBarBelowInfoBar];

    // Insert it into the view hierarchy if necessary.
    if (![floatingBarBackingView_ superview] ||
        aboveBookmarkBar != floatingBarAboveBookmarkBar_) {
      NSView* contentView = [[self window] contentView];
      // z-order gets messed up unless we explicitly remove the floatingbar view
      // and re-add it.
      [floatingBarBackingView_ removeFromSuperview];
      [contentView addSubview:floatingBarBackingView_
                   positioned:(aboveBookmarkBar ?
                                   NSWindowAbove : NSWindowBelow)
                   relativeTo:[bookmarkBarController_ view]];
      floatingBarAboveBookmarkBar_ = aboveBookmarkBar;
    }

    // Set its frame.
    [floatingBarBackingView_ setFrame:NSMakeRect(0, y, width, height)];
  } else {
    // Okay to call even if |floatingBarBackingView_| is nil.
    if ([floatingBarBackingView_ superview])
      [floatingBarBackingView_ removeFromSuperview];
  }
}

- (CGFloat)layoutInfoBarAtMaxY:(CGFloat)maxY width:(CGFloat)width {
  NSView* infoBarView = [infoBarContainerController_ view];
  NSRect infoBarFrame = [infoBarView frame];
  infoBarFrame.origin.y = maxY - NSHeight(infoBarFrame);
  infoBarFrame.size.width = width;
  [infoBarView setFrame:infoBarFrame];
  maxY -= NSHeight(infoBarFrame);
  return maxY;
}

- (CGFloat)layoutDownloadShelfAtMinY:(CGFloat)minY width:(CGFloat)width {
  if (downloadShelfController_.get()) {
    NSView* downloadView = [downloadShelfController_ view];
    NSRect downloadFrame = [downloadView frame];
    downloadFrame.origin.y = minY;
    downloadFrame.size.width = width;
    [downloadView setFrame:downloadFrame];
    minY += NSHeight(downloadFrame);
  }
  return minY;
}

- (void)layoutTabContentAreaAtMinY:(CGFloat)minY
                              maxY:(CGFloat)maxY
                             width:(CGFloat)width {
  NSView* tabContentView = [self tabContentArea];
  NSRect tabContentFrame = [tabContentView frame];

  bool contentShifted = NSMaxY(tabContentFrame) != maxY;

  tabContentFrame.origin.y = minY;
  tabContentFrame.size.height = maxY - minY;
  tabContentFrame.size.width = width;
  [tabContentView setFrame:tabContentFrame];

  // If the relayout shifts the content area up or down, let the renderer know.
  if (contentShifted) {
    if (TabContents* contents = browser_->GetSelectedTabContents()) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->WindowFrameChanged();
    }
  }
}

- (BOOL)shouldShowBookmarkBar {
  DCHECK(browser_.get());
  return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      YES : NO;
}

- (BOOL)shouldShowDetachedBookmarkBar {
  DCHECK(browser_.get());
  TabContents* contents = browser_->GetSelectedTabContents();
  return (contents && contents->ShouldShowBookmarkBar()) ? YES : NO;
}

- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression {
  CGFloat newHeight =
      [toolbarController_ desiredHeightForCompression:compression];
  NSRect toolbarFrame = [[toolbarController_ view] frame];
  CGFloat deltaH = newHeight - toolbarFrame.size.height;

  if (deltaH == 0)
    return;

  toolbarFrame.size.height = newHeight;
  NSRect bookmarkFrame = [[bookmarkBarController_ view] frame];
  bookmarkFrame.size.height = bookmarkFrame.size.height - deltaH;
  [[toolbarController_ view] setFrame:toolbarFrame];
  [[bookmarkBarController_ view] setFrame:bookmarkFrame];
  [self layoutSubviews];
}

// TODO(rohitrao): This function has shrunk into uselessness, and
// |-setFullscreen:| has grown rather large.  Find a good way to break up
// |-setFullscreen:| into smaller pieces.  http://crbug.com/36449
- (void)adjustUIForFullscreen:(BOOL)fullscreen {
  // Create the floating bar backing view if necessary.
  if (fullscreen && !floatingBarBackingView_.get()) {
    floatingBarBackingView_.reset(
        [[FloatingBarBackingView alloc] initWithFrame:NSZeroRect]);
  }
}

- (void)enableBarVisibilityUpdates {
  // Early escape if there's nothing to do.
  if (barVisibilityUpdatesEnabled_)
    return;

  barVisibilityUpdatesEnabled_ = YES;

  if ([barVisibilityLocks_ count])
    [fullscreenController_ ensureOverlayShownWithAnimation:NO delay:NO];
  else
    [fullscreenController_ ensureOverlayHiddenWithAnimation:NO delay:NO];
}

- (void)disableBarVisibilityUpdates {
  // Early escape if there's nothing to do.
  if (!barVisibilityUpdatesEnabled_)
    return;

  barVisibilityUpdatesEnabled_ = NO;
  [fullscreenController_ cancelAnimationAndTimers];
}

@end  // @implementation BrowserWindowController(Private)
