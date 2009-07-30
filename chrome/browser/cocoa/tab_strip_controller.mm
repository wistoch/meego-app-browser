// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_cell.h"
#import "chrome/browser/cocoa/tab_contents_controller.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/throbber_view.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"

NSString* const kTabStripNumberOfTabsChanged = @"kTabStripNumberOfTabsChanged";

// A value to indicate tab layout should use the full available width of the
// view.
static const float kUseFullAvailableWidth = -1.0;

// A simple view class that prevents the windowserver from dragging the
// area behind tabs. Sometimes core animation confuses it.
@interface TabStripControllerDragBlockingView : NSView
@end
@implementation TabStripControllerDragBlockingView
- (BOOL)mouseDownCanMoveWindow {return NO;}
- (void)drawRect:(NSRect)rect {}
@end

@interface TabStripController(Private)
- (void)installTrackingArea;
- (BOOL)useFullWidthForLayout;
@end

@implementation TabStripController

- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
             model:(TabStripModel*)model {
  DCHECK(view && switchView && model);
  if ((self = [super init])) {
    tabView_ = view;
    switchView_ = switchView;
    tabModel_ = model;
    bridge_.reset(new TabStripModelObserverBridge(tabModel_, self));
    tabContentsArray_.reset([[NSMutableArray alloc] init]);
    tabArray_.reset([[NSMutableArray alloc] init]);
    // Take the only child view present in the nib as the new tab button. For
    // some reason, if the view is present in the nib apriori, it draws
    // correctly. If we create it in code and add it to the tab view, it draws
    // with all sorts of crazy artifacts.
    newTabButton_ = [[tabView_ subviews] objectAtIndex:0];
    DCHECK([newTabButton_ isKindOfClass:[NSButton class]]);
    [newTabButton_ setTarget:nil];
    [newTabButton_ setAction:@selector(commandDispatch:)];
    [newTabButton_ setTag:IDC_NEW_TAB];
    targetFrames_.reset([[NSMutableDictionary alloc] init]);
    [tabView_ setWantsLayer:YES];
    dragBlockingView_.reset([[TabStripControllerDragBlockingView alloc]
                              initWithFrame:NSZeroRect]);
    [view addSubview:dragBlockingView_];
    newTabTargetFrame_ = NSMakeRect(0, 0, 0, 0);
    availableResizeWidth_ = kUseFullAvailableWidth;

    // Watch for notifications that the tab strip view has changed size so
    // we can tell it to layout for the new size.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(tabViewFrameChanged:)
               name:NSViewFrameDidChangeNotification
             object:tabView_];
  }
  return self;
}

- (void)dealloc {
  if (closeTabTrackingArea_.get())
    [tabView_ removeTrackingArea:closeTabTrackingArea_.get()];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

+ (CGFloat)defaultTabHeight {
  return 24.0;
}

// Finds the associated TabContentsController at the given |index| and swaps
// out the sole child of the contentArea to display its contents.
- (void)swapInTabAtIndex:(NSInteger)index {
  TabContentsController* controller = [tabContentsArray_ objectAtIndex:index];

  // Resize the new view to fit the window. Calling |view| may lazily
  // instantiate the TabContentsController from the nib. Until we call
  // |-ensureContentsVisible|, the controller doesn't install the RWHVMac into
  // the view hierarchy. This is in order to avoid sending the renderer a
  // spurious default size loaded from the nib during the call to |-view|.
  NSView* newView = [controller view];
  NSRect frame = [switchView_ bounds];
  [newView setFrame:frame];
  [controller ensureContentsVisible];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of |switchView_| because we're the one who put it there. There
  // may not be any children in the case of a tab that's been closed, in
  // which case there's no swapping going on.
  NSArray* subviews = [switchView_ subviews];
  if ([subviews count]) {
    NSView* oldView = [subviews objectAtIndex:0];
    [switchView_ replaceSubview:oldView with:newView];
  } else {
    [switchView_ addSubview:newView];
  }
}

// Create a new tab view and set its cell correctly so it draws the way we want
// it to. It will be sized and positioned by |-layoutTabs| so there's no need to
// set the frame here. This also creates the view as hidden, it will be
// shown during layout.
- (TabController*)newTab {
  TabController* controller = [[[TabController alloc] init] autorelease];
  [controller setTarget:self];
  [controller setAction:@selector(selectTab:)];
  [[controller view] setHidden:YES];

  return controller;
}

// Returns the number of tabs in the tab strip. This is just the number
// of TabControllers we know about as there's a 1-to-1 mapping from these
// controllers to a tab.
- (NSInteger)numberOfTabViews {
  return [tabArray_ count];
}

// Returns the index of the subview |view|. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view {
  NSInteger index = 0;
  for (TabController* current in tabArray_.get()) {
    if ([current view] == view)
      return index;
    ++index;
  }
  return -1;
}

