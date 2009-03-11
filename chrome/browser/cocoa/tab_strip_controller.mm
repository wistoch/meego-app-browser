// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_controller.h"

#import "base/sys_string_conversions.h"
#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_cell.h"
#import "chrome/browser/cocoa/tab_contents_controller.h"
#import "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/tabs/tab_strip_model.h"

// The amount of overlap tabs have in their button frames.
const short kTabOverlap = 16;

// The private methods the brige object needs from the controller.
@interface TabStripController(BridgeMethods)
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground;
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture;
- (void)tabDetachedWithContents:(TabContents*)contents
                        atIndex:(NSInteger)index;
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index;
@end

// A C++ bridge class to handle receiving notifications from the C++ tab
// strip model. Doesn't do much on its own, just sends everything straight
// to the Cocoa controller.
class TabStripBridge : public TabStripModelObserver {
 public:
  TabStripBridge(TabStripModel* model, TabStripController* controller);
  ~TabStripBridge();

  // Overridden from TabStripModelObserver
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index);
  virtual void TabStripEmpty();

 private:
  TabStripController* controller_;  // weak, owns me
  TabStripModel* model_;  // weak, owned by Browser
};

@implementation TabStripController

- (id)initWithView:(TabStripView*)view
          tabModel:(TabStripModel*)tabModel
      toolbarModel:(ToolbarModel*)toolbarModel
          commands:(CommandUpdater*)commands {
  DCHECK(view && tabModel && toolbarModel);
  if ((self = [super init])) {
    tabView_ = view;
    tabModel_ = tabModel;
    toolbarModel_ = toolbarModel;
    commands_ = commands;
    bridge_ = new TabStripBridge(tabModel, self);
    tabControllerArray_ = [[NSMutableArray alloc] init];

    // Create the new tab button separate from the nib so we can make sure
    // it's always at the end of the subview list.
    NSImage* image = [NSImage imageNamed:@"newtab"];
    NSRect frame = NSMakeRect(0, 0, [image size].width, [image size].height);
    newTabButton_ = [[NSButton alloc] initWithFrame:frame];
    [newTabButton_ setImage:image];
    [newTabButton_ setImagePosition:NSImageOnly];
    [newTabButton_ setTarget:nil];
    [newTabButton_ setAction:@selector(commandDispatch:)];
    [newTabButton_ setTag:IDC_NEW_TAB];
    [newTabButton_ setButtonType:NSMomentaryPushInButton];
    [newTabButton_ setBordered:NO];
  }
  return self;
}

- (void)dealloc {
  delete bridge_;
  [tabControllerArray_ release];
  [newTabButton_ release];
  [super dealloc];
}

// Finds the associated TabContentsController at the given |index| and swaps
// out the sole child of the contentArea to display its contents.
- (void)swapInTabAtIndex:(NSInteger)index {
  TabContentsController* controller = [tabControllerArray_ objectAtIndex:index];

  // Resize the new view to fit the window
  NSView* contentView = [[tabView_ window] contentView];
  NSView* newView = [controller view];
  NSRect frame = [contentView bounds];
  frame.size.height -= 14.0;
  [newView setFrame:frame];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of the contentView because we're the one who put it there. There
  // may not be any children in the case of a tab that's been closed, in
  // which case there's no swapping going on.
  NSArray* subviews = [contentView subviews];
  if ([subviews count]) {
    NSView* oldView = [subviews objectAtIndex:0];
    [contentView replaceSubview:oldView with:newView];
  } else {
    [contentView addSubview:newView];
  }
}

// Create a new tab view and set its cell correctly so it draws the way we
// want it to.
- (NSButton*)newTabWithFrame:(NSRect)frame {
  NSButton* button = [[[NSButton alloc] initWithFrame:frame] autorelease];
  TabCell* cell = [[[TabCell alloc] init] autorelease];
  [button setCell:cell];
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:@"New Tab"];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTarget:self];
  [button setAction:@selector(selectTab:)];

  return button;
}

