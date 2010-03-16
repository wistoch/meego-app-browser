// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include <algorithm>

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/bubble_positioner.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/back_forward_menu_controller.h"
#import "chrome/browser/cocoa/background_gradient_view.h"
#import "chrome/browser/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#import "chrome/browser/cocoa/menu_button.h"
#import "chrome/browser/cocoa/menu_controller.h"
#import "chrome/browser/cocoa/toolbar_view.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/page_menu_model.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/rect.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Names of images in the bundle for buttons.
NSString* const kBackButtonImageName = @"back_Template.pdf";
NSString* const kForwardButtonImageName = @"forward_Template.pdf";
NSString* const kReloadButtonImageName = @"reload_Template.pdf";
NSString* const kHomeButtonImageName = @"home_Template.pdf";
NSString* const kStarButtonImageName = @"star_Template.pdf";
NSString* const kStarButtonFillingImageName = @"starred.pdf";
NSString* const kGoButtonGoImageName = @"go_Template.pdf";
NSString* const kGoButtonStopImageName = @"stop_Template.pdf";
NSString* const kPageButtonImageName = @"menu_page_Template.pdf";
NSString* const kWrenchButtonImageName = @"menu_chrome_Template.pdf";

// Height of the toolbar in pixels when the bookmark bar is closed.
const CGFloat kBaseToolbarHeight = 36.0;

// The distance from the 'Go' button to the Browser Actions container in pixels.
const CGFloat kBrowserActionsContainerLeftPadding = 5.0;

// The minimum width of the location bar in pixels.
const CGFloat kMinimumLocationBarWidth = 100.0;

}  // namespace

@interface ToolbarController(Private)
- (void)addAccessibilityDescriptions;
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(std::wstring*)prefName;
- (BackgroundGradientView*)backgroundGradientView;
- (void)toolbarFrameChanged;
- (void)pinGoButtonToLeftOfBrowserActionsContainer;
- (void)maintainMinimumLocationBarWidth;
- (void)adjustBrowserActionsContainerForNewWindow;
- (void)browserActionsContainerDragged;
- (void)browserActionsVisibilityChanged;
- (void)adjustLocationAndGoPositionsBy:(CGFloat)dX;
@end

namespace {

// A C++ class used to correctly position the omnibox.
class BubblePositionerMac : public BubblePositioner {
 public:
  BubblePositionerMac(ToolbarController* controller)
      : controller_(controller) { }
  virtual ~BubblePositionerMac() { }

  // BubblePositioner:
  virtual gfx::Rect GetLocationStackBounds() const {
    return [controller_ locationStackBounds];
  }

 private:
  ToolbarController* controller_;  // weak, owns us
};

}  // namespace

namespace ToolbarControllerInternal {

// A C++ delegate that handles enabling/disabling menu items and handling when
// a menu command is chosen.
class MenuDelegate : public menus::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(Browser* browser)
      : browser_(browser) { }

  // Overridden from menus::SimpleMenuModel::Delegate
  virtual bool IsCommandIdChecked(int command_id) const {
    if (command_id == IDC_SHOW_BOOKMARK_BAR) {
      return browser_->profile()->GetPrefs()->GetBoolean(
          prefs::kShowBookmarkBar);
    }
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const {
    return browser_->command_updater()->IsCommandEnabled(command_id);
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) { return false; }
  virtual void ExecuteCommand(int command_id) {
    browser_->ExecuteCommand(command_id);
  }
  virtual bool IsLabelForCommandIdDynamic(int command_id) const {
    // On Mac, switch between "Enter Full Screen" and "Exit Full Screen".
    return (command_id == IDC_FULLSCREEN);
  }
  virtual string16 GetLabelForCommandId(int command_id) const {
    if (command_id == IDC_FULLSCREEN) {
      int string_id = IDS_ENTER_FULLSCREEN_MAC;  // Default to Enter.
      // Note: On startup, |window()| may be NULL.
      if (browser_->window() && browser_->window()->IsFullscreen())
        string_id = IDS_EXIT_FULLSCREEN_MAC;
      return l10n_util::GetStringUTF16(string_id);
    }
    return menus::SimpleMenuModel::Delegate::GetLabelForCommandId(command_id);
  }

 private:
  Browser* browser_;
};

// A C++ class registered for changes in preferences. Bridges the
// notification back to the ToolbarController.
class PrefObserverBridge : public NotificationObserver {
 public:
  explicit PrefObserverBridge(ToolbarController* controller)
      : controller_(controller) { }
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [controller_ prefChanged:Details<std::wstring>(details).ptr()];
  }
 private:
  ToolbarController* controller_;  // weak, owns us
};

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate {
  DCHECK(model && commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:mac_util::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;
    browser_ = browser;
    resizeDelegate_ = resizeDelegate;
    hasToolbar_ = YES;
    hasLocationBar_ = YES;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(new CommandObserverBridge(self, commands));
    commandObserver_->ObserveCommand(IDC_BACK);
    commandObserver_->ObserveCommand(IDC_FORWARD);
    commandObserver_->ObserveCommand(IDC_RELOAD);
    commandObserver_->ObserveCommand(IDC_HOME);
    commandObserver_->ObserveCommand(IDC_BOOKMARK_PAGE);
  }
  return self;
}