// Returns the view at the given index, using the array of TabControllers to
// get the associated view. Returns nil if out of range.
- (NSView*)viewAtIndex:(NSUInteger)index {
  if (index >= [tabArray_ count])
    return NULL;
  return [[tabArray_ objectAtIndex:index] view];
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  DCHECK([sender isKindOfClass:[NSView class]]);
  int index = [self indexForTabView:sender];
  if (index >= 0 && tabModel_->ContainsIndex(index))
    tabModel_->SelectTabContentsAt(index, true);
}

// Called when the user closes a tab. Asks the model to close the tab. |sender|
// is the TabView that is potentially going away.
- (void)closeTab:(id)sender {
  DCHECK([sender isKindOfClass:[NSView class]]);
  int index = [self indexForTabView:sender];
  if (tabModel_->ContainsIndex(index)) {
    TabContents* contents = tabModel_->GetTabContentsAt(index);
    if (contents)
      UserMetrics::RecordAction(L"CloseTab_Mouse", contents->profile());
    if ([self numberOfTabViews] > 1) {
      // Limit the width available for laying out tabs so that tabs are not
      // resized until a later time (when the mouse leaves the tab strip).
      // TODO(pinkerton): re-visit when handling tab overflow.
      NSView* penultimateTab = [self viewAtIndex:[tabArray_ count] - 2];
      availableResizeWidth_ = NSMaxX([penultimateTab frame]);
      [self installTrackingArea];
      tabModel_->CloseTabContentsAt(index);
    } else {
      // Use the standard window close if this is the last tab
      // this prevents the tab from being removed from the model until after
      // the window dissapears
      [[tabView_ window] performClose:nil];
    }
  }
}

// Dispatch context menu commands for the given tab controller.
- (void)commandDispatch:(TabStripModel::ContextMenuCommand)command
          forController:(TabController*)controller {
  int index = [self indexForTabView:[controller view]];
  tabModel_->ExecuteContextMenuCommand(index, command);
}

// Returns YES if the specificed command should be enabled for the given
// controller.
- (BOOL)isCommandEnabled:(TabStripModel::ContextMenuCommand)command
           forController:(TabController*)controller {
  int index = [self indexForTabView:[controller view]];
  return tabModel_->IsContextMenuCommandEnabled(index, command) ? YES : NO;
}

- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                  yStretchiness:(CGFloat)yStretchiness {
  placeholderTab_ = tab;
  placeholderFrame_ = frame;
  placeholderStretchiness_ = yStretchiness;
  [self layoutTabs];
}

- (void)showNewTabButton:(BOOL)show {
  forceNewTabButtonHidden_ = show ? NO : YES;
  if (forceNewTabButtonHidden_)
    [newTabButton_ setHidden:YES];
}

