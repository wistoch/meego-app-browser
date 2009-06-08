// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#include "chrome/browser/cocoa/find_bar_bridge.h"
#import "chrome/browser/cocoa/status_bubble_mac.h"
#import "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/common/pref_service.h"

namespace {

// Size of the gradient. Empirically determined so that the gradient looks
// like what the heuristic does when there are just a few tabs.
const int kWindowGradientHeight = 24;

}

@interface BrowserWindowController(Private)

- (void)positionToolbar;

// Leopard's gradient heuristic gets confused by our tabs and makes the title
// gradient jump when creating a tab that is less than a tab width from the
// right side of the screen. This function disables Leopard's gradient
// heuristic.
- (void)fixWindowGradient;

// Called by the Notification Center whenever the tabContentArea's
// frame changes.  Re-positions the bookmark bar and the find bar.
- (void)tabContentAreaFrameChanged:(id)sender;

// Saves the window's position in the local state preferences.
- (void)saveWindowPositionIfNeeded;

// Saves the window's position to the given pref service.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;

// We need to adjust where sheets come out of the window, as by default they
// erupt from the omnibox, which is rather weird.
- (NSRect)window:(NSWindow *)window
willPositionSheet:(NSWindow *)sheet
       usingRect:(NSRect)defaultSheetRect;
@end


@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  return [self initWithBrowser:browser takeOwnership:YES];
}

// Private (TestingAPI) init routine with testing options.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt {
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString *nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"BrowserWindow"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    DCHECK(browser);
    browser_.reset(browser);
    ownsBrowser_ = ownIt;
    tabObserver_.reset(
        new TabStripModelObserverBridge(browser->tabstrip_model(), self));
    windowShim_.reset(new BrowserWindowCocoa(browser, self, [self window]));

    // The window is now fully realized and |-windowDidLoad:| has been
    // called. We shouldn't do much in wDL because |windowShim_| won't yet
    // be initialized (as it's called in response to |[self window]| above).
    // Retain it per the comment in the header.
    window_.reset([[self window] retain]);

    // Since we don't have a standard resize control, Cocoa won't enable the
    // zoom (green) button on the titlebar for us. Grab it and enable it
    // manually. Note that when launched from XCode, the doesn't work for the
    // first window (and only the first window). There's some activation
    // wonkiness there, since XCode stays active and the menus don't switch
    // either. It always works when launched from the Finder.
    NSButton* zoomButton =
        [[self window] standardWindowButton:NSWindowZoomButton];
    [zoomButton setEnabled:YES];

    // Register ourselves for frame changed notifications from the
    // tabContentArea.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(tabContentAreaFrameChanged:)
               name:nil
             object:[self tabContentArea]];

    // Get the most appropriate size for the window. The window shim will handle
    // flipping the coordinates for us so we can use it to save some code.
    gfx::Rect windowRect = browser_->GetSavedWindowBounds();
    windowShim_->SetBounds(windowRect);

    // Create a controller for the tab strip, giving it the model object for
    // this window's Browser and the tab strip view. The controller will handle
    // registering for the appropriate tab notifications from the back-end and
    // managing the creation of new tabs.
    tabStripController_.reset([[TabStripController alloc]
                                initWithView:[self tabStripView]
                                  switchView:[self tabContentArea]
                                       model:browser_->tabstrip_model()]);

    // Create a controller for the toolbar, giving it the toolbar model object
    // and the toolbar view from the nib. The controller will handle
    // registering for the appropriate command state changes from the back-end.
    toolbarController_.reset([[ToolbarController alloc]
                               initWithModel:browser->toolbar_model()
                                    commands:browser->command_updater()
                                     profile:browser->profile()]);
    [self positionToolbar];

    // After we've adjusted the toolbar, create a controller for the bookmark
    // bar. It will show/hide itself based on the global preference and handle
    // positioning itself (if visible) above the content area, which is why
    // we need to do it after we've placed the toolbar.
    bookmarkBarController_.reset([[BookmarkBarController alloc]
                                initWithProfile:browser_->profile()
                                    contentView:[self tabContentArea]
                                       delegate:self]);

   [self fixWindowGradient];

    // Create the bridge for the status bubble.
    statusBubble_.reset(new StatusBubbleMac([self window]));
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  // Under certain testing configurations we may not actually own the browser.
  if (ownsBrowser_ == NO)
    browser_.release();
  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return windowShim_.get();
}

- (void)destroyBrowser {
  [NSApp removeWindowsItem:[self window]];

  // We need the window to go away now.
  [self autorelease];
}

