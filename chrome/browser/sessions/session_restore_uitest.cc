// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

namespace {

class SessionRestoreUITest : public UITest {
 protected:
  SessionRestoreUITest() : UITest() {
    FilePath path_prefix = FilePath::FromWStringHack(test_data_directory_)
        .AppendASCII("session_history");

    url1 = net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html"));
    url2 = net::FilePathToFileURL(path_prefix.AppendASCII("bot2.html"));
    url3 = net::FilePathToFileURL(path_prefix.AppendASCII("bot3.html"));
  }

  virtual void QuitBrowserAndRestore(int expected_tab_count) {
    UITest::TearDown();

    clear_profile_ = false;

    launch_arguments_.AppendSwitchWithValue(switches::kRestoreLastSession,
                                            IntToWString(expected_tab_count));
    UITest::SetUp();
  }

  void CloseWindow(int window_index, int initial_count) {
    scoped_ptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(window_index));
    ASSERT_TRUE(browser_proxy.get());
    ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_CLOSE_WINDOW));
    browser_proxy.reset();
    ASSERT_TRUE(automation()->WaitForWindowCountToBecome(initial_count - 1,
                                                         action_timeout_ms()));
  }

  void AssertOneWindowWithOneTab() {
    int window_count;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);
    GURL url;
    AssertWindowHasOneTab(0, &url);
  }

  void AssertWindowHasOneTab(int window_index, GURL* url) {
    scoped_ptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(window_index));
    ASSERT_TRUE(browser_proxy.get());

    int tab_count;
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_EQ(1, tab_count);

    int active_tab_index;
    ASSERT_TRUE(browser_proxy->GetActiveTabIndex(&active_tab_index));
    ASSERT_EQ(0, active_tab_index);

    scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetActiveTab());
    ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

    ASSERT_TRUE(tab_proxy->GetCurrentURL(url));
  }

  GURL url1;
  GURL url2;
  GURL url3;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestoreUITest);
};

}  // namespace

TEST_F(SessionRestoreUITest, Basic) {
  NavigateToURL(url1);
  NavigateToURL(url2);

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  ASSERT_EQ(url2, GetActiveTabURL());
  tab_proxy->GoBack();
  ASSERT_EQ(url1, GetActiveTabURL());
}

TEST_F(SessionRestoreUITest, RestoresForwardAndBackwardNavs) {
  NavigateToURL(url1);
  NavigateToURL(url2);
  NavigateToURL(url3);

  scoped_ptr<TabProxy> active_tab(GetActiveTab());
  ASSERT_TRUE(active_tab->GoBack());

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  ASSERT_TRUE(GetActiveTabURL() == url2);
  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(GetActiveTabURL() == url3);
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(GetActiveTabURL() == url2);
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(GetActiveTabURL() == url1);
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, so that going back to a restored
// cross-site page and then forward again works.  (Bug 1204135)
TEST_F(SessionRestoreUITest, RestoresCrossSiteForwardAndBackwardNavs) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL cross_site_url(server->TestServerPageW(L"files/title2.html"));

  // Visit URLs on different sites.
  NavigateToURL(url1);
  NavigateToURL(cross_site_url);
  NavigateToURL(url2);

  scoped_ptr<TabProxy> active_tab(GetActiveTab());
  ASSERT_TRUE(active_tab->GoBack());

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count) && tab_count == 1);
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_max_timeout_ms()));

  // Check that back and forward work as expected.
  GURL url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(cross_site_url, url);

  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url1, url);

  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(cross_site_url, url);

  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);
}

TEST_F(SessionRestoreUITest, TwoTabsSecondSelected) {
  NavigateToURL(url1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  ASSERT_TRUE(browser_proxy->AppendTab(url2));

  QuitBrowserAndRestore(2);
  browser_proxy.reset();

  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  browser_proxy.reset(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));

  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  int active_tab_index;
  ASSERT_TRUE(browser_proxy->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  tab_proxy.reset(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  ASSERT_TRUE(GetActiveTabURL() == url2);

  ASSERT_TRUE(browser_proxy->ActivateTab(0));
  tab_proxy.reset(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  ASSERT_TRUE(GetActiveTabURL() == url1);
}

// Creates two tabs, closes one, quits and makes sure only one tab is restored.
TEST_F(SessionRestoreUITest, ClosedTabStaysClosed) {
  NavigateToURL(url1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));

  browser_proxy->AppendTab(url2);

  scoped_ptr<TabProxy> active_tab(browser_proxy->GetActiveTab());
  active_tab->Close(true);

  QuitBrowserAndRestore(1);
  browser_proxy.reset();
  tab_proxy.reset();

  AssertOneWindowWithOneTab();

  ASSERT_TRUE(GetActiveTabURL() == url1);
}

// This test is failing on win2k.
//
// Creates a browser, goes incognito, closes browser, launches and make sure
// we don't restore.
TEST_F(SessionRestoreUITest, DISABLED_DontRestoreWhileIncognito) {
  NavigateToURL(url1);

  // Make sure we have one window.
  int initial_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&initial_window_count) &&
              initial_window_count == 1);

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  // Create an off the record window and wait for it to appear.
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_NEW_INCOGNITO_WINDOW));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  // Close the first window.
  CloseWindow(0, 2);
  browser_proxy.reset();

  // Launch the browser again. Note, this doesn't spawn a new process, instead
  // it attaches to the current process.
  include_testing_id_ = false;
  use_existing_browser_ = true;
  clear_profile_ = false;
  launch_arguments_.AppendSwitch(switches::kRestoreLastSession);
  LaunchBrowser(launch_arguments_, false);

  // A new window should appear;
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  // And it shouldn't have url1 in it.
  browser_proxy.reset(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser_proxy.get());
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  GURL url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_TRUE(url != url1);
}

