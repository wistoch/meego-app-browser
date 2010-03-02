// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_window.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/test_event_utils.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Just like a BookmarkBarController but openURL: is stubbed out.
@interface BookmarkBarControllerNoOpen : BookmarkBarController {
 @public
  std::vector<GURL> urls_;
  std::vector<WindowOpenDisposition> dispositions_;
}
@end

@implementation BookmarkBarControllerNoOpen
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition {
  urls_.push_back(url);
  dispositions_.push_back(disposition);
}
- (void)clear {
  urls_.clear();
  dispositions_.clear();
}
@end


// NSCell that is pre-provided with a desired size that becomes the
// return value for -(NSSize)cellSize:.
@interface CellWithDesiredSize : NSCell {
 @private
  NSSize cellSize_;
}
@property(readonly) NSSize cellSize;
@end

@implementation CellWithDesiredSize

@synthesize cellSize = cellSize_;

- (id)initTextCell:(NSString*)string desiredSize:(NSSize)size {
  if ((self = [super initTextCell:string])) {
    cellSize_ = size;
  }
  return self;
}

@end

// Remember the number of times we've gotten a frameDidChange notification.
@interface BookmarkBarControllerTogglePong : BookmarkBarControllerNoOpen {
 @private
  int toggles_;
}
@property (readonly) int toggles;
@end

@implementation BookmarkBarControllerTogglePong

@synthesize toggles = toggles_;

- (void)frameDidChange {
  toggles_++;
}

@end

class FakeTheme : public ThemeProvider {
 public:
  FakeTheme(NSColor* color) : color_(color) { }
  scoped_nsobject<NSColor> color_;

  virtual void Init(Profile* profile) { }
  virtual SkBitmap* GetBitmapNamed(int id) const { return nil; }
  virtual SkColor GetColor(int id) const { return SkColor(); }
  virtual bool GetDisplayProperty(int id, int* result) const { return false; }
  virtual bool ShouldUseNativeFrame() const { return false; }
  virtual bool HasCustomImage(int id) const { return false; }
  virtual RefCountedMemory* GetRawData(int id) const { return NULL; }
  virtual NSImage* GetNSImageNamed(int id, bool allow_default) const {
    return nil;
  }
  virtual NSColor* GetNSImageColorNamed(int id, bool allow_default) const {
    return nil;
  }
  virtual NSColor* GetNSColor(int id, bool allow_default) const {
    return color_.get();
  }
  virtual NSColor* GetNSColorTint(int id, bool allow_default) const {
    return nil;
  }
  virtual NSGradient* GetNSGradient(int id) const {
    return nil;
  }
};


namespace {

static const int kContentAreaHeight = 500;
static const int kInfoBarViewHeight = 30;

class BookmarkBarControllerTest : public CocoaTest {
 public:
  BookmarkBarControllerTest() {
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    NSRect parent_frame = NSMakeRect(0, 0, 800, 50);
    parent_view_.reset([[NSView alloc] initWithFrame:parent_frame]);
    [parent_view_ setHidden:YES];
    bar_.reset(
      [[BookmarkBarControllerNoOpen alloc]
          initWithBrowser:helper_.browser()
             initialWidth:NSWidth(parent_frame)
                 delegate:nil
           resizeDelegate:resizeDelegate_.get()]);

    InstallAndToggleBar(bar_.get());

    // Create a menu/item to act like a sender
    menu_.reset([[BookmarkMenu alloc] initWithTitle:@"I_dont_care"]);
    menu_item_.reset([[NSMenuItem alloc]
                       initWithTitle:@"still_dont_care"
                              action:NULL
                       keyEquivalent:@""]);
    cell_.reset([[NSButtonCell alloc] init]);
    [menu_item_ setMenu:menu_.get()];
    [menu_ setDelegate:cell_.get()];
  }

  void InstallAndToggleBar(BookmarkBarController* bar) {
    // Force loading of the nib.
    [bar view];
    // Awkwardness to look like we've been installed.
    [parent_view_ addSubview:[bar view]];
    NSRect frame = [[[bar view] superview] frame];
    frame.origin.y = 100;
    [[[bar view] superview] setFrame:frame];

    // Make sure it's on in a window so viewDidMoveToWindow is called
    [[test_window() contentView] addSubview:parent_view_];

    // Make sure it's open so certain things aren't no-ops.
    [bar updateAndShowNormalBar:YES
                showDetachedBar:NO
                  withAnimation:NO];
  }

  // Return a menu item that points to the right URL.
  NSMenuItem* ItemForBookmarkBarMenu(GURL& gurl) {
    node_.reset(new BookmarkNode(gurl));
    [menu_ setRepresentedObject:[NSValue valueWithPointer:node_.get()]];
    return menu_item_;
  }

