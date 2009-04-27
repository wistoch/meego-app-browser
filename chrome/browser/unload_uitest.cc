// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/platform_thread.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/message_box_flags.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

const std::string NOLISTENERS_HTML =
    "<html><head><title>nolisteners</title></head><body></body></html>";

const std::string UNLOAD_HTML =
    "<html><head><title>unload</title></head><body>"
    "<script>window.onunload=function(e){}</script></body></html>";

const std::string BEFORE_UNLOAD_HTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const std::string TWO_SECOND_BEFORE_UNLOAD_HTML =
    "<html><head><title>twosecondbeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "return 'foo';"
    "}</script></body></html>";

const std::string INFINITE_UNLOAD_HTML =
    "<html><head><title>infiniteunload</title></head><body>"
    "<script>window.onunload=function(e){while(true){}}</script>"
    "</body></html>";

const std::string INFINITE_BEFORE_UNLOAD_HTML =
    "<html><head><title>infinitebeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){while(true){}}</script>"
    "</body></html>";

const std::string INFINITE_UNLOAD_ALERT_HTML =
    "<html><head><title>infiniteunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string INFINITE_BEFORE_UNLOAD_ALERT_HTML =
    "<html><head><title>infinitebeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string TWO_SECOND_UNLOAD_ALERT_HTML =
    "<html><head><title>twosecondunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML =
    "<html><head><title>twosecondbeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER =
    "<html><head><title>only_one_unload</title></head>"
    "<body onload=\"window.open('data:text/html,<html><head><title>second_tab</title></head></body>')\" "
    "onbeforeunload='return;'"
    "</body></html>";

class UnloadTest : public UITest {
 public:
  virtual void SetUp() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(),
        "BrowserCloseTabWhenOtherTabHasListener") == 0) {
      launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
    }

    UITest::SetUp();
  }

  void WaitForBrowserClosed() {
    const int kCheckDelayMs = 100;
    int max_wait_time = 5000;
    while (max_wait_time > 0) {
      max_wait_time -= kCheckDelayMs;
      PlatformThread::Sleep(kCheckDelayMs);
      if (!IsBrowserRunning())
        break;
    }
  }

  void CheckTitle(const std::wstring& expected_title) {
    const int kCheckDelayMs = 100;
    int max_wait_time = 5000;
    while (max_wait_time > 0) {
      max_wait_time -= kCheckDelayMs;
      PlatformThread::Sleep(kCheckDelayMs);
      if (expected_title == GetActiveTabTitle())
        break;
    }

    EXPECT_EQ(expected_title, GetActiveTabTitle());
  }

  void NavigateToDataURL(const std::string& html_content,
                         const std::wstring& expected_title) {
    NavigateToURL(GURL("data:text/html," + html_content));
    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
    CheckTitle(L"Title Of Awesomeness");
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
    CheckTitle(L"Title Of Awesomeness");
  }

  // Navigates to a URL asynchronously, then again synchronously. The first
  // load is purposely async to test the case where the user loads another
  // page without waiting for the first load to complete.
  void NavigateToNolistenersFileTwiceAsync() {
    // TODO(ojan): We hit a DCHECK in RenderViewHost::OnMsgShouldCloseACK
    // if we don't sleep here.
    PlatformThread::Sleep(400);
    NavigateToURLAsync(
        URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
    PlatformThread::Sleep(400);
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));

    CheckTitle(L"Title Of Awesomeness");
  }

  void LoadUrlAndQuitBrowser(const std::string& html_content,
                             const std::wstring& expected_title = L"") {
    scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    NavigateToDataURL(html_content, expected_title);
    bool application_closed = false;
    EXPECT_TRUE(CloseBrowser(browser.get(), &application_closed));
  }

  void ClickModalDialogButton(MessageBoxFlags::DialogButton button) {
#if defined(OS_WIN) || defined(OS_LINUX)
    bool modal_dialog_showing = false;
    MessageBoxFlags::DialogButton available_buttons;
    EXPECT_TRUE(automation()->WaitForAppModalDialog(3000));
    EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
        &available_buttons));
    ASSERT_TRUE(modal_dialog_showing);
    EXPECT_TRUE((button & available_buttons) != 0);
    EXPECT_TRUE(automation()->ClickAppModalDialogButton(button));