// Called when the window meets the criteria to be closed (ie,
// |-windowShoudlClose:| returns YES). We must be careful to preserve the
// semantics of BrowserWindow::Close() and not call the Browser's dtor directly
// from this method.
- (void)windowWillClose:(NSNotification *)notification {
  DCHECK(!browser_->tabstrip_model()->count());

  // We can't actually use |-autorelease| here because there's an embedded
  // run loop in the |-performClose:| which contains its own autorelease pool.
  // Instead we use call it after a zero-length delay, which gets us back
  // to the main event loop.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
}

// Called when the user wants to close a window or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onUnload handlers needing to
// be fired. If closing is deferred, the Browser will handle the processing
// required to get us to the closing state and (by watching for all the tabs
// going away) will again call to close the window when it's finally ready.
- (BOOL)windowShouldClose:(id)sender {
  // Disable updates while closing all tabs to avoid flickering.
  NSDisableScreenUpdates();
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return NO;

  // saveWindowPositionIfNeeded: only works if we are the last active
  // window, but orderOut: ends up activating another window, so we
  // have to save the window position before we call orderOut:.
  [self saveWindowPositionIfNeeded];

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    [[self window] orderOut:self];
    browser_->OnWindowClosing();
    return NO;
  }
  NSEnableScreenUpdates();

  // the tab strip is empty, it's ok to close the window
  return YES;
}

// Called right after our window became the main window.
- (void)windowDidBecomeMain:(NSNotification *)notification {
  BrowserList::SetLastActive(browser_.get());
  [self saveWindowPositionIfNeeded];
}

// Called when the user clicks the zoom button (or selects it from the Window
// menu). Zoom to the appropriate size based on the content. Make sure we
// enforce a minimum width to ensure websites with small intrinsic widths
// (such as google.com) don't end up with a wee window. Enforce a max width
// that leaves room for icons on the right side. Use the full (usable) height
// regardless.
- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)frame {
  // If the shift key is down, maximize. Hopefully this should make the
  // "switchers" happy.
  if ([[[NSApplication sharedApplication] currentEvent] modifierFlags] &
          NSShiftKeyMask) {
    return [[window screen] visibleFrame];
  }

  const int kMinimumIntrinsicWidth = 700;
  const int kScrollbarWidth = 16;
  const int kSpaceForIcons = 50;
  const NSSize screenSize = [[window screen] visibleFrame].size;
  // Always leave room on the right for icons.
  const int kMaxWidth = screenSize.width - kSpaceForIcons;

  TabContents* contents = browser_->tabstrip_model()->GetSelectedTabContents();
  if (contents) {
    int intrinsicWidth = contents->view()->preferred_width() + kScrollbarWidth;
    int tempWidth = std::max(intrinsicWidth, kMinimumIntrinsicWidth);
    frame.size.width = std::min(tempWidth, kMaxWidth);
    frame.size.height = screenSize.height;
  }
  return frame;
}

// Update a toggle state for an NSMenuItem if modified.
// Take care to insure |item| looks like a NSMenuItem.
// Called by validateUserInterfaceItem:.
- (void)updateToggleStateWithTag:(NSInteger)tag forItem:(id)item {
  if (![item respondsToSelector:@selector(state)] ||
      ![item respondsToSelector:@selector(setState:)])
    return;

  // On Windows this logic happens in bookmark_bar_view.cc.  On the
  // Mac we're a lot more MVC happy so we've moved it into a
  // controller.  To be clear, this simply updates the menu item; it
  // does not display the bookmark bar itself.
  if (tag == IDC_SHOW_BOOKMARK_BAR) {
    bool toggled = windowShim_->IsBookmarkBarVisible();
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }

  // Update the checked/Unchecked state of items in the encoding menu.
  // On Windows this logic is part of encoding_menu_controller_delegate.cc
  EncodingMenuController encoding_controller;
  if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
    DCHECK(browser_.get());
    Profile *profile = browser_->profile();
    DCHECK(profile);
    TabContents* current_tab = browser_->GetSelectedTabContents();
    if (!current_tab) {
      return;
    }
    const std::wstring encoding = current_tab->encoding();

    bool toggled = encoding_controller.IsItemChecked(profile, encoding, tag);
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }

}

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
// NOTE: we might have to handle state for app-wide menu items,
// although we could cheat and directly ask the app controller if our
// command_updater doesn't support the command. This may or may not be an issue,
// too early to tell.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (browser_->command_updater()->SupportsCommand(tag)) {
      // Generate return value (enabled state)
      enable = browser_->command_updater()->IsCommandEnabled(tag) ? YES : NO;

      // Disable "close tab" if we're not the key window or if there's only
      // one tab.
      if (tag == IDC_CLOSE_TAB)
        enable &= [self numberOfTabs] > 1 && [[self window] isKeyWindow];

      // If the item is toggleable, find it's toggle state and
      // try to update it.  This is a little awkward, but the alternative is
      // to check after a commandDispatch, which seems worse.
      [self updateToggleStateWithTag:tag forItem:item];
    }
  }
  return enable;
}

