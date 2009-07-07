// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
#if defined(OS_WIN) || defined(OS_LINUX)
  if (page_title.empty())
    return l10n_util::GetString(IDS_PRODUCT_NAME);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
#elif defined(OS_MACOSX)
  // On Mac, we don't want to suffix the page title with the application name.
  if (page_title.empty())
    return l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  return page_title;
#endif
}

class BrowserTest : public UITest {
 protected:
#if defined(OS_WIN)
  HWND GetMainWindow() {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    scoped_refptr<WindowProxy> window(browser->GetWindow());

    HWND window_handle;
    EXPECT_TRUE(window->GetHWND(&window_handle));
    return window_handle;
  }
#endif

  std::wstring GetWindowTitle() {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    scoped_refptr<WindowProxy> window(browser->GetWindow());

    string16 title;
    EXPECT_TRUE(window->GetWindowTitle(&title));
    return UTF16ToWide(title);
  }

  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::wstring LocaleWindowCaptionFromPageTitle(
      const std::wstring& expected_title) {
    std::wstring page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
    string16  browser_locale;

    EXPECT_TRUE(automation()->GetBrowserLocale(&browser_locale));

    const std::string& locale_utf8 = UTF16ToUTF8(browser_locale);
    if (l10n_util::GetTextDirectionForLocale(locale_utf8.c_str()) ==
        l10n_util::RIGHT_TO_LEFT) {
      l10n_util::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }
};

class VisibleBrowserTest : public UITest {
 protected:
  VisibleBrowserTest() : UITest() {
    show_window_ = true;
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
TEST_F(BrowserTest, NoTitle) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  // The browser lazily updates the title.
  PlatformThread::Sleep(sleep_timeout_ms());
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(L"title1.html"), GetWindowTitle());
  EXPECT_EQ(L"title1.html", GetActiveTabTitle());
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
TEST_F(BrowserTest, Title) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title2.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  // The browser lazily updates the title.
  PlatformThread::Sleep(sleep_timeout_ms());

  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title), GetWindowTitle());
  EXPECT_EQ(test_title, GetActiveTabTitle());
}

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
TEST_F(BrowserTest, ThirtyFourTabs) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title2.html");
  GURL url(net::FilePathToFileURL(test_file));
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  // There is one initial tab.
  for (int ix = 0; ix != 33; ++ix) {
    EXPECT_TRUE(window->AppendTab(url));
  }
  int tab_count = 0;
  EXPECT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(34, tab_count);
  // Do not test the rest in single process mode.
  if (in_process_renderer())
    return;
  // See browser\renderer_host\render_process_host.cc for the algorithm to
  // decide how many processes to create.
  int process_count = GetBrowserProcessCount();
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(process_count, 24);
  } else {
    EXPECT_LE(process_count, 23);
  }
}

#if defined(OS_WIN)
// The browser should quit quickly if it receives a WM_ENDSESSION message.
TEST_F(BrowserTest, WindowsSessionEnd) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  PlatformThread::Sleep(action_timeout_ms());

  // Simulate an end of session. Normally this happens when the user
  // shuts down the pc or logs off.
  HWND window_handle = GetMainWindow();
  ASSERT_TRUE(::PostMessageW(window_handle, WM_ENDSESSION, 0, 0));

  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_FALSE(IsBrowserRunning());

  // Make sure the UMA metrics say we didn't crash.
  scoped_ptr<DictionaryValue> local_prefs(GetLocalState());
  bool exited_cleanly;
  ASSERT_TRUE(local_prefs.get());
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilityExitedCleanly,
                                      &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);

  // And that session end was successful.
  bool session_end_completed;
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilitySessionEndCompleted,
                                      &session_end_completed));
  ASSERT_TRUE(session_end_completed);

  // Make sure session restore says we didn't crash.
  scoped_ptr<DictionaryValue> profile_prefs(GetDefaultProfilePreferences());
  ASSERT_TRUE(profile_prefs.get());
  ASSERT_TRUE(profile_prefs->GetBoolean(prefs::kSessionExitedCleanly,
                                        &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);
}
#endif

// This test is flakey, see bug 5668 for details.
TEST_F(BrowserTest, DISABLED_JavascriptAlertActivatesTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  int start_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&start_index));
  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));
  int javascript_tab_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&javascript_tab_index));
  scoped_refptr<TabProxy> javascript_tab = window->GetActiveTab();
  ASSERT_TRUE(javascript_tab.get());
  // Switch back to the starting tab, then send the second tab a javascript
  // alert, which should force it to become active.
  ASSERT_TRUE(window->ActivateTab(start_index));
  ASSERT_TRUE(
      javascript_tab->NavigateToURLAsync(GURL("javascript:alert('Alert!')")));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(javascript_tab_index,
                                               action_max_timeout_ms()));
}

// Test that scripts can fork a new renderer process for a tab in a particular
// case (which matches following a link in Gmail).  The script must open a new
// tab, set its window.opener to null, and redirect it to a cross-site URL.
// (Bug 1115708)
// This test can only run if V8 is in use, and not KJS, because KJS will not
// set window.opener to null properly.
#ifdef CHROME_V8
TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to "fork" a new tab, just like Gmail.  (Open tab to a
  // blank page, set its opener to null, and redirect it cross-site.)
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL fork_url(url_prefix +
      L"w.opener=null;w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab has been created and that we have a new renderer
  // process for it.
  ASSERT_TRUE(tab->NavigateToURLAsync(fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count + 1, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);
}
#endif

#if !defined(OS_LINUX)
// TODO(port): This passes on linux locally, but fails on the try bot.
// Tests that non-Gmail-like script redirects (i.e., non-null window.opener) or
// a same-page-redirect) will not fork a new process.
TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to almost fork a new tab, but not quite.  (Leave the
  // opener non-null.)  Should not fork a process.
  std::string url_prefix("javascript:(function(){w=window.open();");
  GURL dont_fork_url(url_prefix +
      "w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab but not new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  GURL dont_fork_url2(url_prefix +
      "document.location=\"http://localhost:1337\";})()");

  // Make sure that no new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url2));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
}
#endif

#if defined(OS_WIN)
// TODO(estade): need to port GetActiveTabTitle().
TEST_F(VisibleBrowserTest, WindowOpenClose) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("window.close.html");

  NavigateToURL(net::FilePathToFileURL(test_file));

  int i;
  for (i = 0; i < 10; ++i) {
    PlatformThread::Sleep(action_max_timeout_ms() / 10);
    std::wstring title = GetActiveTabTitle();
    if (title == L"PASSED") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10)
    FAIL() << "failed to get error page title";
}
#endif

#if defined(OS_WIN)  // only works on Windows for now: http:://crbug.com/15891
class ShowModalDialogTest : public UITest {
 public:
  ShowModalDialogTest() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(ShowModalDialogTest, BasicTest) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("showmodaldialog.html");
  NavigateToURL(net::FilePathToFileURL(test_file));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(1);
  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_EQ(title, L"ModalDialogTitle");
}
#endif

}  // namespace
