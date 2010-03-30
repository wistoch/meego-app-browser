// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

}  // namespace

typedef UITest RepostFormWarningTest;

TEST_F(RepostFormWarningTest, TestDoubleReload) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Load a form.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPageW(L"files/form.html")));
  // Submit it.
  ASSERT_TRUE(tab->NavigateToURL(GURL(
      "javascript:document.getElementById('form').submit()")));

  // Try to reload it twice, checking for repost.
  tab->ReloadAsync();
  tab->ReloadAsync();

  // Navigate away from the page (this is when the test usually crashes).
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPageW(L"bar")));
}

TEST_F(RepostFormWarningTest, TestLoginAfterRepost) {
  scoped_refptr<HTTPTestServer> server =
  HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Load a form.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPageW(L"files/form.html")));
  // Submit it.
  ASSERT_TRUE(tab->NavigateToURL(GURL(
      "javascript:document.getElementById('form').submit()")));

  // Try to reload it, checking for repost.
  tab->ReloadAsync();

  // Navigate to a page that requires authentication, bringing up another
  // tab-modal sheet.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPageW(L"auth-basic")));

  // Try to reload it again.
  tab->ReloadAsync();

  // Navigate away from the page.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPageW(L"bar")));
}