- (void)dealloc {
  // Make sure any code in the base class which assumes [self view] is
  // the "parent" view continues to work.
  hasToolbar_ = YES;
  hasLocationBar_ = YES;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (trackingArea_.get())
    [[self view] removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location
// bar and button state.
- (void)awakeFromNib {
  // A bug in AppKit (<rdar://7298597>, <http://openradar.me/7298597>) causes
  // images loaded directly from nibs in a framework to not get their "template"
  // flags set properly. Thus, despite the images being set on the buttons in
  // the xib, we must set them in code.
  [backButton_ setImage:nsimage_cache::ImageNamed(kBackButtonImageName)];
  [forwardButton_ setImage:nsimage_cache::ImageNamed(kForwardButtonImageName)];
  [reloadButton_ setImage:nsimage_cache::ImageNamed(kReloadButtonImageName)];
  [homeButton_ setImage:nsimage_cache::ImageNamed(kHomeButtonImageName)];
  [starButton_ setImage:nsimage_cache::ImageNamed(kStarButtonImageName)];
  [goButton_ setImage:nsimage_cache::ImageNamed(kGoButtonGoImageName)];
  [pageButton_ setImage:nsimage_cache::ImageNamed(kPageButtonImageName)];
  [wrenchButton_ setImage:nsimage_cache::ImageNamed(kWrenchButtonImageName)];

  [pageButton_ setShowsBorderOnlyWhileMouseInside:YES];
  [wrenchButton_ setShowsBorderOnlyWhileMouseInside:YES];

  [self initCommandStatus:commands_];
  bubblePositioner_.reset(new BubblePositionerMac(self));
  locationBarView_.reset(new LocationBarViewMac(locationBar_,
                                                bubblePositioner_.get(),
                                                commands_, toolbarModel_,
                                                profile_, browser_));
  [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar based on those prefs.
  prefObserver_.reset(new ToolbarControllerInternal::PrefObserverBridge(self));
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(prefs::kShowHomeButton, prefs, prefObserver_.get());
  showPageOptionButtons_.Init(prefs::kShowPageOptionsButtons, prefs,
                              prefObserver_.get());
  [self showOptionalHomeButton];
  [self showOptionalPageWrenchButtons];

  // Create the controllers for the back/forward menus.
  backMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_BACK
                   button:backButton_]);
  forwardMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_FORWARD
                   button:forwardButton_]);

  // For a popup window, the toolbar is really just a location bar
  // (see override for [ToolbarController view], below).  When going
  // fullscreen, we remove the toolbar controller's view from the view
  // hierarchy.  Calling [locationBar_ removeFromSuperview] when going
  // fullscreen causes it to get released, making us unhappy
  // (http://crbug.com/18551).  We avoid the problem by incrementing
  // the retain count of the location bar; use of the scoped object
  // helps us remember to release it.
  locationBarRetainer_.reset([locationBar_ retain]);
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:NSZeroRect // Ignored
                                   options:NSTrackingMouseMoved |
                                           NSTrackingInVisibleRect |
                                           NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways
                                     owner:self
                                  userInfo:nil]);
  NSView* toolbarView = [self view];
  [toolbarView addTrackingArea:trackingArea_.get()];

  // We want a dynamic tooltip on the go button, so tell the go button to ask
  // us for the tooltip.
  [goButton_ addToolTipRect:[goButton_ bounds] owner:self userData:nil];

  // If the user has any Browser Actions installed, the container view for them
  // may have to be resized depending on the width of the toolbar frame.
  [toolbarView setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(toolbarFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:toolbarView];
}