// Lay out all tabs in the order of their TabContentsControllers, which matches
// the ordering in the TabStripModel. This call isn't that expensive, though
// it is O(n) in the number of tabs. Tabs will animate to their new position
// if the window is visible and |animate| is YES.
// TODO(pinkerton): Handle drag placeholders via proxy objects, perhaps a
// subclass of TabContentsController with everything stubbed out or by
// abstracting a base class interface.
// TODO(pinkerton): Note this doesn't do too well when the number of min-sized
// tabs would cause an overflow.
- (void)layoutTabsWithAnimation:(BOOL)animate {
  const float kIndentLeavingSpaceForControls = 64.0;
  const float kTabOverlap = 20.0;
  const float kNewTabButtonOffset = 8.0;
  const float kMaxTabWidth = [TabController maxTabWidth];
  const float kMinTabWidth = [TabController minTabWidth];
  const float kMinSelectedTabWidth = [TabController minSelectedTabWidth];

  NSRect enclosingRect = NSZeroRect;
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:0.2];

  // Compute the base width of tabs given how much room we're allowed. We
  // may not be able to use the entire width if the user is quickly closing
  // tabs.
  float availableWidth = 0;
  if ([self useFullWidthForLayout]) {
    availableWidth = NSWidth([tabView_ frame]);
    availableWidth -= NSWidth([newTabButton_ frame]) + kNewTabButtonOffset;
  } else {
    availableWidth = availableResizeWidth_;
  }
  availableWidth -= kIndentLeavingSpaceForControls;

  // Add back in the amount we "get back" from the tabs overlapping.
  availableWidth += ([tabContentsArray_ count] - 1) * kTabOverlap;
  const float baseTabWidth =
      MAX(MIN(availableWidth / [tabContentsArray_ count],
              kMaxTabWidth),
          kMinTabWidth);

  CGFloat minX = NSMinX(placeholderFrame_);
  BOOL visible = [[tabView_ window] isVisible];

  float offset = kIndentLeavingSpaceForControls;
  NSUInteger i = 0;
  NSInteger gap = -1;
  NSView* previousTab = nil;
  for (TabController* tab in tabArray_.get()) {
    BOOL isPlaceholder = [[tab view] isEqual:placeholderTab_];
    NSRect tabFrame = [[tab view] frame];
    tabFrame.size.height = [[self class] defaultTabHeight];
    tabFrame.origin.y = 0;
    tabFrame.origin.x = offset;

    // If the tab is hidden, we consider it a new tab. We make it visible
    // and animate it in.
    BOOL newTab = [[tab view] isHidden];
    if (newTab) {
      [[tab view] setHidden:NO];
    }

    if (isPlaceholder) {
      // Move the current tab to the correct location instantly.
      // We need a duration or else it doesn't cancel an inflight animation.
      [NSAnimationContext beginGrouping];
      [[NSAnimationContext currentContext] setDuration:0.000001];
      tabFrame.origin.x = placeholderFrame_.origin.x;
      // TODO(alcor): reenable this
      //tabFrame.size.height += 10.0 * placeholderStretchiness_;
      id target = animate ? [[tab view] animator] : [tab view];
      [target setFrame:tabFrame];
      [NSAnimationContext endGrouping];

      // Store the frame by identifier to aviod redundant calls to animator.
      NSValue *identifier = [NSValue valueWithPointer:[tab view]];
      [targetFrames_ setObject:[NSValue valueWithRect:tabFrame]
                        forKey:identifier];
      continue;
    } else {
      // If our left edge is to the left of the placeholder's left, but our mid
      // is to the right of it we should slide over to make space for it.
      if (placeholderTab_ && gap < 0 && NSMidX(tabFrame) > minX) {
        gap = i;
        offset += NSWidth(tabFrame);
        offset -= kTabOverlap;
        tabFrame.origin.x = offset;
      }

      // Animate the tab in by putting it below the horizon.
      if (newTab && visible && animate) {
        [[tab view] setFrame:NSOffsetRect(tabFrame, 0, -NSHeight(tabFrame))];
      }

      // Set the width. Selected tabs are slightly wider when things get
      // really small and thus we enforce a different minimum width.
      tabFrame.size.width =
          [tab selected] ? MAX(baseTabWidth, kMinSelectedTabWidth) :
                           baseTabWidth;

      // Check the frame by identifier to avoid redundant calls to animator.
      id frameTarget = visible && animate ? [[tab view] animator] : [tab view];
      NSValue *identifier = [NSValue valueWithPointer:[tab view]];
      NSValue *oldTargetValue = [targetFrames_ objectForKey:identifier];
      if (!oldTargetValue ||
          !NSEqualRects([oldTargetValue rectValue], tabFrame)) {
        [frameTarget setFrame:tabFrame];
        [targetFrames_ setObject:[NSValue valueWithRect:tabFrame]
                          forKey:identifier];
      }
      enclosingRect = NSUnionRect(tabFrame, enclosingRect);
    }

#if 0
    // Ensure the current tab is "below" the tab before it in z-order so that
    // all the tab overlaps are consistent. The selected tab is always the
    // frontmost, but it's already been made frontmost when the tab was selected
    // so we don't need to do anything about it here. It will get put back into
    // place when another tab is selected.
    // TODO(pinkerton): this doesn't seem to work in the case where a tab
    // is opened between existing tabs. Disabling.
    if (![tab selected]) {
      [tabView_ addSubview:[tab view]
                positioned:NSWindowBelow
                relativeTo:previousTab];
    }
#endif
    previousTab = [tab view];

    offset += NSWidth(tabFrame);
    offset -= kTabOverlap;
    i++;
  }

  // Hide the new tab button if we're explicitly told to. It may already
  // be hidden, doing it again doesn't hurt.
  if (forceNewTabButtonHidden_) {
    [newTabButton_ setHidden:YES];
  } else {
    NSRect newTabNewFrame = [newTabButton_ frame];
    if ([self useFullWidthForLayout])
      newTabNewFrame.origin =
          NSMakePoint(MIN(availableWidth, offset + kNewTabButtonOffset), 0);
    else
      newTabNewFrame.origin = NSMakePoint(offset + kNewTabButtonOffset, 0);
    newTabNewFrame.origin.x = MAX(newTabNewFrame.origin.x,
                                  NSMaxX(placeholderFrame_));
    if (i > 0 && [newTabButton_ isHidden]) {
      id target = animate ? [newTabButton_ animator] : newTabButton_;
      [target setHidden:NO];
    }

    if (!NSEqualRects(newTabTargetFrame_, newTabNewFrame)) {
      [newTabButton_ setFrame:newTabNewFrame];
      newTabTargetFrame_ = newTabNewFrame;
      // Move the new tab button into place.
    }
  }

  [NSAnimationContext endGrouping];
  [dragBlockingView_ setFrame:enclosingRect];
}

