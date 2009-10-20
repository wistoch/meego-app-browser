// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface BBDelegate : NSObject<BookmarkBubbleControllerDelegate> {
  NSWindow* window_;  // weak
  int edits_;
  int dones_;
}
@property (readonly) int edits;
@property (readonly) int dones;
@property (readonly) NSWindow* window;
@end

@implementation BBDelegate

@synthesize edits = edits_;
@synthesize window = window_;
@synthesize dones = dones_;

- (NSPoint)topLeftForBubble {
  return NSMakePoint(10, 300);
}

- (void)editBookmarkNode:(const BookmarkNode*)node {
  edits_++;
}

- (void)doneWithBubbleController:(BookmarkBubbleController*)controller {
  dones_++;
}

- (void)clear {
  edits_ = 0;
  dones_ = 0;
}

@end

namespace {

class BookmarkBubbleControllerTest : public PlatformTest {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<BBDelegate> delegate_;
  scoped_nsobject<BookmarkBubbleController> controller_;

  BookmarkBubbleControllerTest() {
    delegate_.reset([[BBDelegate alloc] init]);
  }

  // Returns a controller but ownership not transferred.
  // Only one of these will be valid at a time.
  BookmarkBubbleController* ControllerForNode(const BookmarkNode* node) {
    controller_.reset([[BookmarkBubbleController alloc]
                        initWithDelegate:delegate_.get()
                            parentWindow:cocoa_helper_.window()
                        topLeftForBubble:[delegate_ topLeftForBubble]
                                   model:helper_.profile()->GetBookmarkModel()
                                    node:node
                       alreadyBookmarked:YES]);
    [controller_ view];  // force nib load
    return controller_.get();
  }

  BookmarkModel* GetBookmarkModel() {
    return helper_.profile()->GetBookmarkModel();
  }
};

// Confirm basics about the bubble window (e.g. that it is inside the
// parent window)
TEST_F(BookmarkBubbleControllerTest, TestBubbleWindow) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  NSWindow* window = [controller createBubbleWindow];
  EXPECT_TRUE(window);
  EXPECT_TRUE(NSContainsRect([cocoa_helper_.window() frame],
                             [window frame]));
}

// Confirm population of folder list
TEST_F(BookmarkBubbleControllerTest, TestFillInFolder) {
  // Create some folders, including a nested folder
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node1 = model->AddGroup(model->GetBookmarkBarNode(),
                                              0, L"one");
  const BookmarkNode* node2 = model->AddGroup(model->GetBookmarkBarNode(),
                                              1, L"two");
  const BookmarkNode* node3 = model->AddGroup(model->GetBookmarkBarNode(),
                                              2, L"three");
  const BookmarkNode* node4 = model->AddGroup(node2,
                                              0, L"sub");
  model->AddURL(node1, 0, L"title1", GURL("http://www.google.com"));
  model->AddURL(node3, 0, L"title2", GURL("http://www.google.com"));
  model->AddURL(node4, 0, L"title3", GURL("http://www.google.com/reader"));

  BookmarkBubbleController* controller = ControllerForNode(node4);
  EXPECT_TRUE(controller);

  NSArray* items = [[controller folderComboBox] objectValues];
  EXPECT_TRUE([items containsObject:@"one"]);
  EXPECT_TRUE([items containsObject:@"two"]);
  EXPECT_TRUE([items containsObject:@"three"]);
  EXPECT_TRUE([items containsObject:@"sub"]);
  EXPECT_FALSE([items containsObject:@"title1"]);
  EXPECT_FALSE([items containsObject:@"title2"]);
}

// Click on edit; bubble gets closed.
TEST_F(BookmarkBubbleControllerTest, TestSimpleActions) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  EXPECT_EQ([delegate_ edits], 0);
  EXPECT_EQ([delegate_ dones], 0);
  EXPECT_FALSE([controller windowHasBeenClosed]);
  [controller edit:controller];
  EXPECT_EQ([delegate_ edits], 1);
  EXPECT_EQ([delegate_ dones], 1);
  EXPECT_TRUE([controller windowHasBeenClosed]);

  [delegate_ clear];
  EXPECT_EQ([delegate_ edits], 0);
  EXPECT_EQ([delegate_ dones], 0);

  controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_FALSE([controller windowHasBeenClosed]);
  [controller close:controller];
  EXPECT_EQ([delegate_ edits], 0);
  EXPECT_EQ([delegate_ dones], 1);
  EXPECT_TRUE([controller windowHasBeenClosed]);
}

// User changes title and parent folder in the UI
TEST_F(BookmarkBubbleControllerTest, TestUserEdit) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"short-title",
                                           GURL("http://www.google.com"));
  model->AddGroup(model->GetBookmarkBarNode(), 0, L"grandma");
  model->AddGroup(model->GetBookmarkBarNode(), 0, L"grandpa");
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  // simulate a user edit
  [controller setTitle:@"oops" parentFolder:@"grandma"];
  [controller edit:controller];

  // Make sure bookmark has changed
  EXPECT_EQ(node->GetTitle(), L"oops");
  EXPECT_EQ(node->GetParent()->GetTitle(), L"grandma");
}

// Click the "remove" button
TEST_F(BookmarkBubbleControllerTest, TestRemove) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_TRUE(model->IsBookmarked(gurl));

  [controller remove:controller];
  EXPECT_FALSE(model->IsBookmarked(gurl));
  EXPECT_TRUE([controller windowHasBeenClosed]);
  EXPECT_EQ([delegate_ dones], 1);
}

// Confirm picking "choose another folder" caused edit: to be called.
TEST_F(BookmarkBubbleControllerTest, ComboSelectionChanged) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0, L"super-title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  NSString* chooseAnotherFolder = [controller chooseAnotherFolderString];
  EXPECT_EQ([delegate_ edits], 0);
  [controller setTitle:@"DOH!" parentFolder:chooseAnotherFolder];
  EXPECT_EQ([delegate_ edits], 1);
}

// Create a controller that simulates the bookmark just now being created by
// the user clicking the star, then sending the "cancel" command to represent
// them pressing escape. The bookmark should not be there.
TEST_F(BookmarkBubbleControllerTest, EscapeRemovesNewBookmark) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  scoped_nsobject<BookmarkBubbleController> controller(
      [[BookmarkBubbleController alloc]
          initWithDelegate:delegate_.get()
              parentWindow:cocoa_helper_.window()
          topLeftForBubble:[delegate_ topLeftForBubble]
                     model:helper_.profile()->GetBookmarkModel()
                      node:node
         alreadyBookmarked:NO]);  // The last param is the key difference.
  EXPECT_TRUE(controller);

  [controller cancel:nil];
  EXPECT_FALSE(model->IsBookmarked(gurl));
}

// Create a controller where the bookmark already existed prior to clicking
// the star and test that sending a cancel command doesn't change the state
// of the bookmark.
TEST_F(BookmarkBubbleControllerTest, EscapeDoesntTouchExistingBookmark) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  [(id)controller cancel:nil];
  EXPECT_TRUE(model->IsBookmarked(gurl));
}

}  // namespace