// This test is failing because of ipc_chanel errors when launched the second
// time.
//
// Creates two windows, closes one, restores, make sure only one window open.
TEST_F(SessionRestoreUITest, DISABLED_TwoWindowsCloseOneRestoreOnlyOne) {
  NavigateToURL(url1);

  // Make sure we have one window.
  int initial_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&initial_window_count) &&
              initial_window_count == 1);

  // Open a second window.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  // Close it.
  CloseWindow(1, 2);

  // Restart and make sure we have only one window with one tab and the url
  // is url1.
  QuitBrowserAndRestore(1);

  AssertOneWindowWithOneTab();

  ASSERT_TRUE(GetActiveTabURL() == url1);
}

// This test is disabled because it's triggering a bug in chrome_plugin_host.
//
// Launches an app window, closes tabbed browser, launches and makes sure
// we restore the tabbed browser url.
TEST_F(SessionRestoreUITest,
       DISABLED_RestoreAfterClosingTabbedBrowserWithAppAndLaunching) {
  NavigateToURL(url1);

  // Launch an app.
  include_testing_id_ = false;
  use_existing_browser_ = true;
  clear_profile_ = false;
  CommandLine app_launch_arguments = launch_arguments_;
  app_launch_arguments.AppendSwitchWithValue(switches::kApp,
                                             UTF8ToWide(url2.spec()));
  LaunchBrowser(app_launch_arguments, false);
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  // Close the first window.
  CloseWindow(0, 2);

  // Restore it, which should bring back the first window with url1.
  CommandLine restore_launch_arguments = launch_arguments_;
  restore_launch_arguments.AppendSwitch(switches::kRestoreLastSession);
  LaunchBrowser(restore_launch_arguments, false);
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));
  GURL url;
  AssertWindowHasOneTab(1, &url);
  ASSERT_EQ(url1, url);
}

// TODO(sky): bug 1200852, this test is flakey, so I'm disabling.
//
// Make sure after a restore the number of processes matches that of the number
// of processes running before the restore. This creates a new tab so that
// we should have two new tabs running.  (This test will pass in both
// process-per-site and process-per-site-instance, because we treat the new tab
// as a special case in process-per-site-instance so that it only ever uses one
// process.)
TEST_F(SessionRestoreUITest, DISABLED_ShareProcessesOnRestore) {
  if (in_process_renderer()) {
    // No point in running this test in single process mode.
    return;
  }

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get() != NULL);
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Create two new tabs.
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(tab_count + 1,
                                                     action_timeout_ms()));
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  scoped_ptr<TabProxy> last_tab(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(last_tab.get() != NULL);
  // Do a reload to ensure new tab page has loaded.
  ASSERT_TRUE(last_tab->Reload());
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(tab_count + 1,
                                                     action_timeout_ms()));
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  last_tab.reset(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(last_tab.get() != NULL);
  // Do a reload to ensure new tab page has loaded.
  ASSERT_TRUE(last_tab->Reload());
  int expected_process_count = GetBrowserProcessCount();
  int expected_tab_count = tab_count;

  // Restart.
  browser_proxy.reset();
  last_tab.reset();
  QuitBrowserAndRestore(3);

  // Wait for each tab to finish being restored, then make sure the process
  // count matches.
  browser_proxy.reset(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get() != NULL);
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  ASSERT_EQ(expected_tab_count, tab_count);

  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(tab_count - 2));
  ASSERT_TRUE(tab_proxy.get() != NULL);
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  tab_proxy.reset(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab_proxy.get() != NULL);
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  ASSERT_EQ(expected_process_count, GetBrowserProcessCount());
}