// When we're told to layout from the public API we always want to animate.
- (void)layoutTabs {
  [self layoutTabsWithAnimation:YES];
}

// Handles setting the title of the tab based on the given |contents|. Uses
// a canned string if |contents| is NULL.
- (void)setTabTitle:(NSViewController*)tab withContents:(TabContents*)contents {
  NSString* titleString = nil;
  if (contents)
    titleString = base::SysUTF16ToNSString(contents->GetTitle());
  if (![titleString length]) {
    titleString =
      base::SysWideToNSString(
          l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
  }
  [tab setTitle:titleString];
}

// Called when a notification is received from the model to insert a new tab
// at |index|.
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground {
  DCHECK(contents);
  DCHECK(index == TabStripModel::kNoTab || tabModel_->ContainsIndex(index));

  // TODO(pinkerton): handle tab dragging in here

  // Make a new tab. Load the contents of this tab from the nib and associate
  // the new controller with |contents| so it can be looked up later.
  TabContentsController* contentsController =
      [[[TabContentsController alloc] initWithNibName:@"TabContents"
                                             contents:contents]
          autorelease];
  [tabContentsArray_ insertObject:contentsController atIndex:index];

  // Make a new tab and add it to the strip. Keep track of its controller.
  TabController* newController = [self newTab];
  [tabArray_ insertObject:newController atIndex:index];
  NSView* newView = [newController view];

  // Set the originating frame to just below the strip so that it animates
  // upwards as it's being initially layed out. Oddly, this works while doing
  // something similar in |-layoutTabs| confuses the window server.
  // TODO(pinkerton): I'm not happy with this animiation either, but it's
  // a little better that just sliding over (maybe?).
  [newView setFrame:NSOffsetRect([newView frame],
                                 0, -[[self class] defaultTabHeight])];

  [tabView_ addSubview:newView
            positioned:inForeground ? NSWindowAbove : NSWindowBelow
            relativeTo:nil];

  [self setTabTitle:newController withContents:contents];

  // If a tab is being inserted, we can again use the entire tab strip width
  // for layout.
  availableResizeWidth_ = kUseFullAvailableWidth;

  // We don't need to call |-layoutTabs| if the tab will be in the foreground
  // because it will get called when the new tab is selected by the tab model.
  if (!inForeground) {
    [self layoutTabs];
  }

  // Send a broadcast that the number of tabs have changed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabStripNumberOfTabsChanged
                    object:self];
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  // De-select all other tabs and select the new tab.
  int i = 0;
  for (TabController* current in tabArray_.get()) {
    [current setSelected:(i == index) ? YES : NO];
    ++i;
  }

  // Make this the top-most tab in the strips's z order.
  NSView* selectedTab = [self viewAtIndex:index];
  [tabView_ addSubview:selectedTab positioned:NSWindowAbove relativeTo:nil];

  // Tell the new tab contents it is about to become the selected tab. Here it
  // can do things like make sure the toolbar is up to date.
  TabContentsController* newController =
      [tabContentsArray_ objectAtIndex:index];
  [newController willBecomeSelectedTab];

  // Relayout for new tabs and to let the selected tab grow to be larger in
  // size than surrounding tabs if the user has many.
  [self layoutTabs];

  if (oldContents) {
    oldContents->view()->StoreFocus();
    oldContents->WasHidden();
  }

  // Swap in the contents for the new tab
  [self swapInTabAtIndex:index];

  if (newContents) {
    newContents->DidBecomeSelected();
    newContents->view()->RestoreFocus();
  }
}