  // Does NOT take ownership of node.
  NSMenuItem* ItemForBookmarkBarMenu(const BookmarkNode* node) {
    [menu_ setRepresentedObject:[NSValue valueWithPointer:node]];
    return menu_item_;
  }

  BrowserTestHelper helper_;
  scoped_nsobject<NSView> parent_view_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  scoped_nsobject<BookmarkBarControllerNoOpen> bar_;
  scoped_nsobject<BookmarkMenu> menu_;
  scoped_nsobject<NSMenuItem> menu_item_;
  scoped_nsobject<NSButtonCell> cell_;
  scoped_ptr<BookmarkNode> node_;
};

TEST_F(BookmarkBarControllerTest, ShowWhenShowBookmarkBarTrue) {
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  EXPECT_GT([resizeDelegate_ height], 0);
  EXPECT_GT([[bar_ view] frame].size.height, 0);
}

TEST_F(BookmarkBarControllerTest, HideWhenShowBookmarkBarFalse) {
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_FALSE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  EXPECT_EQ(0, [resizeDelegate_ height]);
  EXPECT_EQ(0, [[bar_ view] frame].size.height);
}

TEST_F(BookmarkBarControllerTest, HideWhenShowBookmarkBarTrueButDisabled) {
  [bar_ setBookmarkBarEnabled:NO];
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  EXPECT_EQ(0, [resizeDelegate_ height]);
  EXPECT_EQ(0, [[bar_ view] frame].size.height);
}

TEST_F(BookmarkBarControllerTest, ShowOnNewTabPage) {
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_FALSE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_TRUE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  EXPECT_GT([resizeDelegate_ height], 0);
  EXPECT_GT([[bar_ view] frame].size.height, 0);

  // Make sure no buttons fall off the bar, either now or when resized
  // bigger or smaller.
  CGFloat sizes[] = { 300.0, -100.0, 200.0, -420.0 };
  CGFloat previousX = 0.0;
  for (unsigned x = 0; x < arraysize(sizes); x++) {
    // Confirm the buttons moved from the last check (which may be
    // init but that's fine).
    CGFloat newX = [[bar_ offTheSideButton] frame].origin.x;
    EXPECT_NE(previousX, newX);
    previousX = newX;

    // Confirm the buttons have a reasonable bounds. Recall that |-frame|
    // returns rectangles in the superview's coordinates.
    NSRect buttonViewFrame =
        [[bar_ buttonView] convertRect:[[bar_ buttonView] frame]
                              fromView:[[bar_ buttonView] superview]];
    EXPECT_EQ([bar_ buttonView], [[bar_ offTheSideButton] superview]);
    EXPECT_TRUE(NSContainsRect(buttonViewFrame,
                               [[bar_ offTheSideButton] frame]));
    EXPECT_EQ([bar_ buttonView], [[bar_ otherBookmarksButton] superview]);
    EXPECT_TRUE(NSContainsRect(buttonViewFrame,
                               [[bar_ otherBookmarksButton] frame]));

    // Now move them implicitly.
    // We confirm FrameChangeNotification works in the next unit test;
    // we simply assume it works here to resize or reposition the
    // buttons above.
    NSRect frame = [[bar_ view] frame];
    frame.size.width += sizes[x];
    [[bar_ view] setFrame:frame];
  }
}

// Test whether |-updateAndShowNormalBar:...| sets states as we expect. Make
// sure things don't crash.
TEST_F(BookmarkBarControllerTest, StateChanges) {
  // First, go in one-at-a-time cycle.
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kHiddenState, [bar_ visualState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kDetachedState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);

  // Now try some "jumps".
  for (int i = 0; i < 2; i++) {
    [bar_ updateAndShowNormalBar:NO
                 showDetachedBar:NO
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kHiddenState, [bar_ visualState]);
    EXPECT_FALSE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
    [bar_ updateAndShowNormalBar:YES
                 showDetachedBar:YES
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
  }

  // Now try some "jumps".
  for (int i = 0; i < 2; i++) {
    [bar_ updateAndShowNormalBar:YES
                 showDetachedBar:NO
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
    [bar_ updateAndShowNormalBar:NO
                 showDetachedBar:YES
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kDetachedState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
  }
}

// Make sure we're watching for frame change notifications.
TEST_F(BookmarkBarControllerTest, FrameChangeNotification) {
  scoped_nsobject<BookmarkBarControllerTogglePong> bar;
  bar.reset(
    [[BookmarkBarControllerTogglePong alloc]
          initWithBrowser:helper_.browser()
             initialWidth:100  // arbitrary
                 delegate:nil
           resizeDelegate:resizeDelegate_.get()]);
  InstallAndToggleBar(bar.get());

  // Send a frame did change notification for the pong's view.
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSViewFrameDidChangeNotification
                  object:[bar view]];

  EXPECT_GT([bar toggles], 0);
}

// Confirm off the side button only enabled when reasonable.
TEST_F(BookmarkBarControllerTest, OffTheSideButtonHidden) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  [bar_ loaded:model];
  EXPECT_TRUE([bar_ offTheSideButtonIsHidden]);

  for (int i = 0; i < 2; i++) {
    model->SetURLStarred(GURL("http://www.foo.com"), L"small", true);
    EXPECT_TRUE([bar_ offTheSideButtonIsHidden]);
  }

  for (int i = 0; i < 20; i++) {
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    model->AddURL(parent, parent->GetChildCount(),
                  L"super duper wide title",
                  GURL("http://superfriends.hall-of-justice.edu"));
  }
  EXPECT_FALSE([bar_ offTheSideButtonIsHidden]);
}

TEST_F(BookmarkBarControllerTest, TagMap) {
  int64 ids[] = { 1, 3, 4, 40, 400, 4000, 800000000, 2, 123456789 };
  std::vector<int32> tags;

  // Generate some tags
  for (unsigned int i = 0; i < arraysize(ids); i++) {
    tags.push_back([bar_ menuTagFromNodeId:ids[i]]);
  }

  // Confirm reverse mapping.
  for (unsigned int i = 0; i < arraysize(ids); i++) {
    EXPECT_EQ(ids[i], [bar_ nodeIdFromMenuTag:tags[i]]);
  }

  // Confirm uniqueness.
  std::sort(tags.begin(), tags.end());
  for (unsigned int i=0; i<(tags.size()-1); i++) {
    EXPECT_NE(tags[i], tags[i+1]);
  }
}

TEST_F(BookmarkBarControllerTest, MenuForFolderNode) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  // First make sure something (e.g. "(empty)" string) is always present.
  NSMenu* menu = [bar_ menuForFolderNode:model->GetBookmarkBarNode()];
  EXPECT_GT([menu numberOfItems], 0);

  // Test two bookmarks.
  GURL gurl("http://www.foo.com");
  model->SetURLStarred(gurl, L"small", true);
  model->SetURLStarred(GURL("http://www.cnn.com"), L"bigger title", true);
  menu = [bar_ menuForFolderNode:model->GetBookmarkBarNode()];
  EXPECT_EQ([menu numberOfItems], 2);
  NSMenuItem *item = [menu itemWithTitle:@"bigger title"];
  EXPECT_TRUE(item);
  item = [menu itemWithTitle:@"small"];
  EXPECT_TRUE(item);
  if (item) {
    int64 tag = [bar_ nodeIdFromMenuTag:[item tag]];
    const BookmarkNode* node = model->GetNodeByID(tag);
    EXPECT_TRUE(node);
    EXPECT_EQ(gurl, node->GetURL());
  }

  // Test with an actual folder as well
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* folder = model->AddGroup(parent,
                                               parent->GetChildCount(),
                                               L"group");
  model->AddURL(folder, folder->GetChildCount(),
                L"f1", GURL("http://framma-lamma.com"));
  model->AddURL(folder, folder->GetChildCount(),
                L"f2", GURL("http://framma-lamma-ding-dong.com"));
  menu = [bar_ menuForFolderNode:model->GetBookmarkBarNode()];
  EXPECT_EQ([menu numberOfItems], 3);

  item = [menu itemWithTitle:@"group"];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item hasSubmenu]);
  NSMenu *submenu = [item submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ(2, [submenu numberOfItems]);
  EXPECT_TRUE([submenu itemWithTitle:@"f1"]);
  EXPECT_TRUE([submenu itemWithTitle:@"f2"]);
}

