// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"

class BookmarksUITest : public UITest {
 public:
  BookmarksUITest() {
    dom_automation_enabled_ = true;
  }

  bool WaitForBookmarksUI(TabProxy* tab) {
    return WaitUntilJavaScriptCondition(tab, L"",
        L"domAutomationController.send("
        L"    location.protocol == 'chrome-extension:' && "
        L"    document.readyState == 'complete')",
        100, UITest::test_timeout_ms());
  }

  scoped_refptr<TabProxy> GetBookmarksUITab() {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser.get());
    if (!browser.get())
      return NULL;
    scoped_refptr<TabProxy> tab = browser->GetActiveTab();
    EXPECT_TRUE(tab.get());
    if (!tab.get())
      return NULL;
    if (!tab->NavigateToURL(GURL(chrome::kChromeUIBookmarksURL)))
      return NULL;
    if (!WaitForBookmarksUI(tab))
      return NULL;
    return tab;
  }

  void AssertIsBookmarksPage(TabProxy* tab) {
    // tab->GetCurrentURL is not up to date.
    GURL url;
    std::wstring out;
    ASSERT_TRUE(tab->ExecuteAndExtractString(L"",
        L"domAutomationController.send(location.protocol)", &out));
    ASSERT_EQ(L"chrome-extension:", out);
    ASSERT_TRUE(tab->ExecuteAndExtractString(L"",
        L"domAutomationController.send(location.pathname)", &out));
    ASSERT_EQ(L"/main.html", out);
  }
};

TEST_F(BookmarksUITest, ShouldRedirectToExtension) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  int tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  // Navigate to chrome
  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(tab->NavigateToURL(GURL(chrome::kChromeUIBookmarksURL)));

  // At this point the URL is chrome://bookmarks. We need to wait for the
  // redirect to happen.
  ASSERT_TRUE(WaitForBookmarksUI(tab));

  AssertIsBookmarksPage(tab);
}

TEST_F(BookmarksUITest, CommandOpensBookmarksTab) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  int tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  // Bring up the bookmarks manager tab.
  ASSERT_TRUE(browser->RunCommand(IDC_SHOW_BOOKMARK_MANAGER));
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(WaitForBookmarksUI(tab));

  AssertIsBookmarksPage(tab);
}

TEST_F(BookmarksUITest, BookmarksLoaded) {
  scoped_refptr<TabProxy> tab = GetBookmarksUITab();
  ASSERT_TRUE(tab.get());
}