// Returns the number of tab buttons in the tab strip by counting the children.
// Recall the last view is the "new tab" button, so the number of tabs is one
// less than the count.
- (NSInteger)numberOfTabViews {
  return [[tabView_ subviews] count] - 1;
}

// Returns the index of the subview |view|. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view {
  NSInteger index = -1;
  const int numSubviews = [self numberOfTabViews];
  for (int i = 0; i < numSubviews; i++) {
    if ([[tabView_ subviews] objectAtIndex:i] == view)
      index = i;
  }
  return index;
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  int index = [self indexForTabView:sender];  // for testing...
  if (index >= 0 && tabModel_->ContainsIndex(index))
    tabModel_->SelectTabContentsAt(index, true);
}

// Return the frame for a new tab that will go to the immediate right of the
// tab at |index|. If |index| is 0, this will be the first tab, indented so
// as to not cover the window controls.
- (NSRect)frameForNewTabAtIndex:(NSInteger)index {
  const short kIndentLeavingSpaceForControls = 66;
  const short kNewTabWidth = 160;

  short xOffset = kIndentLeavingSpaceForControls;
  if (index > 0) {
    NSRect previousTab = [[[tabView_ subviews] objectAtIndex:index - 1] frame];
    xOffset = NSMaxX(previousTab) - kTabOverlap;
  }

  return NSMakeRect(xOffset, 0, kNewTabWidth, [tabView_ frame].size.height);
}

// Handles setting the title of the tab based on the given |contents|. Uses
// a canned string if |contents| is NULL.
- (void)setTabTitle:(NSButton*)tab withContents:(TabContents*)contents {
  NSString* titleString = nil;
  if (contents)
    titleString = base::SysUTF16ToNSString(contents->GetTitle());
  if (![titleString length])
    titleString = NSLocalizedString(@"untitled", nil);
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
                                               bundle:nil
                                             contents:contents
                                             commands:commands_
                                         toolbarModel:toolbarModel_]
          autorelease];
  [tabControllerArray_ insertObject:contentsController atIndex:index];

  // Remove the new tab button so the only views present are the tabs,
  // we'll add it back when we're done
  [newTabButton_ removeFromSuperview];

  // Make a new tab view and add it to the strip.
  // TODO(pinkerton): move everyone else over and animate. Also will need to
  // move the "add tab" button over.
  NSRect newTabFrame = [self frameForNewTabAtIndex:index];
  NSButton* newView = [self newTabWithFrame:newTabFrame];
  [tabView_ addSubview:newView];

  [self setTabTitle:newView withContents:contents];

  // Add the new tab button back in to the right of the last tab.
  const NSInteger kNewTabXOffset = 10;
  NSRect lastTab =
    [[[tabView_ subviews] objectAtIndex:[[tabView_ subviews] count] - 1] frame];
  NSInteger maxRightEdge = NSMaxX(lastTab);
  NSRect newTabButtonFrame = [newTabButton_ frame];
  newTabButtonFrame.origin.x = maxRightEdge + kNewTabXOffset;
  [newTabButton_ setFrame:newTabButtonFrame];
  [tabView_ addSubview:newTabButton_];

  // Select the newly created tab if in the foreground
  if (inForeground)
    [self swapInTabAtIndex:index];
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  // De-select all other tabs and select the new tab.
  const int numSubviews = [self numberOfTabViews];
  for (int i = 0; i < numSubviews; i++) {
    NSButton* current = [[tabView_ subviews] objectAtIndex:i];
    [current setState:(i == index) ? NSOnState : NSOffState];
  }

  // Tell the new tab contents it is about to become the selected tab. Here it
  // can do things like make sure the toolbar is up to date.
  TabContentsController* newController =
      [tabControllerArray_ objectAtIndex:index];
  [newController willBecomeSelectedTab];

  // Swap in the contents for the new tab
  [self swapInTabAtIndex:index];
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
  [tabControllerArray_ removeObjectAtIndex:index];

  // Remove the |index|th view from the tab strip
  NSView* tab = [[tabView_ subviews] objectAtIndex:index];
  NSInteger tabWidth = [tab frame].size.width;
  [tab removeFromSuperview];

  // Move all the views to the right the width of the tab that was closed.
  // TODO(pinkerton): Animate!
  const int numSubviews = [[tabView_ subviews] count];
  for (int i = index; i < numSubviews; i++) {
    NSView* curr = [[tabView_ subviews] objectAtIndex:i];
    NSRect newFrame = [curr frame];
    newFrame.origin.x -= tabWidth - kTabOverlap;
    [curr setFrame:newFrame];
  }
}