// Confirm openBookmark: forwards the request to the controller's delegate
TEST_F(BookmarkBarControllerTest, OpenBookmark) {
  GURL gurl("http://walla.walla.ding.dong.com");
  scoped_ptr<BookmarkNode> node(new BookmarkNode(gurl));

  scoped_nsobject<BookmarkButtonCell> cell([[BookmarkButtonCell alloc] init]);
  [cell setBookmarkNode:node.get()];
  scoped_nsobject<BookmarkButton> button([[BookmarkButton alloc] init]);
  [button setCell:cell.get()];
  [cell setRepresentedObject:[NSValue valueWithPointer:node.get()]];

  [bar_ openBookmark:button];
  EXPECT_EQ(bar_.get()->urls_[0], node->GetURL());
  EXPECT_EQ(bar_.get()->dispositions_[0], CURRENT_TAB);
}

// Confirm opening of bookmarks works from the menus (different
// dispositions than clicking on the button).
TEST_F(BookmarkBarControllerTest, OpenBookmarkFromMenus) {
  const char* urls[] = { "http://walla.walla.ding.dong.com",
                         "http://i_dont_know.com",
                         "http://cee.enn.enn.dot.com" };
  SEL selectors[] = { @selector(openBookmarkInNewForegroundTab:),
                      @selector(openBookmarkInNewWindow:),
                      @selector(openBookmarkInIncognitoWindow:) };
  WindowOpenDisposition dispositions[] = { NEW_FOREGROUND_TAB,
                                           NEW_WINDOW,
                                           OFF_THE_RECORD };
  for (unsigned int i = 0;
       i < sizeof(dispositions)/sizeof(dispositions[0]);
       i++) {
    GURL gurl(urls[i]);
    [bar_ performSelector:selectors[i]
               withObject:ItemForBookmarkBarMenu(gurl)];
    EXPECT_EQ(bar_.get()->urls_[0], gurl);
    EXPECT_EQ(bar_.get()->dispositions_[0], dispositions[i]);
    [bar_ clear];
  }
}