// Called when a notification is received from the model that the given tab
// has gone away. Remove all knowledge about this tab and it's associated
// controller and remove the view from the strip.
- (void)tabDetachedWithContents:(TabContents*)contents
                        atIndex:(NSInteger)index {
  // Release the tab contents controller so those views get destroyed. This
  // will remove all the tab content Cocoa views from the hierarchy. A
  // subsequent "select tab" notification will follow from the model. To
  // tell us what to swap in in its absence.
  [tabContentsArray_ removeObjectAtIndex:index];

  // Remove the |index|th view from the tab strip
  NSView* tab = [self viewAtIndex:index];
  [tab removeFromSuperview];

  NSValue *identifier = [NSValue valueWithPointer:tab];
  [targetFrames_ removeObjectForKey:identifier];

  // Once we're totally done with the tab, delete its controller
  [tabArray_ removeObjectAtIndex:index];

  // Send a broadcast that the number of tabs have changed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabStripNumberOfTabsChanged
                    object:self];

  [self layoutTabs];
}

// A helper routine for creating an NSImageView to hold the fav icon for
// |contents|.
- (NSImageView*)favIconImageViewForContents:(TabContents*)contents {
  NSRect iconFrame = NSMakeRect(0, 0, 16, 16);
  NSImageView* view = [[[NSImageView alloc] initWithFrame:iconFrame]
                          autorelease];

  NSImage* image = nil;

  NavigationEntry* navEntry = contents->controller().GetLastCommittedEntry();
  if (navEntry != NULL) {
    NavigationEntry::FaviconStatus favIcon = navEntry->favicon();
    const SkBitmap& bitmap = favIcon.bitmap();
    if (favIcon.is_valid() && !bitmap.isNull())
      image = gfx::SkBitmapToNSImage(bitmap);
  }

  // Either we don't have a valid favicon or there was some issue converting it
  // from an SkBitmap. Either way, just show the default.
  if (!image) {
    NSBundle* bundle = mac_util::MainAppBundle();
    image = [[NSImage alloc] initByReferencingFile:
             [bundle pathForResource:@"nav" ofType:@"pdf"]];
    [image autorelease];
  }

  [view setImage:image];
  return view;
}

