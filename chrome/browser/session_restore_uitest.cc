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

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
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
    std::wstring path_prefix = test_data_directory_;
    file_util::AppendToPath(&path_prefix, L"session_history");
    path_prefix += file_util::kPathSeparator;

    url1 = net::FilePathToFileURL(path_prefix + L"bot1.html");
    url2 = net::FilePathToFileURL(path_prefix + L"bot2.html");
    url3 = net::FilePathToFileURL(path_prefix + L"bot3.html");
  }

  virtual void QuitBrowserAndRestore() {
    UITest::TearDown();

    clear_profile_ = false;

    CommandLine::AppendSwitch(&launch_arguments_,
                              switches::kRestoreLastSession);
    UITest::SetUp();
  }

  void CloseWindow(int window_index, int initial_count) {
    scoped_ptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(window_index));
    ASSERT_TRUE(browser_proxy.get());
    ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_CLOSEWINDOW));
    browser_proxy.reset();
    int window_count;
    ASSERT_TRUE(automation()->WaitForWindowCountToChange(initial_count,
                                                         &window_count,
                                                         kWaitForActionMsec));
    ASSERT_EQ(initial_count - 1, window_count);
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
    ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

    ASSERT_TRUE(tab_proxy->GetCurrentURL(url));
  }

  GURL url1;
  GURL url2;
  GURL url3;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SessionRestoreUITest);
};

}  // namespace

TEST_F(SessionRestoreUITest, Basic) {
  NavigateToURL(url1);
  NavigateToURL(url2);

  QuitBrowserAndRestore();

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

  ASSERT_EQ(url2, GetActiveTabURL());
  tab_proxy->GoBack();
  ASSERT_EQ(url1, GetActiveTabURL());
}

TEST_F(SessionRestoreUITest, RestoresForwardAndBackwardNavs) {
  NavigateToURL(url1);
  NavigateToURL(url2);
  NavigateToURL(url3);

  ASSERT_TRUE(GetActiveTab()->GoBack());

  QuitBrowserAndRestore();

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

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
  TestServer server(kDocRoot);
  GURL cross_site_url(server.TestServerPageW(L"files/title2.html"));

  // Visit URLs on different sites.
  NavigateToURL(url1);
  NavigateToURL(cross_site_url);
  NavigateToURL(url2);

  ASSERT_TRUE(GetActiveTab()->GoBack());

  QuitBrowserAndRestore();

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count) &&
              window_count == 1);
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count) && tab_count == 1);
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

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

  QuitBrowserAndRestore();
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
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

  ASSERT_TRUE(GetActiveTabURL() == url2);

  ASSERT_TRUE(browser_proxy->ActivateTab(0));
  tab_proxy.reset(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

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

  browser_proxy->GetActiveTab()->Close(true);

  QuitBrowserAndRestore();
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
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_GOOFFTHERECORD));
  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec) &&
              window_count == 2);

  // Close the first window.
  CloseWindow(0, 2);
  browser_proxy.reset();

  // Launch the browser again. Note, this doesn't spawn a new process, instead
  // it attaches to the current process.
  include_testing_id_ = false;
  use_existing_browser_ = true;
  clear_profile_ = false;
  CommandLine::AppendSwitch(&launch_arguments_, switches::kRestoreLastSession);
  LaunchBrowser(launch_arguments_, false);

  // A new window should appear;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec) &&
              window_count == 2);

  // And it shouldn't have url1 in it.
  browser_proxy.reset(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser_proxy.get());
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));
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
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(SW_SHOWNORMAL));
  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec) &&
              window_count == 2);

  // Close it.
  CloseWindow(1, 2);

  // Restart and make sure we have only one window with one tab and the url
  // is url1.
  QuitBrowserAndRestore();

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
  std::wstring app_launch_arguments = launch_arguments_;
  CommandLine::AppendSwitchWithValue(
      &app_launch_arguments, switches::kApp, UTF8ToWide(url2.spec()));
  LaunchBrowser(app_launch_arguments, false);
  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec));
  ASSERT_EQ(2, window_count);

  // Close the first window.
  CloseWindow(0, 2);

  // Restore it, which should bring back the first window with url1.
  std::wstring restore_launch_arguments = launch_arguments_;
  CommandLine::AppendSwitch(&restore_launch_arguments,
                            switches::kRestoreLastSession);
  LaunchBrowser(restore_launch_arguments, false);
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec));
  ASSERT_EQ(2, window_count);
  GURL url;
  AssertWindowHasOneTab(1, &url);
  ASSERT_EQ(url1, url);
}

// Make sure after a restore the number of processes matches that of the number
// of processes running before the restore. This creates a new tab so that
// we should have two new tabs running.  (This test will pass in both
// process-per-site and process-per-site-instance, because we treat the new tab
// as a special case in process-per-site-instance so that it only ever uses one
// process.)
TEST_F(SessionRestoreUITest, ShareProcessesOnRestore) {
  if (in_process_renderer()) {
    // No point in running this test in single process mode.
    return;
  }

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  printf("check point\n");
  ASSERT_TRUE(browser_proxy.get() != NULL);
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  printf("check point\n");

  // Create two new tabs.
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_NEWTAB));
  printf("check point\n");
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(tab_count, &new_tab_count,
                                                     kWaitForActionMsec));
  printf("check point\n");
  scoped_ptr<TabProxy> last_tab(browser_proxy->GetTab(new_tab_count - 1));
  ASSERT_TRUE(last_tab.get() != NULL);
  // Do a reload to ensure new tab page has loaded.
  printf("check point\n");
  ASSERT_TRUE(last_tab->Reload());
  tab_count = new_tab_count;
  printf("check point\n");
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_NEWTAB));
  printf("check point\n");
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(tab_count, &new_tab_count,
                                                     kWaitForActionMsec));
  printf("check point\n");
  last_tab.reset(browser_proxy->GetTab(new_tab_count - 1));
  ASSERT_TRUE(last_tab.get() != NULL);
  printf("check point\n");
  // Do a reload to ensure new tab page has loaded.
  ASSERT_TRUE(last_tab->Reload());
  int expected_process_count = GetBrowserProcessCount();

  // Restart.
  browser_proxy.reset();
  last_tab.reset();
  printf("check point\n");
  QuitBrowserAndRestore();

  printf("check point\n");
  // Wait for each tab to finish being restored, then make sure the process
  // count matches.
  browser_proxy.reset(automation()->GetBrowserWindow(0));
  printf("check point\n");
  ASSERT_TRUE(browser_proxy.get() != NULL);
  int restored_tab_count;
  printf("check point\n");
  ASSERT_TRUE(browser_proxy->GetTabCount(&restored_tab_count));
  printf("check point\n");
  ASSERT_EQ(new_tab_count, restored_tab_count);

  printf("check point\n");
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(restored_tab_count - 2));
  printf("check point\n");
  ASSERT_TRUE(tab_proxy.get() != NULL);
  printf("check point\n");
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));
  printf("check point\n");
  tab_proxy.reset(browser_proxy->GetTab(restored_tab_count - 1));
  printf("check point\n");
  ASSERT_TRUE(tab_proxy.get() != NULL);
  printf("check point\n");
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));

  printf("check point\n");
  ASSERT_EQ(expected_process_count, GetBrowserProcessCount());
}
