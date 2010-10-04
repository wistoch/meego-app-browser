// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"

const std::string kAllUrlsTarget =
    "files/extensions/api_test/all_urls/index.html";

typedef ExtensionApiTest AllUrlsApiTest;

IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, FAILS_WhitelistedExtension) {
  // First add the two extensions we are going to load to the whitelist.
  const char* kCanExecuteScriptsEverywhere[] = {
    "fekpfaahmgnelcjpkefdnpiofglcgmgo",
    "bpkfbiacjfimfmglhncgmibnddpnhmoj",
  };
  Dbg("Set Whitelist");
  Extension::SetScriptingWhitelist(kCanExecuteScriptsEverywhere,
                                   arraysize(kCanExecuteScriptsEverywhere));


  // Then load the two extension.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir1));
  ASSERT_TRUE(LoadExtension(extension_dir2));
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  std::string url;

  Dbg("Extensions loaded");

  // Now verify we run content scripts on chrome://newtab/.
  url = "chrome://newtab/";
  ExtensionTestMessageListener listener1a("content script: " + url);
  ExtensionTestMessageListener listener1b("execute: " + url);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Dbg("Listening on 1a");
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  Dbg("Listening on 1b");
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  ExtensionTestMessageListener listener2a("content script: " + url);
  ExtensionTestMessageListener listener2b("execute: " + url);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Dbg("Listening on 2a");
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  Dbg("Listening on 2b");
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify about:version.
  url = "about:version";
  ExtensionTestMessageListener listener3a("content script: " + url);
  ExtensionTestMessageListener listener3b("execute: " + url);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Dbg("Listening on 3a");
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  Dbg("Listening on 3b");
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  url = "about:blank";
  ExtensionTestMessageListener listener4a("content script: " + url);
  ExtensionTestMessageListener listener4b("execute: " + url);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Dbg("Listening on 4a");
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  Dbg("Listening on 4b");
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec());
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec());
  ui_test_utils::NavigateToURL(browser(), page_url);
  Dbg("Listening on 5a");
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  Dbg("Listening on 5b");
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());
}

// Test that an extension NOT whitelisted for scripting can ask for <all_urls>
// and run scripts on non-restricted all pages.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, RegularExtensions) {
  // First load the two extension.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir1));
  ASSERT_TRUE(LoadExtension(extension_dir2));
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener1a("content script: " + page_url.spec());
  ExtensionTestMessageListener listener1b("execute: " + page_url.spec());
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());
}