#else
    // TODO(port): port this function if and when the tests that use it are
    // enabled (currently they are not being run even on windows).
    NOTIMPLEMENTED();
#endif
  }
};

// TODO(port): these tests fail on linux because they leave a renderer process
// lying around which holds onto the user data directory.
#if defined(OS_WIN)
// Navigate to a page with an infinite unload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
TEST_F(UnloadTest, CrossSiteInfiniteUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, L"infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite unload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one.
TEST_F(UnloadTest, CrossSiteInfiniteUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, L"infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one.
TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}
#endif

// Tests closing the browser on a page with no unload listeners registered.
TEST_F(UnloadTest, BrowserCloseNoUnloadListeners) {
  LoadUrlAndQuitBrowser(NOLISTENERS_HTML, L"nolisteners");
}

// Tests closing the browser on a page with an unload listener registered.
TEST_F(UnloadTest, BrowserCloseUnload) {
  LoadUrlAndQuitBrowser(UNLOAD_HTML, L"unload");
}

// Tests closing the browser with a beforeunload handler and clicking
// OK in the beforeunload confirm dialog.
TEST_F(UnloadTest, BrowserCloseBeforeUnloadOK) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  NavigateToDataURL(BEFORE_UNLOAD_HTML, L"beforeunload");

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(MessageBoxFlags::DIALOGBUTTON_OK);
  WaitForBrowserClosed();
  EXPECT_FALSE(IsBrowserRunning());
}

// Tests closing the browser with a beforeunload handler and clicking
// CANCEL in the beforeunload confirm dialog.
TEST_F(UnloadTest, BrowserCloseBeforeUnloadCancel) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  NavigateToDataURL(BEFORE_UNLOAD_HTML, L"beforeunload");

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(MessageBoxFlags::DIALOGBUTTON_CANCEL);
  WaitForBrowserClosed();
  EXPECT_TRUE(IsBrowserRunning());

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(MessageBoxFlags::DIALOGBUTTON_OK);
  WaitForBrowserClosed();
  EXPECT_FALSE(IsBrowserRunning());
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run.
TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnload) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_HTML,
                        L"twosecondbeforeunload");
}

// TODO(estade): On linux, the renderer process doesn't seem to quit and pegs
// CPU.
#if defined(OS_WIN)
// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop.
TEST_F(UnloadTest, BrowserCloseInfiniteUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_HTML, L"infiniteunload");
}

// Tests closing the browser with a beforeunload handler that hangs.
TEST_F(UnloadTest, BrowserCloseInfiniteBeforeUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop followed by an alert.
TEST_F(UnloadTest, BrowserCloseInfiniteUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_ALERT_HTML, L"infiniteunloadalert");
}

// Tests closing the browser with a beforeunload handler that hangs then
// pops up an alert.
TEST_F(UnloadTest, BrowserCloseInfiniteBeforeUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_ALERT_HTML,
                        L"infinitebeforeunloadalert");
}
#endif  // defined(OS_WIN)

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an 2 second long loop followed by an alert.
TEST_F(UnloadTest, BrowserCloseTwoSecondUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_UNLOAD_ALERT_HTML, L"twosecondunloadalert");
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run then pops up an alert.
TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML,
                        L"twosecondbeforeunloadalert");
}

// Tests that if there's a renderer process with two tabs, one of which has an
// unload handler, and the other doesn't, the tab that doesn't have an unload
// handler can be closed.  If this test fails, the Close() call will hang.
TEST_F(UnloadTest, BrowserCloseTabWhenOtherTabHasListener) {
  NavigateToDataURL(CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER, L"second_tab");

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());

  int tab_count;
  EXPECT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(tab_count, 2);

  scoped_ptr<TabProxy> second_tab(browser_proxy->GetActiveTab());
  EXPECT_TRUE(second_tab.get()!= NULL);
  EXPECT_TRUE(second_tab->Close(true));

  scoped_ptr<TabProxy> first_tab(browser_proxy->GetActiveTab());
  std::wstring title;
  EXPECT_TRUE(first_tab.get() != NULL);
  EXPECT_TRUE(first_tab->GetTabTitle(&title));
  EXPECT_EQ(title, L"only_one_unload");
}

// TODO(ojan): Add tests for unload/beforeunload that have multiple tabs
// and multiple windows.