- (void)addAccessibilityDescriptions {
  // Set accessibility descriptions. http://openradar.appspot.com/7496255
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_FORWARD);
  [[forwardButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_RELOAD);
  [[reloadButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_HOME);
  [[homeButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_STAR);
  [[starButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_LOCATION);
  [[locationBar_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_GO);
  [[goButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_PAGE);
  [[pageButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_APP);
  [[wrenchButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [[hoveredButton_ cell] setMouseInside:NO animate:YES];
  [hoveredButton_ release];
  hoveredButton_ = nil;
}

- (NSButton*)hoverButtonForEvent:(NSEvent*)theEvent {
  NSButton* targetView = (NSButton*)[[self view]
                                     hitTest:[theEvent locationInWindow]];

  // Only interpret the view as a hoverButton_ if it's both button and has a
  // button cell that cares.  GradientButtonCell derived cells care.
  if (([targetView isKindOfClass:[NSButton class]]) &&
      ([[targetView cell]
         respondsToSelector:@selector(setMouseInside:animate:)]))
    return targetView;
  return nil;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  NSButton* targetView = [self hoverButtonForEvent:theEvent];
  if (hoveredButton_ != targetView) {
    [[hoveredButton_ cell] setMouseInside:NO animate:YES];
    [[targetView cell] setMouseInside:YES animate:YES];
    [hoveredButton_ release];
    hoveredButton_ = [targetView retain];
  }
}

- (void)mouseEntered:(NSEvent*)event {
  [self mouseMoved:event];
}

- (LocationBar*)locationBarBridge {
  return locationBarView_.get();
}

- (void)focusLocationBar {
  if (locationBarView_.get()) {
    locationBarView_->FocusLocation();
  }
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  NSButton* button = nil;
  switch (command) {
    case IDC_BACK:
      button = backButton_;
      break;
    case IDC_FORWARD:
      button = forwardButton_;
      break;
    case IDC_HOME:
      button = homeButton_;
      break;
    case IDC_BOOKMARK_PAGE:
      button = starButton_;
      break;
  }
  [button setEnabled:enabled];
}

// Init the enabled state of the buttons on the toolbar to match the state in
// the controller.
- (void)initCommandStatus:(CommandUpdater*)commands {
  [backButton_ setEnabled:commands->IsCommandEnabled(IDC_BACK) ? YES : NO];
  [forwardButton_
      setEnabled:commands->IsCommandEnabled(IDC_FORWARD) ? YES : NO];
  [reloadButton_ setEnabled:commands->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  [homeButton_ setEnabled:commands->IsCommandEnabled(IDC_HOME) ? YES : NO];
  [starButton_
      setEnabled:commands->IsCommandEnabled(IDC_BOOKMARK_PAGE) ? YES : NO];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  locationBarView_->Update(tab, shouldRestore ? true : false);

  [locationBar_ updateCursorAndToolTipRects];

  if (browserActionsController_.get()) {
    [browserActionsController_ update];
  }
}

- (void)setStarredState:(BOOL)isStarred {
  NSImage* starImage = nil;
  NSString* toolTip;
  if (isStarred) {
    starImage = nsimage_cache::ImageNamed(kStarButtonFillingImageName);
    // Cache the string since we'll need it a lot
    static NSString* starredToolTip =
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STARRED) retain];
    toolTip = starredToolTip;
  } else {
    // Cache the string since we'll need it a lot
    static NSString* starToolTip =
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STAR) retain];
    toolTip = starToolTip;
  }

  [(GradientButtonCell*)[starButton_ cell] setUnderlayImage:starImage];
  [starButton_ setToolTip:toolTip];
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = kGoButtonGoImageName;
  NSInteger tag = IDC_GO;
  if (isLoading) {
    imageName = kGoButtonStopImageName;
    tag = IDC_STOP;
  }
  NSImage* stopStartImage = nsimage_cache::ImageNamed(imageName);
  [goButton_ setImage:stopStartImage];
  [goButton_ setTag:tag];
}

- (void)setHasToolbar:(BOOL)toolbar hasLocationBar:(BOOL)locBar {
  [self view];  // Force nib loading.

  hasToolbar_ = toolbar;

  // If there's a toolbar, there must be a location bar.
  DCHECK((toolbar && locBar) || !toolbar);
  hasLocationBar_ = toolbar ? YES : locBar;

  // Decide whether to hide/show based on whether there's a location bar.
  [[self view] setHidden:!hasLocationBar_];

  // Make location bar not editable when in a pop-up.
  // TODO(viettrungluu): is this right (all the time)?
  [locationBar_ setEditable:toolbar];
}

- (NSView*)view {
  if (hasToolbar_)
    return [super view];
  return locationBar_;
}

// (Private) Returns the backdrop to the toolbar.
- (BackgroundGradientView*)backgroundGradientView {
  // We really do mean |[super view]|; see our override of |-view|.
  DCHECK([[super view] isKindOfClass:[BackgroundGradientView class]]);
  return (BackgroundGradientView*)[super view];
}

- (id)customFieldEditorForObject:(id)obj {
  if (obj == locationBar_) {
    // Lazilly construct Field editor, Cocoa UI code always runs on the
    // same thread, so there shoudn't be a race condition here.
    if (autocompleteTextFieldEditor_.get() == nil) {
      autocompleteTextFieldEditor_.reset(
          [[AutocompleteTextFieldEditor alloc] init]);
      [autocompleteTextFieldEditor_ setProfile:profile_];
    }

    // This needs to be called every time, otherwise notifications
    // aren't sent correctly.
    DCHECK(autocompleteTextFieldEditor_.get());
    [autocompleteTextFieldEditor_.get() setFieldEditor:YES];
    return autocompleteTextFieldEditor_.get();
  }
  return nil;
}

// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews {
  return [NSArray arrayWithObjects:backButton_, forwardButton_, reloadButton_,
            homeButton_, starButton_, goButton_, pageButton_, wrenchButton_,
            locationBar_, browserActionsContainerView_, nil];
}

// Moves |rect| to the right by |delta|, keeping the right side fixed by
// shrinking the width to compensate. Passing a negative value for |deltaX|
// moves to the left and increases the width.
- (NSRect)adjustRect:(NSRect)rect byAmount:(CGFloat)deltaX {
  NSRect frame = NSOffsetRect(rect, deltaX, 0);
  frame.size.width -= deltaX;
  return frame;
}

// Computes the padding between the buttons that should have a separation from
// the positions in the nib. Since the forward and reload buttons are always
// visible, we use those buttons as the canonical spacing.
- (CGFloat)interButtonSpacing {
  NSRect forwardFrame = [forwardButton_ frame];
  NSRect reloadFrame = [reloadButton_ frame];
  DCHECK(NSMinX(reloadFrame) > NSMaxX(forwardFrame));
  return NSMinX(reloadFrame) - NSMaxX(forwardFrame);
}

// Show or hide the home button based on the pref.
- (void)showOptionalHomeButton {
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
  BOOL hide = showHomeButton_.GetValue() ? NO : YES;
  if (hide == [homeButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Always shift the star and text field by the width of the home button plus
  // the appropriate gap width. If we're hiding the button, we have to
  // reverse the direction of the movement (to the left).
  CGFloat moveX = [self interButtonSpacing] + [homeButton_ frame].size.width;
  if (hide)
    moveX *= -1;  // Reverse the direction of the move.

  [starButton_ setFrame:NSOffsetRect([starButton_ frame], moveX, 0)];
  [locationBar_ setFrame:[self adjustRect:[locationBar_ frame]
                                 byAmount:moveX]];
  [homeButton_ setHidden:hide];
}

// Lazily install the menus on the page and wrench buttons. Calling this
// repeatedly is inexpensive so it can be done every time the buttons are shown.
- (void)installPageWrenchMenus {
  if (pageMenuModel_.get())
    return;
  menuDelegate_.reset(new ToolbarControllerInternal::MenuDelegate(browser_));
  pageMenuModel_.reset(new PageMenuModel(menuDelegate_.get(), browser_));
  pageMenuController_.reset(
      [[MenuController alloc] initWithModel:pageMenuModel_.get()
                     useWithPopUpButtonCell:YES]);
  [pageButton_ setAttachedMenu:[pageMenuController_ menu]];

  appMenuModel_.reset(new AppMenuModel(menuDelegate_.get(), browser_));
  appMenuController_.reset(
      [[MenuController alloc] initWithModel:appMenuModel_.get()
                     useWithPopUpButtonCell:YES]);
  [wrenchButton_ setAttachedMenu:[appMenuController_ menu]];
}

// Show or hide the page and wrench buttons based on the pref.
- (void)showOptionalPageWrenchButtons {
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
  DCHECK([pageButton_ isHidden] == [wrenchButton_ isHidden]);
  BOOL hide = showPageOptionButtons_.GetValue() ? NO : YES;
  if (!hide)
    [self installPageWrenchMenus];
  if (hide == [pageButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Shift the go button and resize the text field by the width of the
  // page/wrench buttons plus two times the gap width. If we're showing the
  // buttons, we have to reverse the direction of movement (to the left). Unlike
  // the home button above, we only ever have to resize the text field, we don't
  // have to move it.
  CGFloat moveX = 2 * [self interButtonSpacing] + NSWidth([pageButton_ frame]) +
      NSWidth([wrenchButton_ frame]);

  if (!hide)
    moveX *= -1;  // Reverse the direction of the move.

  [self adjustLocationAndGoPositionsBy:moveX];
  [browserActionsContainerView_ setFrame:NSOffsetRect(
      [browserActionsContainerView_ frame], moveX, 0)];

  [browserActionsContainerView_ setRightBorderShown:!hide];
  [pageButton_ setHidden:hide];
  [wrenchButton_ setHidden:hide];
}

- (void)prefChanged:(std::wstring*)prefName {
  if (!prefName) return;
  if (*prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  } else if (*prefName == prefs::kShowPageOptionsButtons) {
    [self showOptionalPageWrenchButtons];
  }
}

- (void)createBrowserActionButtons {
  if (!browserActionsController_.get()) {
    browserActionsController_.reset([[BrowserActionsController alloc]
            initWithBrowser:browser_
              containerView:browserActionsContainerView_]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsContainerDragged)
               name:kBrowserActionGrippyDraggingNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsVisibilityChanged)
               name:kBrowserActionVisibilityChangedNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(adjustBrowserActionsContainerForNewWindow)
               name:NSWindowDidBecomeKeyNotification
             object:[[self view] window]];
  }

  CGFloat dX = NSWidth([browserActionsContainerView_ frame]) * -1;
  [self adjustLocationAndGoPositionsBy:dX];
  BOOL rightBorderShown = !([pageButton_ isHidden] && [wrenchButton_ isHidden]);
  [browserActionsContainerView_ setRightBorderShown:rightBorderShown];
}

- (void)adjustBrowserActionsContainerForNewWindow {
  [self toolbarFrameChanged];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowDidBecomeKeyNotification
              object:[[self view] window]];
}

- (void)browserActionsContainerDragged {
  CGFloat locationBarWidth = NSWidth([locationBar_ frame]);
  locationBarAtMinSize_ = locationBarWidth <= kMinimumLocationBarWidth;
  [browserActionsContainerView_ setCanDragLeft:!locationBarAtMinSize_];
  [browserActionsContainerView_ setGrippyPinned:locationBarAtMinSize_];

  [self adjustLocationAndGoPositionsBy:
      [browserActionsContainerView_ resizeDeltaX]];
}

- (void)browserActionsVisibilityChanged {
  [self pinGoButtonToLeftOfBrowserActionsContainer];
}

- (void)pinGoButtonToLeftOfBrowserActionsContainer {
  NSRect goFrame = [goButton_ frame];
  NSRect containerFrame = [browserActionsContainerView_ frame];
  CGFloat leftPadding = containerFrame.origin.x -
      (goFrame.origin.x + NSWidth(goFrame));
  if (leftPadding != kBrowserActionsContainerLeftPadding) {
    CGFloat dX = leftPadding - kBrowserActionsContainerLeftPadding;
    [self adjustLocationAndGoPositionsBy:dX];
  }
}

- (void)maintainMinimumLocationBarWidth {
  CGFloat locationBarWidth = NSWidth([locationBar_ frame]);
  locationBarAtMinSize_ = locationBarWidth <= kMinimumLocationBarWidth;
  if (locationBarAtMinSize_) {
    CGFloat dX = kMinimumLocationBarWidth - locationBarWidth;
    [self adjustLocationAndGoPositionsBy:dX];
  }
}

- (void)toolbarFrameChanged {
  // Do nothing if the frame changes but no Browser Action Controller is
  // present.
  if (!browserActionsController_.get())
    return;

  [self maintainMinimumLocationBarWidth];

  if (locationBarAtMinSize_) {
    // Once the grippy is pinned, leave it until it is explicity un-pinned.
    [browserActionsContainerView_ setGrippyPinned:YES];
    NSRect containerFrame = [browserActionsContainerView_ frame];
    // Determine how much the container needs to move in case it's overlapping
    // with the location bar.
    CGFloat dX = ([goButton_ frame].origin.x + NSWidth([goButton_ frame])) -
        containerFrame.origin.x + kBrowserActionsContainerLeftPadding;
    containerFrame = NSOffsetRect(containerFrame, dX, 0);
    containerFrame.size.width -= dX;
    [browserActionsContainerView_ setFrame:containerFrame];
  } else if (!locationBarAtMinSize_ &&
      [browserActionsContainerView_ grippyPinned]) {
    // Expand out the container until it hits the saved size, then unpin the
    // grippy.
    // Add 0.1 pixel so that it doesn't hit the minimum width codepath above.
    CGFloat dX = NSWidth([locationBar_ frame]) -
        (kMinimumLocationBarWidth + 0.1);
    NSRect containerFrame = [browserActionsContainerView_ frame];
    containerFrame = NSOffsetRect(containerFrame, -dX, 0);
    containerFrame.size.width += dX;
    CGFloat savedContainerWidth = [browserActionsController_ savedWidth];
    if (NSWidth(containerFrame) >= savedContainerWidth) {
      containerFrame = NSOffsetRect(containerFrame,
          NSWidth(containerFrame) - savedContainerWidth, 0);
      containerFrame.size.width = savedContainerWidth;
      [browserActionsContainerView_ setGrippyPinned:NO];
    }
    [browserActionsContainerView_ setFrame:containerFrame];
    [self pinGoButtonToLeftOfBrowserActionsContainer];
  }
}

- (void)adjustLocationAndGoPositionsBy:(CGFloat)dX {
  // Ensure that the 'Go' button is in its proper place.
  NSRect goFrame = [goButton_ frame];
  NSRect locationFrame = [locationBar_ frame];
  CGFloat rightDelta = (locationFrame.origin.x + NSWidth(locationFrame)) -
      goFrame.origin.x;
  if (rightDelta != 0.0)
    [goButton_ setFrame:NSOffsetRect(goFrame, rightDelta, 0)];

  [goButton_ setFrame:NSOffsetRect([goButton_ frame], dX, 0)];
  locationFrame.size.width += dX;
  [locationBar_ setFrame:locationFrame];
}

- (NSRect)starButtonInWindowCoordinates {
  return [starButton_ convertRect:[starButton_ bounds] toView:nil];
}

- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight {
  // With no toolbar, just ignore the compression.
  return hasToolbar_ ? kBaseToolbarHeight - compressByHeight :
                       NSHeight([locationBar_ frame]);
}

- (void)setDividerOpacity:(CGFloat)opacity {
  BackgroundGradientView* view = [self backgroundGradientView];
  [view setShowsDivider:(opacity > 0 ? YES : NO)];

  // We may not have a toolbar view (e.g., popup windows only have a location
  // bar).
  if ([view isKindOfClass:[ToolbarView class]]) {
    ToolbarView* toolbarView = (ToolbarView*)view;
    [toolbarView setDividerOpacity:opacity];
  }
}

- (BrowserActionsController*)browserActionsController {
  return browserActionsController_.get();
}

- (NSString*)view:(NSView*)view
 stringForToolTip:(NSToolTipTag)tag
            point:(NSPoint)point
         userData:(void*)userData {
  DCHECK(view == goButton_);

  // Following chrome/browser/views/go_button.cc: GoButton::GetTooltipText()

  // Is it currently 'stop'?
  if ([goButton_ tag] == IDC_STOP) {
    return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STOP);
  }

  // It is 'go', so see what it would do...

  // Fetch the EditView and EditModel
  LocationBar* locationBar = [self locationBarBridge];
  DCHECK(locationBar);
  AutocompleteEditView* editView = locationBar->location_entry();
  DCHECK(editView);
  AutocompleteEditModel* editModel = editView->model();
  DCHECK(editModel);

  std::wstring currentText(editView->GetText());
  if (currentText.empty()) {
    return nil;
  }
  string16 currentText16(WideToUTF16Hack(currentText));

  // It is simply an url it is gonna go to, build the tip with the info.
  if (editModel->CurrentTextIsURL()) {
    return l10n_util::GetNSStringF(IDS_TOOLTIP_GO_SITE, currentText16);
  }

  // Build the tip based on what provide/template it will get.
  std::wstring keyword(editModel->keyword());
  TemplateURLModel* template_url_model =
      editModel->profile()->GetTemplateURLModel();
  const TemplateURL* provider =
      (keyword.empty() || editModel->is_keyword_hint()) ?
      template_url_model->GetDefaultSearchProvider() :
      template_url_model->GetTemplateURLForKeyword(keyword);
  if (!provider)
    return nil;
  std::wstring shortName(provider->AdjustedShortNameForLocaleDirection());
  return l10n_util::GetNSStringF(IDS_TOOLTIP_GO_SEARCH,
                                 WideToUTF16(shortName), currentText16);

}

- (gfx::Rect)locationStackBounds {
  // The number of pixels from the left or right edges of the location stack to
  // "just inside the visible borders".  When the omnibox bubble contents are
  // aligned with this, the visible borders tacked on to the outsides will line
  // up with the visible borders on the location stack.
  const int kLocationStackEdgeWidth = 2;

  const NSRect locationFrame = [locationBar_ frame];

  // Expand to include star and go buttons.  Including the widths
  // rather that calculating from their current placement because this
  // method can be called while the resize is still rearranging the
  // views involved.
  const CGFloat minX = NSMinX(locationFrame) - NSWidth([starButton_ frame]);
  const CGFloat maxX = NSMaxX(locationFrame) + NSWidth([goButton_ frame]);

  NSRect r = NSMakeRect(minX, NSMinY(locationFrame), maxX - minX,
                        NSHeight(locationFrame));
  gfx::Rect stack_bounds(
      NSRectToCGRect([[self view] convertRect:r toView:nil]));
  // Inset the bounds to just inside the visible edges (see comment above).
  stack_bounds.Inset(kLocationStackEdgeWidth, 0);
  return stack_bounds;
}

// (URLDropTargetController protocol)
- (void)dropURLs:(NSArray*)urls inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripController|. I'll refactor this soon to make it common and expand
  // its capabilities (e.g., allow text DnD).
  if ([urls count] < 1) {
    NOTREACHED();
    return;
  }

  // TODO(viettrungluu): dropping multiple URLs?
  if ([urls count] > 1)
    NOTIMPLEMENTED();

  // Get the first URL and fix it up.
  GURL url(URLFixerUpper::FixupURL(
      base::SysNSStringToUTF8([urls objectAtIndex:0]), std::string()));

  browser_->GetSelectedTabContents()->OpenURL(url, GURL(), CURRENT_TAB,
                                              PageTransition::TYPED);
}

// (URLDropTargetController protocol)
- (void)indicateDropURLsInView:(NSView*)view at:(NSPoint)point {
  // Do nothing.
}

// (URLDropTargetController protocol)
- (void)hideDropURLsIndicatorInView:(NSView*)view {
  // Do nothing.
}

@end