TEST_F(BookmarkBarControllerTest, TestAddRemoveAndClear) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  NSView* buttonView = [bar_ buttonView];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  unsigned int initial_subview_count = [[buttonView subviews] count];

  // Make sure a redundant call doesn't choke
  [bar_ clearBookmarkBar];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  EXPECT_EQ(initial_subview_count, [[buttonView subviews] count]);

  GURL gurl1("http://superfriends.hall-of-justice.edu");
  // Short titles increase the chances of this test succeeding if the view is
  // narrow.
  // TODO(viettrungluu): make the test independent of window/view size, font
  // metrics, button size and spacing, and everything else.
  std::wstring title1(L"x");
  model->SetURLStarred(gurl1, title1, true);
  EXPECT_EQ(1U, [[bar_ buttons] count]);
  EXPECT_EQ(1+initial_subview_count, [[buttonView subviews] count]);

  GURL gurl2("http://legion-of-doom.gov");
  std::wstring title2(L"y");
  model->SetURLStarred(gurl2, title2, true);
  EXPECT_EQ(2U, [[bar_ buttons] count]);
  EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);

  for (int i = 0; i < 3; i++) {
    // is_starred=false --> remove the bookmark
    model->SetURLStarred(gurl2, title2, false);
    EXPECT_EQ(1U, [[bar_ buttons] count]);
    EXPECT_EQ(1+initial_subview_count, [[buttonView subviews] count]);

    // and bring it back
    model->SetURLStarred(gurl2, title2, true);
    EXPECT_EQ(2U, [[bar_ buttons] count]);
    EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);
  }

  [bar_ clearBookmarkBar];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  EXPECT_EQ(initial_subview_count, [[buttonView subviews] count]);

  // Explicit test of loaded: since this is a convenient spot
  [bar_ loaded:model];
  EXPECT_EQ(2U, [[bar_ buttons] count]);
  EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);
}

// Make sure that each button we add marches to the right and does not
// overlap with the previous one.
TEST_F(BookmarkBarControllerTest, TestButtonMarch) {
  scoped_nsobject<NSMutableArray> cells([[NSMutableArray alloc] init]);

  CGFloat widths[] = { 10, 10, 100, 10, 500, 500, 80000, 60000, 1, 345 };
  for (unsigned int i = 0; i < arraysize(widths); i++) {
    NSCell* cell = [[CellWithDesiredSize alloc]
                     initTextCell:@"foo"
                      desiredSize:NSMakeSize(widths[i], 30)];
    [cells addObject:cell];
    [cell release];
  }

  int x_offset = 0;
  CGFloat x_end = x_offset;  // end of the previous button
  for (unsigned int i = 0; i < arraysize(widths); i++) {
    NSRect r = [bar_ frameForBookmarkButtonFromCell:[cells objectAtIndex:i]
                                            xOffset:&x_offset];
    EXPECT_GE(r.origin.x, x_end);
    x_end = NSMaxX(r);
  }
}

TEST_F(BookmarkBarControllerTest, CheckForGrowth) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com");
  std::wstring title1(L"x");
  model->SetURLStarred(gurl1, title1, true);

  GURL gurl2("http://www.google.com/blah");
  std::wstring title2(L"y");
  model->SetURLStarred(gurl2, title2, true);

  EXPECT_EQ(2U, [[bar_ buttons] count]);
  CGFloat width_1 = [[[bar_ buttons] objectAtIndex:0] frame].size.width;
  CGFloat x_2 = [[[bar_ buttons] objectAtIndex:1] frame].origin.x;

  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  [[first cell] setTitle:@"This is a really big title; watch out mom!"];
  [bar_ checkForBookmarkButtonGrowth:first];

  // Make sure the 1st button is now wider, the 2nd one is moved over,
  // and they don't overlap.
  NSRect frame_1 = [[[bar_ buttons] objectAtIndex:0] frame];
  NSRect frame_2 = [[[bar_ buttons] objectAtIndex:1] frame];
  EXPECT_GT(frame_1.size.width, width_1);
  EXPECT_GT(frame_2.origin.x, x_2);
  EXPECT_GE(frame_2.origin.x, frame_1.origin.x + frame_1.size.width);
}

