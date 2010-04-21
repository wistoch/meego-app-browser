// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/bookmark_bar_folder_window.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BookmarkBarFolderWindowTest : public CocoaTest {
};

TEST_F(BookmarkBarFolderWindowTest, Borderless) {
  scoped_nsobject<BookmarkBarFolderWindow> window_;
  window_.reset([[BookmarkBarFolderWindow alloc]
                  initWithContentRect:NSMakeRect(0,0,20,20)
                            styleMask:0
                              backing:NSBackingStoreBuffered
                                defer:NO]);
  EXPECT_EQ(NSBorderlessWindowMask, [window_ styleMask]);
}


class BookmarkBarFolderWindowContentViewTest : public CocoaTest {
 public:
  BookmarkBarFolderWindowContentViewTest() {
    view_.reset([[BookmarkBarFolderWindowContentView alloc]
                  initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [[test_window() contentView] addSubview:view_.get()];
  }
  scoped_nsobject<BookmarkBarFolderWindowContentView> view_;
  scoped_nsobject<BookmarkBarFolderWindowScrollView> scroll_view_;
};

TEST_VIEW(BookmarkBarFolderWindowContentViewTest, view_);


class BookmarkBarFolderWindowScrollViewTest : public CocoaTest {
 public:
  BookmarkBarFolderWindowScrollViewTest() {
    scroll_view_.reset([[BookmarkBarFolderWindowScrollView alloc]
                  initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [[test_window() contentView] addSubview:scroll_view_.get()];
  }
  scoped_nsobject<BookmarkBarFolderWindowScrollView> scroll_view_;
};

TEST_VIEW(BookmarkBarFolderWindowScrollViewTest, scroll_view_);
