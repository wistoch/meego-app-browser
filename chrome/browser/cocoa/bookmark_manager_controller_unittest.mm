// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkManagerControllerTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();
    controller_ = [BookmarkManagerController showBookmarkManager:
                   browser_test_helper_.profile()];
    ASSERT_TRUE(controller_);
  }

  void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  BookmarkItem* AddToBar(NSString*title, NSString* urlStr) {
    BookmarkItem* bar = [controller_ bookmarkBarItem];
    return [bar addBookmarkWithTitle:title
                                 URL:urlStr
                             atIndex:[bar numberOfChildren]];
  }

  NSSet* AddFixtureItems() {
    return [NSSet setWithObjects:
        AddToBar(@"Google", @"http://google.com"),
        AddToBar(@"GMail", @"http://gmail.com"),
        AddToBar(@"Google Sites", @"http://sites.google.com"),
        nil];
  }

  bool SearchResultsVisible() {
    NSOutlineView* outline = [[controller_ groupsController] outline];
    return [outline rowForItem:[controller_ searchGroup]] >= 0;
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* controller_;
};

TEST_F(BookmarkManagerControllerTest, IsThisThingTurnedOn) {
  NSWindow* w = [controller_ window];
  ASSERT_TRUE(w);
  EXPECT_TRUE([w isVisible]);

  ASSERT_TRUE([controller_ groupsController]);
  ASSERT_TRUE([controller_ listController]);
}

TEST_F(BookmarkManagerControllerTest, Model) {
  BookmarkModel* model = [controller_ bookmarkModel];
  ASSERT_EQ(browser_test_helper_.profile()->GetBookmarkModel(), model);

  // Check the bookmarks-bar item:
  const BookmarkNode* bar = model->GetBookmarkBarNode();
  ASSERT_TRUE(bar);
  BookmarkItem* barItem = [controller_ itemFromNode:bar];
  ASSERT_TRUE(barItem);
  ASSERT_EQ(barItem, [controller_ bookmarkBarItem]);

  // Check the 'others' item:
  const BookmarkNode* other = model->other_node();
  ASSERT_TRUE(other);
  EXPECT_NE(bar, other);
  scoped_nsobject<BookmarkItem> otherItem(
      [[controller_ itemFromNode:other] retain]);
  ASSERT_TRUE(otherItem);
  ASSERT_EQ(otherItem, [controller_ otherBookmarksItem]);

  EXPECT_NE(barItem, otherItem);
  EXPECT_FALSE([barItem isEqual:otherItem]);

  EXPECT_EQ(bar, [barItem node]);
  EXPECT_EQ(barItem, [controller_ itemFromNode:bar]);
  EXPECT_EQ(other, [otherItem node]);
  EXPECT_EQ(otherItem, [controller_ itemFromNode:other]);

  // Now tell it to forget 'other' and see if we get a different BookmarkItem:
  [controller_ forgetNode:other];
  BookmarkItem* otherItem2 = [controller_ itemFromNode:other];
  EXPECT_TRUE(otherItem2);
  EXPECT_NE(otherItem, otherItem2);
}

TEST_F(BookmarkManagerControllerTest, Recents) {
  NSSet* fixtures = AddFixtureItems();
  // Show the Recent Items group, so its contents will be updated.
  FakeBookmarkItem* recents = [controller_ recentGroup];
  [controller_ showGroup:recents];
  NSSet* shown = [NSSet setWithArray:[recents children]];
  EXPECT_TRUE([fixtures isEqual:shown]);
}

TEST_F(BookmarkManagerControllerTest, Search) {
  AddFixtureItems();
  // Search for 'gmail':
  [controller_ setSearchString:@"gmail"];
  FakeBookmarkItem* search = [controller_ searchGroup];
  EXPECT_EQ(search, [[controller_ groupsController] selectedItem]);
  NSArray* shown = [search children];
  EXPECT_EQ(1U, [shown count]);
  EXPECT_TRUE([@"GMail" isEqualToString:[[shown lastObject] title]]);

  // Search for 'google':
  [controller_ setSearchString:@"google"];
  shown = [search children];
  EXPECT_EQ(2U, [shown count]);

  // Search for 'fnord':
  [controller_ setSearchString:@"fnord"];
  shown = [search children];
  EXPECT_EQ(0U, [shown count]);
}

TEST_F(BookmarkManagerControllerTest, SearchSelection) {
  BookmarkTreeController* groupsController = [controller_ groupsController];
  AddFixtureItems();
  BookmarkItem* originalSelection = [controller_ bookmarkBarItem];
  EXPECT_FALSE(SearchResultsVisible());
  EXPECT_EQ(originalSelection, [groupsController selectedItem]);

  // Start a search and verify the search results group is selected.
  [controller_ setSearchString:@"g"];
  EXPECT_TRUE(SearchResultsVisible());
  EXPECT_EQ([controller_ searchGroup], [groupsController selectedItem]);

  // Type some more, see if updating the search string works.
  [controller_ setSearchString:@"gmail"];
  EXPECT_TRUE(SearchResultsVisible());
  EXPECT_EQ([controller_ searchGroup], [groupsController selectedItem]);

  // Clear search, verify search results are hidden and original sel restored.
  [controller_ setSearchString:@""];
  EXPECT_FALSE(SearchResultsVisible());
  EXPECT_EQ(originalSelection, [groupsController selectedItem]);

  // Now search, then change the selection, then clear search:
  [controller_ setSearchString:@"gmail"];
  EXPECT_TRUE(SearchResultsVisible());
  EXPECT_EQ([controller_ searchGroup], [groupsController selectedItem]);
  BookmarkItem* newerSelection = [controller_ otherBookmarksItem];
  [controller_ showGroup:newerSelection];
  [controller_ setSearchString:@""];
  EXPECT_FALSE(SearchResultsVisible());
  EXPECT_EQ(newerSelection, [groupsController selectedItem]);
}

}  // namespace
