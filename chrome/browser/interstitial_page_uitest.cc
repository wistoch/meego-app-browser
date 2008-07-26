// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>

#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

const std::string kInterstitialPageHTMLText =
    "<html><head><title>Interstitial page</title></head><body><h1>This is an"
    "interstitial page</h1></body></html>";


class InterstitialPageTest : public UITest {
 protected:
  InterstitialPageTest() {
    show_window_ = true;
  }

  TabProxy* GetActiveTabProxy() {
    scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser_proxy.get());

    int active_tab_index = 0;
    EXPECT_TRUE(browser_proxy->GetActiveTabIndex(&active_tab_index));
    return browser_proxy->GetTab(active_tab_index);
  }

  void NavigateTab(TabProxy* tab_proxy, const GURL& url) {
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  }

  void AppendTab(const GURL& url) {
    scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser_proxy.get());
    EXPECT_TRUE(browser_proxy->AppendTab(url));
  }
};

}  // namespace

// Shows and hides an interstitial page.
// Note that we cannot rely on the page title in this case (and we use the page
// type instead) as showing an interstitial without creating a navigation entry
// causes the actual navigation entry (title) to be modified by the content of
// the interstitial.
TEST_F(InterstitialPageTest, TestShowHideInterstitial) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              server.TestServerPageW(L"files/interstitial_page/google.html"));
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  tab->ShowInterstitialPage(kInterstitialPageHTMLText);
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::INTERSTITIAL_PAGE, page_type);

  tab->HideInterstitialPage();
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
}

// Shows an interstitial page then goes back.
// TODO(creis): We are disabling this test for now.  We need to revisit
// whether the interstitial page should actually commit a NavigationEntry,
// because this clears the forward list and changes the meaning of back.  It
// seems like the interstitial should not affect the NavigationController,
// who will remain in a pending state until the user either proceeds or cancels
// the interstitial.  In the mean time, we are treating Back like cancelling
// the interstitial, which breaks this test because no notification occurs.
TEST_F(InterstitialPageTest, DISABLED_TestShowInterstitialThenBack) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              server.TestServerPageW(L"files/interstitial_page/google.html"));
  EXPECT_EQ(L"Google", GetActiveTabTitle());

  tab->ShowInterstitialPage(kInterstitialPageHTMLText);
  EXPECT_EQ(L"Interstitial page", GetActiveTabTitle());

  tab->GoBack();
  EXPECT_EQ(L"Google", GetActiveTabTitle());
}

// Shows an interstitial page then navigates to a new URL.
// Flacky on Windows 2000 bot. Disabled for now bug #1173138.
TEST_F(InterstitialPageTest, DISABLED_TestShowInterstitialThenNavigate) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              server.TestServerPageW(L"files/interstitial_page/google.html"));
  EXPECT_EQ(L"Google", GetActiveTabTitle());

  tab->ShowInterstitialPage(kInterstitialPageHTMLText);
  EXPECT_EQ(L"Interstitial page", GetActiveTabTitle());

  tab->NavigateToURL(
      server.TestServerPageW(L"files/interstitial_page/shopping.html"));
  EXPECT_EQ(L"Google Product Search", GetActiveTabTitle());
}

// Shows an interstitial page then closes the tab (to make sure we don't crash).
TEST_F(InterstitialPageTest, TestShowInterstitialThenCloseTab) {
  TestServer server(kDocRoot);

  // Create 2 tabs so closing one does not close the browser.
  AppendTab(server.TestServerPageW(L"files/interstitial_page/google.html"));
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  EXPECT_EQ(L"Google", GetActiveTabTitle());

  tab->ShowInterstitialPage(kInterstitialPageHTMLText);
  EXPECT_EQ(L"Interstitial page", GetActiveTabTitle());
  tab->Close();
}

// Shows an interstitial page then closes the browser (to make sure we don't
// crash).
// This test is disabled. See bug #1119448.
TEST_F(InterstitialPageTest, DISABLED_TestShowInterstitialThenCloseBrowser) {
  TestServer server(kDocRoot);

  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  tab->NavigateToURL(
      server.TestServerPageW(L"files/interstitial_page/google.html"));
  EXPECT_EQ(L"Google", GetActiveTabTitle());

  tab->ShowInterstitialPage(kInterstitialPageHTMLText);
  EXPECT_EQ(L"Interstitial page", GetActiveTabTitle());

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  bool application_closed;
  EXPECT_TRUE(CloseBrowser(browser_proxy.get(), &application_closed));
  EXPECT_TRUE(application_closed);
}