TEST_F(BookmarkBarControllerTest, DeleteBookmark) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  const char* urls[] = { "https://secret.url.com",
                         "http://super.duper.web.site.for.doodz.gov",
                         "http://www.foo-bar-baz.com/" };
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  for (unsigned int i = 0; i < arraysize(urls); i++) {
    model->AddURL(parent, parent->GetChildCount(),
                  L"title", GURL(urls[i]));
  }
  EXPECT_EQ(3, parent->GetChildCount());
  const BookmarkNode* middle_node = parent->GetChild(1);

  NSMenuItem* item = ItemForBookmarkBarMenu(middle_node);
  [bar_ deleteBookmark:item];
  EXPECT_EQ(2, parent->GetChildCount());
  EXPECT_EQ(parent->GetChild(0)->GetURL(), GURL(urls[0]));
  // node 2 moved into spot 1
  EXPECT_EQ(parent->GetChild(1)->GetURL(), GURL(urls[2]));
}

TEST_F(BookmarkBarControllerTest, OpenAllBookmarks) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  // { one, { two-one, two-two }, three }
  model->AddURL(parent, parent->GetChildCount(),
                L"title", GURL("http://one.com"));
  const BookmarkNode* folder = model->AddGroup(parent,
                                               parent->GetChildCount(),
                                               L"group");
  model->AddURL(folder, folder->GetChildCount(),
                L"title", GURL("http://two-one.com"));
  model->AddURL(folder, folder->GetChildCount(),
                L"title", GURL("http://two-two.com"));
  model->AddURL(parent, parent->GetChildCount(),
                L"title", GURL("https://three.com"));

  // Our first OpenAll... is from the bar itself.
  [bar_ openAllBookmarks:ItemForBookmarkBarMenu(parent)];
  EXPECT_EQ(bar_.get()->urls_.size(), 4U);
  EXPECT_EQ(bar_.get()->dispositions_.size(), 4U);

  // I can't use EXPECT_EQ() here since the macro can't expand
  // properly (no way to print the value of an iterator).
  std::vector<GURL>::iterator i;
  std::vector<GURL>::iterator begin = bar_.get()->urls_.begin();
  std::vector<GURL>::iterator end = bar_.get()->urls_.end();
  i = find(begin, end, GURL("http://two-one.com"));
  EXPECT_FALSE(i == end);
  i = find(begin, end, GURL("https://three.com"));
  EXPECT_FALSE(i == end);
  i = find(begin, end, GURL("https://will-not-be-found.com"));
  EXPECT_TRUE(i == end);

  EXPECT_EQ(bar_.get()->dispositions_[3], NEW_BACKGROUND_TAB);

  // Now try an OpenAll... from a folder node.
  bar_.get()->urls_.clear();
  bar_.get()->dispositions_.clear();
  [bar_ openAllBookmarks:ItemForBookmarkBarMenu(folder)];

  EXPECT_EQ(bar_.get()->urls_.size(), 2U);
  EXPECT_EQ(bar_.get()->dispositions_.size(), 2U);
}

// TODO(jrg): write a test to confirm that nodeFavIconLoaded calls
// checkForBookmarkButtonGrowth:.

TEST_F(BookmarkBarControllerTest, Cell) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  [bar_ loaded:model];

  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddURL(parent, parent->GetChildCount(),
                L"supertitle",
                GURL("http://superfriends.hall-of-justice.edu"));
  const BookmarkNode* node = parent->GetChild(0);

  NSCell* cell = [bar_ cellForBookmarkNode:node];
  EXPECT_TRUE(cell);
  EXPECT_TRUE([[cell title] isEqual:@"supertitle"]);
  EXPECT_EQ(node, [[cell representedObject] pointerValue]);
  EXPECT_TRUE([cell menu]);

  // Empty cells have no menu.
  cell = [bar_ cellForBookmarkNode:nil];
  EXPECT_FALSE([cell menu]);
  // Even empty cells have a title (of "(empty)")
  EXPECT_TRUE([cell title]);

  // cell is autoreleased; no need to release here
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarControllerTest, Display) {
  [[bar_ view] display];
}

// Test that middle clicking on a bookmark button results in an open action.
TEST_F(BookmarkBarControllerTest, MiddleClick) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com/");
  std::wstring title1(L"x");
  model->SetURLStarred(gurl1, title1, true);

  EXPECT_EQ(1U, [[bar_ buttons] count]);
  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(first);

  [first otherMouseUp:test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0)];
  EXPECT_EQ(bar_.get()->urls_.size(), 1U);
}