// Called when a notification is received from the model that the given tab
// has been updated. |loading| will be YES when we only want to update the
// throbber state, not anything else about the (partially) loading tab.
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index
                   loadingOnly:(BOOL)loading {
  if (!loading)
    [self setTabTitle:[tabArray_ objectAtIndex:index] withContents:contents];

  // Update the current loading state, replacing the icon with a throbber, or
  // vice versa. This will get called repeatedly with the same state during a
  // load, so we need to make sure we're not creating the throbber view over and
  // over. However, when the page is done, every state change is important.
  if (contents) {
    static NSImage* throbberWaitingImage =
        [nsimage_cache::ImageNamed(@"throbber_waiting.png") retain];
    static NSImage* throbberLoadingImage =
        [nsimage_cache::ImageNamed(@"throbber.png") retain];
    static NSImage* sadFaviconImage =
        [nsimage_cache::ImageNamed(@"sadfavicon.png") retain];

    TabController* tabController = [tabArray_ objectAtIndex:index];

    TabLoadingState oldState = [tabController loadingState];

    TabLoadingState newState = kTabDone;
    NSImage* throbberImage = nil;
    if (contents->waiting_for_response()) {
      newState = kTabWaiting;
      throbberImage = throbberWaitingImage;
    } else if (contents->is_loading()) {
      newState = kTabLoading;
      throbberImage = throbberLoadingImage;
    } else if (contents->is_crashed()) {
      newState = kTabCrashed;
    }

    if (oldState != newState || newState == kTabDone) {
      NSView* iconView = nil;
      if (newState == kTabDone) {
        iconView = [self favIconImageViewForContents:contents];
      } else if (newState == kTabCrashed) {
        NSImage* oldImage = [[self favIconImageViewForContents:contents] image];
        NSRect frame = NSMakeRect(0, 0, 16, 16);
        iconView = [ThrobberView toastThrobberViewWithFrame:frame
                                                beforeImage:oldImage
                                                 afterImage:sadFaviconImage];
      } else {
        NSRect frame = NSMakeRect(0, 0, 16, 16);
        iconView = [ThrobberView filmstripThrobberViewWithFrame:frame
                                                          image:throbberImage];
      }

      [tabController setLoadingState:newState];
      [tabController setIconView:iconView];
    }
  }

  TabContentsController* updatedController =
      [tabContentsArray_ objectAtIndex:index];
  [updatedController tabDidChange:contents];
}

// Called when a tab is moved (usually by drag&drop). Keep our parallel arrays
// in sync with the tab strip model.
- (void)tabMovedWithContents:(TabContents*)contents
                    fromIndex:(NSInteger)from
                      toIndex:(NSInteger)to {
  scoped_nsobject<TabContentsController> movedController(
      [[tabContentsArray_ objectAtIndex:from] retain]);
  [tabContentsArray_ removeObjectAtIndex:from];
  [tabContentsArray_ insertObject:movedController.get() atIndex:to];
  scoped_nsobject<TabView> movedView(
      [[tabArray_ objectAtIndex:from] retain]);
  [tabArray_ removeObjectAtIndex:from];
  [tabArray_ insertObject:movedView.get() atIndex:to];

  [self layoutTabs];
}

