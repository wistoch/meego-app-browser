// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "net/base/mock_host_resolver.h"

class BrowserTest : public InProcessBrowserTest {
 public:
  BrowserTest() {
    host_resolver_proc_ = new net::RuleBasedHostResolverProc(NULL);
    // Avoid making external DNS lookups. In this test we don't need this
    // to succeed.
    host_resolver_proc_->AddSimulatedFailure("*.google.com");
    scoped_host_resolver_proc_.Init(host_resolver_proc_.get());
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

/*
// This tests that windows without tabstrips can't have new tabs opened in
// them.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTabsInPopups) {
  Browser::RegisterAppPrefs(L"Test");

  // We start with a normal browser with one tab.
  EXPECT_EQ(1, browser()->tab_count());

  // Open a popup browser with a single blank foreground tab.
  Browser* popup_browser = browser()->CreateForPopup(browser()->profile());
  popup_browser->AddBlankTab(true);
  EXPECT_EQ(1, popup_browser->tab_count());

  // Now try opening another tab in the popup browser.
  popup_browser->AddTabWithURL(
      GURL(chrome::kAboutBlankURL), GURL(), PageTransition::TYPED, -1,
      Browser::ADD_SELECTED, NULL, std::string());

  // The popup should still only have one tab.
  EXPECT_EQ(1, popup_browser->tab_count());

  // The normal browser should now have two.
  EXPECT_EQ(2, browser()->tab_count());

  // Open an app frame browser with a single blank foreground tab.
  Browser* app_browser =
      browser()->CreateForApp(L"Test", browser()->profile(), false);
  app_browser->AddBlankTab(true);
  EXPECT_EQ(1, app_browser->tab_count());

  // Now try opening another tab in the app browser.
  app_browser->AddTabWithURL(
      GURL(chrome::kAboutBlankURL), GURL(), PageTransition::TYPED, -1,
      Browser::ADD_SELECTED, NULL, std::string());

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_browser->tab_count());

  // The normal browser should now have three.
  EXPECT_EQ(3, browser()->tab_count());

  // Open an app frame popup browser with a single blank foreground tab.
  Browser* app_popup_browser =
      browser()->CreateForApp(L"Test", browser()->profile(), false);
  app_popup_browser->AddBlankTab(true);
  EXPECT_EQ(1, app_popup_browser->tab_count());

  // Now try opening another tab in the app popup browser.
  app_popup_browser->AddTabWithURL(
      GURL(chrome::kAboutBlankURL), GURL(), PageTransition::TYPED, -1,
      Browser::ADD_SELECTED, NULL, std::string());

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_popup_browser->tab_count());

  // The normal browser should now have four.
  EXPECT_EQ(4, browser()->tab_count());

  // Close the additional browsers.
  popup_browser->CloseAllTabs();
  app_browser->CloseAllTabs();
  app_popup_browser->CloseAllTabs();
}
*/