TEST_F(BookmarkBarControllerTest, TestBuildOffTheSideMenu) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  NSMenu* menu = [bar_ offTheSideMenu];
  ASSERT_TRUE(menu);

  // The bookmark bar should start out with nothing.
  EXPECT_EQ(0U, [[bar_ buttons] count]);

  // Make sure things work when there's nothing. Note that there should always
  // be a blank first menu item.
  [bar_ buildOffTheSideMenu];
  EXPECT_EQ(1, [menu numberOfItems]);

  // We add lots of bookmarks. At first, we expect nothing to be added to the
  // off-the-side menu. But once they start getting added, we expect the
  // remaining ones to be added too. We expect a reasonably substantial number
  // of items to be added by the end.
  int num_off_the_side = 0;
  for (int i = 0; i < 50; i++) {
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    model->AddURL(parent, parent->GetChildCount(),
                  L"very wide title",
                  GURL("http://www.foobar.com/"));
    [bar_ buildOffTheSideMenu];

    if (num_off_the_side) {
      num_off_the_side++;
      EXPECT_EQ(1 + num_off_the_side, [menu numberOfItems]);
    } else {
      EXPECT_TRUE([menu numberOfItems] == 1 || [menu numberOfItems] == 2);
      if ([menu numberOfItems] == 2)
        num_off_the_side++;
    }
  }
  EXPECT_GE(num_off_the_side, 20);

  // Reset, and check that the built menu is "empty" again.
  [bar_ clearBookmarkBar];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  [bar_ buildOffTheSideMenu];
  EXPECT_EQ(1, [menu numberOfItems]);
}

TEST_F(BookmarkBarControllerTest, DisplaysHelpMessageOnEmpty) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  [bar_ loaded:model];
  EXPECT_FALSE([[[bar_ buttonView] noItemTextfield] isHidden]);
}

TEST_F(BookmarkBarControllerTest, HidesHelpMessageWithBookmark) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddURL(parent, parent->GetChildCount(),
                L"title", GURL("http://one.com"));

  [bar_ loaded:model];
  EXPECT_TRUE([[[bar_ buttonView] noItemTextfield] isHidden]);
}

TEST_F(BookmarkBarControllerTest, BookmarkButtonSizing) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddURL(parent, parent->GetChildCount(),
                L"title", GURL("http://one.com"));

  [bar_ loaded:model];

  // Make sure the internal bookmark button also is the correct height.
  NSArray* buttons = [bar_ buttons];
  EXPECT_GT([buttons count], 0u);
  for (NSButton* button in buttons) {
    EXPECT_FLOAT_EQ(
        bookmarks::kBookmarkBarHeight - 2 *
                    bookmarks::kBookmarkVerticalPadding,
        [button frame].size.height);
  }
}

TEST_F(BookmarkBarControllerTest, DropBookmarks) {
  const char* urls[] = {
    "http://qwantz.com",
    "http://xkcd.com",
    "javascript:alert('lolwut')"
  };
  std::wstring titles[] = {
    std::wstring(L"Philosophoraptor"),
    std::wstring(L"Can't draw"),
    std::wstring(L"Inspiration")
  };
  EXPECT_EQ(arraysize(urls), arraysize(titles));

  NSMutableArray* nsurls = [NSMutableArray arrayWithCapacity:0];
  NSMutableArray* nstitles = [NSMutableArray arrayWithCapacity:0];
  for (size_t i = 0; i < arraysize(urls); ++i) {
    [nsurls addObject:base::SysUTF8ToNSString(urls[i])];
    [nstitles addObject:base::SysWideToNSString(titles[i])];
  }

  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  [bar_ addURLs:nsurls withTitles:nstitles at:NSZeroPoint];
  EXPECT_EQ(3, parent->GetChildCount());
  for (int i = 0; i < parent->GetChildCount(); ++i) {
    EXPECT_EQ(parent->GetChild(i)->GetURL(), GURL(urls[i]));
    EXPECT_EQ(parent->GetChild(i)->GetTitle(), titles[i]);
  }
}

TEST_F(BookmarkBarControllerTest, TestButtonOrBar) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com");
  std::wstring title1(L"x");
  model->SetURLStarred(gurl1, title1, true);

  GURL gurl2("http://www.google.com/gurl_power");
  std::wstring title2(L"gurl power");
  model->SetURLStarred(gurl2, title2, true);

  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  NSButton* second = [[bar_ buttons] objectAtIndex:1];
  EXPECT_TRUE(first && second);

  NSMenuItem* menuItem = [[[first cell] menu] itemAtIndex:0];
  BookmarkNode* node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->GetBookmarkBarNode()->GetChild(0));

  menuItem = [[[second cell] menu] itemAtIndex:0];
  node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->GetBookmarkBarNode()->GetChild(1));

  menuItem = [[[bar_ view] menu] itemAtIndex:0];
  node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->GetBookmarkBarNode());
}