- (void)setFrameOfSelectedTab:(NSRect)frame {
  NSView *view = [self selectedTabView];
  NSValue *identifier = [NSValue valueWithPointer:view];
  [targetFrames_ setObject:[NSValue valueWithRect:frame]
                    forKey:identifier];
  [view setFrame:frame];
}

- (NSView *)selectedTabView {
  int selectedIndex = tabModel_->selected_index();
  return [self viewAtIndex:selectedIndex];
}

// Find the index based on the x coordinate of the placeholder. If there is
// no placeholder, this returns the end of the tab strip.
- (int)indexOfPlaceholder {
  double placeholderX = placeholderFrame_.origin.x;
  int index = 0;
  int location = 0;
  const int count = tabModel_->count();
  while (index < count) {
    NSView* curr = [self viewAtIndex:index];
    // The placeholder tab works by changing the frame of the tab being dragged
    // to be the bounds of the placeholder, so we need to skip it while we're
    // iterating, otherwise we'll end up off by one.  Note This only effects
    // dragging to the right, not to the left.
    if (curr == placeholderTab_) {
      index++;
      continue;
    }
    if (placeholderX <= NSMinX([curr frame]))
      break;
    index++;
    location++;
  }
  return location;
}

// Move the given tab at index |from| in this window to the location of the
// current placeholder.
- (void)moveTabFromIndex:(NSInteger)from {
  int toIndex = [self indexOfPlaceholder];
  tabModel_->MoveTabContentsAt(from, toIndex, true);
}

// Drop a given TabContents at the location of the current placeholder. If there
// is no placeholder, it will go at the end. Used when dragging from another
// window when we don't have access to the TabContents as part of our strip.
- (void)dropTabContents:(TabContents*)contents {
  int index = [self indexOfPlaceholder];

  // Insert it into this tab strip. We want it in the foreground and to not
  // inherit the current tab's group.
  tabModel_->InsertTabContentsAt(index, contents, true, false);
}

- (void)userChangedTheme {
  for (TabController* tab in tabArray_.get()) {
    [[tab view] setNeedsDisplay:YES];
  }
}

// Called when the tab strip view changes size. As we only registered for
// changes on our view, we know it's only for our view. Layout w/out
// animations since they are blocked by the resize nested runloop. We need
// the views to adjust immediately.
- (void)tabViewFrameChanged:(NSNotification*)info {
  [self layoutTabsWithAnimation:NO];
}

- (BOOL)useFullWidthForLayout {
  return availableResizeWidth_ == kUseFullAvailableWidth;
}

// Call to install a tracking area that reports mouseEnter/Exit messages so
// we can track when the mouse leaves the tab view after closing a tab with
// the mouse. Don't install another tracking rect if one is already there.
- (void)installTrackingArea {
  if (closeTabTrackingArea_.get())
    return;
  // Note that we pass |NSTrackingInVisibleRect| so the rect is actually
  // ignored.
  closeTabTrackingArea_.reset([[NSTrackingArea alloc]
      initWithRect:[tabView_ bounds]
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways |
                    NSTrackingInVisibleRect
             owner:self
          userInfo:nil]);
  [tabView_ addTrackingArea:closeTabTrackingArea_.get()];
}

- (void)mouseEntered:(NSEvent*)event {
  // Do nothing.
}

// Called when the tracking area is in effect which means we're tracking to
// see if the user leaves the tab strip with their mouse. When they do,
// reset layout to use all available width.
- (void)mouseExited:(NSEvent*)event {
  [tabView_ removeTrackingArea:closeTabTrackingArea_.get()];
  closeTabTrackingArea_.reset(nil);
  availableResizeWidth_ = kUseFullAvailableWidth;
  [self layoutTabs];
}

@end