// Called when the user picks a menu or toolbar item when this window is key.
// Calls through to the browser object to execute the command. This assumes that
// the command is supported and doesn't check, otherwise it would have been
// disabled in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  NSInteger tag = [sender tag];
  browser_->ExecuteCommand(tag);
}

- (LocationBar*)locationBar {
  return [toolbarController_ locationBar];
}

- (StatusBubble*)statusBubble {
  return statusBubble_.get();
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [toolbarController_ updateToolbarWithContents:tab
                             shouldRestoreState:shouldRestore];
}

- (void)setStarredState:(BOOL)isStarred {
  [toolbarController_ setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
// |windowGrowBox| needs to be in the window's coordinate system.
- (NSRect)selectedTabGrowBoxRect {
  return [tabStripController_ selectedTabGrowBoxRect];
}

// Accept tabs from a BrowserWindowController with the same Profile.
- (BOOL)canReceiveFrom:(TabWindowController*)source {
  if (![source isKindOfClass:[BrowserWindowController class]]) {
    return NO;
  }

  BrowserWindowController* realSource =
      static_cast<BrowserWindowController*>(source);
  if (browser_->profile() != realSource->browser_->profile()) {
    return NO;
  }

  return YES;
}

// Move a given tab view to the location of the current placeholder. If there is
// no placeholder, it will go at the end. |controller| is the window controller
// of a tab being dropped from a different window. It will be nil if the drag is
// within the window, otherwise the tab is removed from that window before being
// placed into this one. The implementation will call |-removePlaceholder| since
// the drag is now complete.  This also calls |-layoutTabs| internally so
// clients do not need to call it again.
- (void)moveTabView:(NSView*)view
     fromController:(TabWindowController*)dragController {
  if (dragController) {
    // Moving between windows. Figure out the TabContents to drop into our tab
    // model from the source window's model.
    BOOL isBrowser =
        [dragController isKindOfClass:[BrowserWindowController class]];
    DCHECK(isBrowser);
    if (!isBrowser) return;
    BrowserWindowController* dragBWC = (BrowserWindowController*)dragController;
    int index = [dragBWC->tabStripController_ indexForTabView:view];
    TabContents* contents =
        dragBWC->browser_->tabstrip_model()->GetTabContentsAt(index);

    // Now that we have enough information about the tab, we can remove it from
    // the dragging window. We need to do this *before* we add it to the new
    // window as this will removes the TabContents' delegate.
    [dragController detachTabView:view];

    // Deposit it into our model at the appropriate location (it already knows
    // where it should go from tracking the drag). Doing this sets the tab's
    // delegate to be the Browser.
    [tabStripController_ dropTabContents:contents];
  } else {
    // Moving within a window.
    int index = [tabStripController_ indexForTabView:view];
    [tabStripController_ moveTabFromIndex:index];
  }

  // Remove the placeholder since the drag is now complete.
  [self removePlaceholder];
}

// Tells the tab strip to forget about this tab in preparation for it being
// put into a different tab strip, such as during a drop on another window.
- (void)detachTabView:(NSView*)view {
  int index = [tabStripController_ indexForTabView:view];
  browser_->tabstrip_model()->DetachTabContentsAt(index);
}

- (NSView *)selectedTabView {
  return [tabStripController_ selectedTabView];
}

- (TabStripController *)tabStripController {
  return tabStripController_;
}

- (void)setIsLoading:(BOOL)isLoading {
  [toolbarController_ setIsLoading:isLoading];
}

// Called to start/stop the loading animations.
- (void)updateLoadingAnimations:(BOOL)animate {
  if (animate) {
    // TODO(pinkerton): determine what throbber animation is necessary and
    // start a timer to periodically update. Windows tells the tab strip to
    // do this. It uses a single timer to coalesce the multiple things that
    // could be updating. http://crbug.com/8281
  } else {
    // TODO(pinkerton): stop the timer.
  }
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar {
  [toolbarController_ focusLocationBar];
}

- (void)layoutTabs {
  [tabStripController_ layoutTabs];
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Disable screen updates so that this appears as a single visual change.
  NSDisableScreenUpdates();

  // Fetch the tab contents for the tab being dragged
  int index = [tabStripController_ indexForTabView:tabView];
  TabContents* contents = browser_->tabstrip_model()->GetTabContentsAt(index);

  // Set the window size. Need to do this before we detach the tab so it's
  // still in the window. We have to flip the coordinates as that's what
  // is expected by the Browser code.
  NSWindow* sourceWindow = [tabView window];
  NSRect windowRect = [sourceWindow frame];
  NSScreen* screen = [sourceWindow screen];
  windowRect.origin.y =
      [screen frame].size.height - windowRect.size.height -
          windowRect.origin.y;
  gfx::Rect browserRect(windowRect.origin.x, windowRect.origin.y,
                        windowRect.size.width, windowRect.size.height);

  NSRect tabRect = [tabView frame];

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents. This needs to come before creating the new
  // Browser because it clears the TabContents' delegate, which gets hooked
  // up during creation of the new window.
  browser_->tabstrip_model()->DetachTabContentsAt(index);

  // Create the new window with a single tab in its model, the one being
  // dragged.
  DockInfo dockInfo;
  Browser* newBrowser =
      browser_->tabstrip_model()->TearOffTabContents(contents,
                                                     browserRect,
                                                     dockInfo);

  // Get the new controller by asking the new window for its delegate.
  BrowserWindowController* controller =
      [newBrowser->window()->GetNativeHandle() delegate];
  DCHECK(controller && [controller isKindOfClass:[TabWindowController class]]);

  // Force the added tab to the right size (remove stretching.)
  tabRect.size.height = [TabStripController defaultTabHeight];

  // And make sure we use the correct frame in the new view.
  [[controller tabStripController] setFrameOfSelectedTab:tabRect];

  NSEnableScreenUpdates();
  return controller;
}


- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                      yStretchiness:(CGFloat)yStretchiness {
  [tabStripController_ insertPlaceholderForTab:tab
                                         frame:frame
                                 yStretchiness:yStretchiness];
}

- (void)removePlaceholder {
  [tabStripController_ insertPlaceholderForTab:nil
                                         frame:NSZeroRect
                                 yStretchiness:0];
}

- (BOOL)isBookmarkBarVisible {
  return [bookmarkBarController_ isBookmarkBarVisible];

}

- (void)toggleBookmarkBar {
  [bookmarkBarController_ toggleBookmarkBar];
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  // Shouldn't call addFindBar twice.
  DCHECK(!findBarCocoaController_.get());

  // Create a controller for the findbar.
  findBarCocoaController_.reset([findBarCocoaController retain]);
  [[[self window] contentView] addSubview:[findBarCocoaController_ view]];
  [findBarCocoaController_ positionFindBarView:[self tabContentArea]];
}

// Called by the bookmark bar to open a URL.
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition {
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  DCHECK(tab_contents);
  tab_contents->OpenURL(url, GURL(), disposition,
                        PageTransition::AUTO_BOOKMARK);
}

- (NSInteger)numberOfTabs {
  return browser_->tabstrip_model()->count();
}

- (NSString*)selectedTabTitle {
  TabContents* contents = browser_->tabstrip_model()->GetSelectedTabContents();
  return base::SysUTF16ToNSString(contents->GetTitle());
}

- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  DCHECK(oldContents != newContents);

  // Update various elements that are interested in knowing the current
  // TabContents.
#if 0
// TODO(pinkerton):Update as more things become window-specific
  infobar_container_->ChangeTabContents(new_contents);
  contents_container_->SetTabContents(new_contents);
#endif
  newContents->DidBecomeSelected();

  // Update all the UI bits.
  windowShim_->UpdateTitleBar();
#if 0
// TODO(pinkerton):Update as more things become window-specific
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);
#endif
}

- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index
                   loadingOnly:(BOOL)loading {
  // Update titles if this is the currently selected tab.
  if (index == browser_->tabstrip_model()->selected_index())
    windowShim_->UpdateTitleBar();
}

@end


@interface NSWindow (NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.

- (void)setAutorecalculatesContentBorderThickness:(BOOL)b
                                          forEdge:(NSRectEdge)e;
- (void)setContentBorderThickness:(CGFloat)b forEdge:(NSRectEdge)e;
@end


@implementation BrowserWindowController (Private)

// Position |toolbarView_| below the tab strip, but not as a sibling. The
// toolbar is part of the window's contentView, mainly because we want the
// opacity during drags to be the same as the web content.
- (void)positionToolbar {
  NSView* contentView = [self tabContentArea];
  NSRect contentFrame = [contentView frame];
  NSView* toolbarView = [toolbarController_ view];
  NSRect toolbarFrame = [toolbarView frame];

  // Shrink the content area by the height of the toolbar.
  contentFrame.size.height -= toolbarFrame.size.height;
  [contentView setFrame:contentFrame];

  // Move the toolbar above the content area, within the window's content view
  // (as opposed to the tab strip, which is a sibling).
  toolbarFrame.origin.y = NSMaxY(contentFrame);
  toolbarFrame.origin.x = 0;
  toolbarFrame.size.width = contentFrame.size.width;
  [toolbarView setFrame:toolbarFrame];
  [[[self window] contentView] addSubview:toolbarView];
}

- (void)fixWindowGradient {
  NSWindow* win = [self window];
  if ([win respondsToSelector:@selector(
          setAutorecalculatesContentBorderThickness:forEdge:)] &&
      [win respondsToSelector:@selector(
           setContentBorderThickness:forEdge:)]) {
    [win setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [win setContentBorderThickness:kWindowGradientHeight forEdge:NSMaxYEdge];
  }
}

- (void)tabContentAreaFrameChanged:(id)sender {
  // TODO(rohitrao): This is triggered by window resizes also.  Make
  // sure we aren't doing anything wasteful in those cases.
  [bookmarkBarController_ resizeBookmarkBar];

  if (findBarCocoaController_.get()) {
    [findBarCocoaController_ positionFindBarView:[self tabContentArea]];
  }
}

- (void)saveWindowPositionIfNeeded {
  if (browser_ != BrowserList::GetLastActive())
    return;

  if (!g_browser_process || !g_browser_process->local_state() ||
      !browser_->ShouldSaveWindowPlacement())
    return;

  [self saveWindowPositionToPrefs:g_browser_process->local_state()];
}

- (void)saveWindowPositionToPrefs:(PrefService*)prefs {
  // Window placements are stored relative to the work area bounds,
  // not the monitor bounds.
  NSRect workFrame = [[[self window] screen] visibleFrame];

  // Start with the window's frame, which is in virtual coordinates.
  // Subtract the origin of the visibleFrame to get the window frame
  // relative to the work area.
  gfx::Rect bounds(NSRectToCGRect([[self window] frame]));
  bounds.Offset(-workFrame.origin.x, -workFrame.origin.y);

  // Do some y twiddling to flip the coordinate system.
  bounds.set_y(workFrame.size.height - bounds.y() - bounds.height());

  DictionaryValue* windowPreferences = prefs->GetMutableDictionary(
      browser_->GetWindowPlacementKey().c_str());
  windowPreferences->SetInteger(L"left", bounds.x());
  windowPreferences->SetInteger(L"top", bounds.y());
  windowPreferences->SetInteger(L"right", bounds.right());
  windowPreferences->SetInteger(L"bottom", bounds.bottom());
  windowPreferences->SetBoolean(L"maximized", false);
  windowPreferences->SetBoolean(L"always_on_top", false);
}

- (NSRect)window:(NSWindow *)window
willPositionSheet:(NSWindow *)sheet
       usingRect:(NSRect)defaultSheetRect {
  NSRect windowFrame = [window frame];
  defaultSheetRect.origin.y = windowFrame.size.height - 10;
  return defaultSheetRect;
}

// In addition to the tab strip and content area, which the superview's impl
// takes care of, we need to add the toolbar and bookmark bar to the
// overlay so they draw correctly when dragging out a new window.
- (NSArray*)viewsToMoveToOverlay {
  NSArray* views = [super viewsToMoveToOverlay];
  NSArray* browserViews =
      [NSArray arrayWithObjects:[toolbarController_ view],
                                [bookmarkBarController_ view],
                                nil];
  return [views arrayByAddingObjectsFromArray:browserViews];
}

// Undocumented method for multi-touch gestures in 10.5. Future OS's will
// likely add a public API, but the worst that will happen is that this will
// turn into dead code and just won't get called.
- (void)swipeWithEvent:(NSEvent*)event {
  // Map forwards and backwards to history; left is positive, right is negative.
  unsigned int command = 0;
  if ([event deltaX] > 0.5)
    command = IDC_BACK;
  else if ([event deltaX] < -0.5)
    command = IDC_FORWARD;
  else if ([event deltaY] > 0.5)
    ;  // TODO(pinkerton): figure out page-up
  else if ([event deltaY] < -0.5)
    ;  // TODO(pinkerton): figure out page-down

  // Ensure the command is valid first (ExecuteCommand() won't do that) and
  // then make it so.
  if (browser_->command_updater()->IsCommandEnabled(command))
    browser_->ExecuteCommand(command);
}

@end