TEST_F(BookmarkBarControllerTest, TestMenuNodeAndDisable) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* folder = model->AddGroup(parent,
                                               parent->GetChildCount(),
                                               L"group");
  NSButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(button);

  // Confirm the menu knows which node it is talking about
  BookmarkMenu* menu = static_cast<BookmarkMenu*>([[button cell] menu]);
  EXPECT_TRUE(menu);
  EXPECT_TRUE([menu isKindOfClass:[BookmarkMenu class]]);
  EXPECT_EQ(folder, [menu node]);

  // Make sure "Open All" is disabled (nothing to open -- no children!)
  // (Assumes "Open All" is the 1st item)
  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_FALSE([bar_ validateUserInterfaceItem:item]);

  // Now add a child and make sure the item would be enabled.
  model->AddURL(folder, folder->GetChildCount(),
                L"super duper wide title",
                GURL("http://superfriends.hall-of-justice.edu"));
  EXPECT_TRUE([bar_ validateUserInterfaceItem:item]);
}

TEST_F(BookmarkBarControllerTest, TestDragButton) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  GURL gurls[] = { GURL("http://www.google.com/a"),
                   GURL("http://www.google.com/b"),
                   GURL("http://www.google.com/c") };
  std::wstring titles[] = { L"a", L"b", L"c" };
  for (unsigned i = 0; i < arraysize(titles); i++) {
    model->SetURLStarred(gurls[i], titles[i], true);
  }

  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:0] title] isEqual:@"a"]);

  [bar_ dragButton:[[bar_ buttons] objectAtIndex:2] to:NSMakePoint(0, 0)];
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:0] title] isEqual:@"c"]);

  [bar_ dragButton:[[bar_ buttons] objectAtIndex:1] to:NSMakePoint(1000, 0)];
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:0] title] isEqual:@"c"]);
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:1] title] isEqual:@"b"]);
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:2] title] isEqual:@"a"]);

  // Finally, a drop of the 1st between the next 2
  CGFloat x = NSMinX([[[bar_ buttons] objectAtIndex:2] frame]);
  x += [[bar_ view] frame].origin.x;
  [bar_ dragButton:[[bar_ buttons] objectAtIndex:0] to:NSMakePoint(x, 0)];
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:0] title] isEqual:@"b"]);
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:1] title] isEqual:@"c"]);
  EXPECT_TRUE([[[[bar_ buttons] objectAtIndex:2] title] isEqual:@"a"]);
}

// Fake a theme with colored text.  Apply it and make sure bookmark
// buttons have the same colored text.  Repeat more than once.
TEST_F(BookmarkBarControllerTest, TestThemedButton) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  model->SetURLStarred(GURL("http://www.foo.com"), L"small", true);
  BookmarkButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(button);

  NSArray* colors = [NSArray arrayWithObjects:[NSColor redColor],
                                              [NSColor blueColor],
                                              nil];
  for (NSColor* color in colors) {
    FakeTheme theme(color);
    [bar_ updateTheme:&theme];
    NSAttributedString* astr = [button attributedTitle];
    EXPECT_TRUE(astr);
    EXPECT_TRUE([[astr string] isEqual:@"small"]);
    // Pick a char in the middle to test (index 3)
    NSDictionary* attributes = [astr attributesAtIndex:3 effectiveRange:NULL];
    NSColor* newColor =
        [attributes objectForKey:NSForegroundColorAttributeName];
    EXPECT_TRUE([newColor isEqual:color]);
  }
}

// Test that delegates and targets of buttons are cleared on dealloc.
TEST_F(BookmarkBarControllerTest, TestClearOnDealloc) {
  // Make some bookmark buttons.
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  GURL gurls[] = { GURL("http://www.foo.com/"),
                   GURL("http://www.bar.com/"),
                   GURL("http://www.baz.com/") };
  std::wstring titles[] = { L"foo", L"bar", L"baz" };
  for (size_t i = 0; i < arraysize(titles); i++)
    model->SetURLStarred(gurls[i], titles[i], true);

  // Get and retain the buttons so we can examine them after dealloc.
  scoped_nsobject<NSArray> buttons([[bar_ buttons] retain]);
  EXPECT_EQ([buttons count], arraysize(titles));

  // Make sure that everything is set.
  for (BookmarkButton* button in buttons.get()) {
    ASSERT_TRUE([button isKindOfClass:[BookmarkButton class]]);
    EXPECT_TRUE([button delegate]);
    EXPECT_TRUE([button target]);
    EXPECT_TRUE([button action]);
  }

  // This will dealloc....
  bar_.reset();

  // Make sure that everything is cleared.
  for (BookmarkButton* button in buttons.get()) {
    EXPECT_FALSE([button delegate]);
    EXPECT_FALSE([button target]);
    EXPECT_FALSE([button action]);
  }
}