// Called when a notification is received from the model that the given tab
// has been updated.
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index {
  NSButton* tab = [[tabView_ subviews] objectAtIndex:index];
  [self setTabTitle:tab withContents:contents];

  TabContentsController* updatedController =
      [tabControllerArray_ objectAtIndex:index];
  [updatedController tabDidChange:contents];
}

- (LocationBar*)locationBar {
  TabContentsController* selectedController =
      [tabControllerArray_ objectAtIndex:tabModel_->selected_index()];
  return [selectedController locationBar];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  // TODO(pinkerton): OS_WIN maintains this, but I'm not sure why. It's
  // available by querying the model, which we have available to us.
  currentTab_ = tab;

  // tell the appropriate controller to update its state. |shouldRestore| being
  // YES means we're going back to this tab and should put back any state
  // associated with it.
  TabContentsController* controller =
      [tabControllerArray_ objectAtIndex:tabModel_->GetIndexOfTabContents(tab)];
  [controller updateToolbarWithContents:shouldRestore ? tab : nil];
}

- (void)setStarredState:(BOOL)isStarred {
  TabContentsController* selectedController =
      [tabControllerArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect {
  int selectedIndex = tabModel_->selected_index();
  if (selectedIndex == TabStripModel::kNoTab) {
    // When the window is initially being constructed, there may be no currently
    // selected tab, so pick the first one. If there aren't any, just bail with
    // an empty rect.
    selectedIndex = 0;
  }
  TabContentsController* selectedController =
      [tabControllerArray_ objectAtIndex:selectedIndex];
  if (!selectedController)
    return NSZeroRect;
  return [selectedController growBoxRect];
}

// Called to tell the selected tab to update its loading state.
- (void)setIsLoading:(BOOL)isLoading {
  // TODO(pinkerton): update the favicon on the selected tab view to/from
  // a spinner?

  TabContentsController* selectedController =
      [tabControllerArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController setIsLoading:isLoading];
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar {
  TabContentsController* selectedController =
      [tabControllerArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController focusLocationBar];
}

@end

//--------------------------------------------------------------------------

TabStripBridge::TabStripBridge(TabStripModel* model,
                               TabStripController* controller)
    : controller_(controller), model_(model) {
  // Register to be a listener on the model so we can get updates and tell
  // the TabStripController about them.
  model_->AddObserver(this);
}

TabStripBridge::~TabStripBridge() {
  // Remove ourselves from receiving notifications.
  model_->RemoveObserver(this);
}

void TabStripBridge::TabInsertedAt(TabContents* contents,
                                   int index,
                                   bool foreground) {
  [controller_ insertTabWithContents:contents
                             atIndex:index
                        inForeground:foreground];
}

void TabStripBridge::TabDetachedAt(TabContents* contents, int index) {
  [controller_ tabDetachedWithContents:contents atIndex:index];
}

void TabStripBridge::TabSelectedAt(TabContents* old_contents,
                                   TabContents* new_contents,
                                   int index,
                                   bool user_gesture) {
  [controller_ selectTabWithContents:new_contents
                    previousContents:old_contents
                             atIndex:index
                         userGesture:user_gesture];
}

void TabStripBridge::TabMoved(TabContents* contents,
                              int from_index,
                              int to_index) {
  NOTIMPLEMENTED();
}

void TabStripBridge::TabChangedAt(TabContents* contents, int index) {
  [controller_ tabChangedWithContents:contents atIndex:index];
}

void TabStripBridge::TabStripEmpty() {
  NOTIMPLEMENTED();
}