TEST_F(BookmarkBarControllerTest, TestFolders) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();

  // Create some folder buttons.
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* folder = model->AddGroup(parent,
                                               parent->GetChildCount(),
                                               L"group");
  model->AddURL(folder, folder->GetChildCount(),
                L"f1", GURL("http://framma-lamma.com"));
  folder = model->AddGroup(parent, parent->GetChildCount(), L"empty");

  EXPECT_EQ([[bar_ buttons] count], 2U);
  BookmarkButton* button = [[bar_ buttons] objectAtIndex:0];  // full one

  EXPECT_FALSE([bar_ folderController]);
  [bar_ openBookmarkFolderFromButton:button];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);

  // Make sure a 2nd open on the same button closes things.
  [bar_ openBookmarkFolderFromButton:button];
  EXPECT_FALSE([bar_ folderController]);

  // Next open is a different button.
  [bar_ openBookmarkFolderFromButton:[[bar_ buttons] objectAtIndex:1]];
  EXPECT_TRUE([bar_ folderController]);
  EXPECT_NE(bbfc, [bar_ folderController]);

  // Finally confirm a close removes the folder controller.
  [bar_ closeBookmarkFolder:nil];
  EXPECT_FALSE([bar_ folderController]);

  // Next part of the test: similar actions but with mouseEntered/mouseExited.

  // First confirm mouseEntered does nothing if "menus" aren't active.
  NSEvent* event = test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0);
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:0] event:event];
  EXPECT_FALSE([bar_ folderController]);

  // Make one active.  Entering it is now a no-op.
  [bar_ openBookmarkFolderFromButton:[[bar_ buttons] objectAtIndex:0]];
  bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:0] event:event];
  EXPECT_EQ(bbfc, [bar_ folderController]);

  // Enter a different one; a new folderController is active.
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:1] event:event];
  EXPECT_NE(bbfc, [bar_ folderController]);

  // Confirm exited is a no-op.
  [bar_ mouseExitedButton:[[bar_ buttons] objectAtIndex:1] event:event];
  EXPECT_NE(bbfc, [bar_ folderController]);

  // Clean up.
  [bar_ closeBookmarkFolder:nil];
}

TEST_F(BookmarkBarControllerTest, ClickOutsideCheck) {
  NSEvent* event = test_event_utils::MakeMouseEvent(NSMouseMoved, 0);
  EXPECT_FALSE([bar_ isEventAClickOutside:event]);

  BookmarkBarFolderWindow* folderWindow = [[[BookmarkBarFolderWindow alloc]
                                             init] autorelease];
  [[[bar_ view] window] addChildWindow:folderWindow
                               ordered:NSWindowAbove];
  event = test_event_utils::LeftMouseDownAtPointInWindow(NSMakePoint(1,1),
                                                         folderWindow);
  EXPECT_FALSE([bar_ isEventAClickOutside:event]);

  event = test_event_utils::LeftMouseDownAtPointInWindow(NSMakePoint(100,100),
                                                         test_window());
  EXPECT_TRUE([bar_ isEventAClickOutside:event]);
  [[[bar_ view] window] removeChildWindow:folderWindow];
}

TEST_F(BookmarkBarControllerTest, DropDestination) {
  // Make some buttons.
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddGroup(parent, parent->GetChildCount(), L"group 1");
  model->AddGroup(parent, parent->GetChildCount(), L"group 2");
  EXPECT_EQ([[bar_ buttons] count], 2U);

  // Confirm "off to left" and "off to right" match nothing.
  NSPoint p = NSMakePoint(-1, 2);
  EXPECT_FALSE([bar_ buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bar_ shouldShowIndicatorShownForPoint:p]);
  p = NSMakePoint(50000, 10);
  EXPECT_FALSE([bar_ buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bar_ shouldShowIndicatorShownForPoint:p]);

  // Confirm "right in the center" (give or take a pixel) is a match,
  // and confirm "just barely in the button" is not.  Anything more
  // specific seems likely to be tweaked.
  for (BookmarkButton* button in [bar_ buttons]) {
    CGFloat x = NSMidX([button frame]);
    // Somewhere near the center: a match
    EXPECT_EQ(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x-1, 10)]);
    EXPECT_EQ(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x+1, 10)]);
    EXPECT_FALSE([bar_ shouldShowIndicatorShownForPoint:NSMakePoint(x, 10)]);;

    // On the very edges: NOT a match
    x = NSMinX([button frame]);
    EXPECT_NE(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x, 9)]);
    x = NSMaxX([button frame]);
    EXPECT_NE(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x, 11)]);
  }
}

// TODO(jrg): draggingEntered: and draggingExited: trigger timers so
// they are hard to test.  Factor out "fire timers" into routines
// which can be overridden to fire immediately to make behavior
// confirmable.

// TODO(jrg): add unit test to make sure "Other Bookmarks" responds
// properly to a hover open.

// TODO(viettrungluu): figure out how to test animations.

}  // namespace
