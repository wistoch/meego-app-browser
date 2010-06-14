// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/app_switches.h"
#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/i18n/rtl.h"
#include "base/keyboard_codes.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "gfx/rect.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"
#include "views/event.h"

using ui_test_utils::TimedMessageLoopRunner;
using testing::CreateFunctor;
using testing::StrEq;
using testing::_;

class AutomationProxyTest : public UITest {
 protected:
  AutomationProxyTest() {
    dom_automation_enabled_ = true;
    launch_arguments_.AppendSwitchWithValue(switches::kLang,
                                            L"en-US");
  }
};

TEST_F(AutomationProxyTest, GetBrowserWindowCount) {
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
#ifdef NDEBUG
  ASSERT_FALSE(automation()->GetBrowserWindowCount(NULL));
#endif
}

TEST_F(AutomationProxyTest, GetBrowserWindow) {
  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(-1));
    ASSERT_FALSE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(1));
    ASSERT_FALSE(window.get());
  }
};

#if defined(OS_MACOSX)
// Missing automation provider support: http://crbug.com/45892
#define MAYBE_WindowGetViewBounds FAILS_WindowGetViewBounds
#else
#define MAYBE_WindowGetViewBounds WindowGetViewBounds
#endif
TEST_F(AutomationProxyVisibleTest, MAYBE_WindowGetViewBounds) {
  {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    scoped_refptr<WindowProxy> window(browser->GetWindow());
    ASSERT_TRUE(window.get());

    scoped_refptr<TabProxy> tab1(browser->GetTab(0));
    ASSERT_TRUE(tab1.get());
    GURL tab1_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

    // Add another tab so we can simulate dragging.
    ASSERT_TRUE(browser->AppendTab(GURL("about:")));

    scoped_refptr<TabProxy> tab2(browser->GetTab(1));
    ASSERT_TRUE(tab2.get());
    GURL tab2_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_url));

    EXPECT_NE(tab1_url.spec(), tab2_url.spec());

    gfx::Rect bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds, false));
    EXPECT_GT(bounds.width(), 0);
    EXPECT_GT(bounds.height(), 0);

    gfx::Rect bounds2;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_LAST, &bounds2, false));
    EXPECT_GT(bounds2.x(), 0);
    EXPECT_GT(bounds2.width(), 0);
    EXPECT_GT(bounds2.height(), 0);

    // The tab logic is mirrored in RTL locales, so what is to the right in
    // LTR locales is now on the left with RTL ones.
    string16 browser_locale;

    EXPECT_TRUE(automation()->GetBrowserLocale(&browser_locale));

    const std::string& locale_utf8 = UTF16ToUTF8(browser_locale);
    if (base::i18n::GetTextDirectionForLocale(locale_utf8.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      EXPECT_LT(bounds2.x(), bounds.x());
    } else {
      EXPECT_GT(bounds2.x(), bounds.x());
    }
    EXPECT_EQ(bounds2.y(), bounds.y());

    gfx::Rect urlbar_bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                      false));
    EXPECT_GT(urlbar_bounds.x(), 0);
    EXPECT_GT(urlbar_bounds.y(), 0);
    EXPECT_GT(urlbar_bounds.width(), 0);
    EXPECT_GT(urlbar_bounds.height(), 0);

    /*

    TODO(beng): uncomment this section or move to interactive_ui_tests post
    haste!

    // Now that we know where the tabs are, let's try dragging one.
    POINT start;
    POINT end;
    start.x = bounds.x() + bounds.width() / 2;
    start.y = bounds.y() + bounds.height() / 2;
    end.x = start.x + 2 * bounds.width() / 3;
    end.y = start.y;
    ASSERT_TRUE(browser->SimulateDrag(start, end,
                                      views::Event::EF_LEFT_BUTTON_DOWN));

    // Check to see that the drag event successfully swapped the two tabs.
    tab1 = browser->GetTab(0);
    ASSERT_TRUE(tab1.get());
    GURL tab1_new_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

    tab2 = browser->GetTab(1);
    ASSERT_TRUE(tab2.get());
    GURL tab2_new_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

    EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
    EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());

    */
  }
}

TEST_F(AutomationProxyTest, GetTabCount) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = 0;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
}

TEST_F(AutomationProxyTest, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyVisibleTest, AppendTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int original_tab_count;
  ASSERT_TRUE(window->GetTabCount(&original_tab_count));
  ASSERT_EQ(1, original_tab_count);  // By default there are 2 tabs opened.

  int original_active_tab_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&original_active_tab_index));
  ASSERT_EQ(0, original_active_tab_index);  // By default 0-th tab is active

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));
  int tab_count;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(original_tab_count + 1, tab_count);

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(tab_count - 1, active_tab_index);
  ASSERT_NE(original_active_tab_index, active_tab_index);

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(filename)));

  int appended_tab_index;
  // Append tab will also be active tab
  ASSERT_TRUE(window->GetActiveTabIndex(&appended_tab_index));

  scoped_refptr<TabProxy> tab(window->GetTab(appended_tab_index));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, ActivateTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));

  ASSERT_TRUE(window->ActivateTab(1));
  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  ASSERT_TRUE(window->ActivateTab(0));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyTest, GetTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  {
    scoped_refptr<TabProxy> tab(window->GetTab(0));
    ASSERT_TRUE(tab.get());
    std::wstring title;
    ASSERT_TRUE(tab->GetTabTitle(&title));
    // BUG [634097] : expected title should be "about:blank"
    ASSERT_STREQ(L"", title.c_str());
  }

  {
    ASSERT_FALSE(window->GetTab(-1));
  }

  {
    scoped_refptr<TabProxy> tab(window->GetTab(1));
    ASSERT_FALSE(tab.get());
  }
};

TEST_F(AutomationProxyTest, NavigateToURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(filename)));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  // TODO(vibhor) : Add a test using testserver.
}

TEST_F(AutomationProxyTest, GoBackForward) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  ASSERT_FALSE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_TRUE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  ASSERT_TRUE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_FALSE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, GetCurrentURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  GURL url;
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  ASSERT_STREQ("about:blank", url.spec().c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);
  ASSERT_TRUE(tab->NavigateToURL(newurl));
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  // compare canonical urls...
  ASSERT_STREQ(newurl.spec().c_str(), url.spec().c_str());
}

class AutomationProxyTest2 : public AutomationProxyVisibleTest {
 protected:
  AutomationProxyTest2() {
    document1_= test_data_directory_.AppendASCII("title1.html");

    document2_ = test_data_directory_.AppendASCII("title2.html");
    launch_arguments_ = CommandLine(CommandLine::ARGUMENTS_ONLY);
    launch_arguments_.AppendLooseValue(document1_.ToWStringHack());
    launch_arguments_.AppendLooseValue(document2_.ToWStringHack());
  }

  FilePath document1_;
  FilePath document2_;
};

TEST_F(AutomationProxyTest2, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);

  ASSERT_TRUE(window->ActivateTab(1));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);
}

TEST_F(AutomationProxyTest2, GetTabTitle) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"title1.html", title.c_str());

  tab = window->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, Cookies) {
  GURL url("http://mojo.jojo.google.com");
  std::string value_result;

  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  // test setting the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo=baz"));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("baz", value_result.c_str());

  // test clearing the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo="));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_TRUE(value_result.empty());

  // now, test that we can get multiple cookies:
  ASSERT_TRUE(tab->SetCookie(url, "foo1=baz1"));
  ASSERT_TRUE(tab->SetCookie(url, "foo2=baz2"));

  ASSERT_TRUE(tab->GetCookies(url, &value_result));
  ASSERT_FALSE(value_result.empty());
  EXPECT_TRUE(value_result.find("foo1=baz1") != std::string::npos);
  EXPECT_TRUE(value_result.find("foo2=baz2") != std::string::npos);

  // test deleting cookie
  ASSERT_TRUE(tab->SetCookie(url, "foo3=deleteme"));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo3", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("deleteme", value_result.c_str());

  ASSERT_TRUE(tab->DeleteCookie(url, "foo3"));
}

TEST_F(AutomationProxyTest, NavigateToURLAsync) {
  AutomationProxy* automation_object = automation();
  scoped_refptr<BrowserProxy> window(automation_object->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);

  ASSERT_TRUE(tab->NavigateToURLAsync(newurl));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), newurl, "foo",
                                              action_max_timeout_ms());
  ASSERT_STREQ("baz", value.c_str());
}

TEST_F(AutomationProxyTest, AcceleratorNewTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = -1;
  ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(2, tab_count);

  scoped_refptr<TabProxy> tab(window->GetTab(1));
  ASSERT_TRUE(tab.get());
}

TEST_F(AutomationProxyTest, AcceleratorDownloads) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_SHOW_DOWNLOADS));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"Downloads", GetActiveTabTitle());
}

TEST_F(AutomationProxyTest, AcceleratorExtensions) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_MANAGE_EXTENSIONS));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"Extensions", GetActiveTabTitle());
}

TEST_F(AutomationProxyTest, AcceleratorHistory) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_SHOW_HISTORY));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"History", GetActiveTabTitle());
}

class AutomationProxyTest4 : public UITest {
 protected:
  AutomationProxyTest4() : UITest() {
    dom_automation_enabled_ = true;
  }
};

std::wstring CreateJSString(const std::wstring& value) {
  std::wstring jscript;
  SStringPrintf(&jscript,
      L"window.domAutomationController.send(%ls);",
      value.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest4, StringValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_STREQ(expected.c_str(), actual.c_str());
}

std::wstring BooleanToString(bool bool_value) {
  Value* value = Value::CreateBooleanValue(bool_value);
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(*value);
  return UTF8ToWide(json_string);
}

TEST_F(AutomationProxyTest4, BooleanValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool expected = true;
  std::wstring jscript = CreateJSString(BooleanToString(expected));
  bool actual = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

TEST_F(AutomationProxyTest4, NumberValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  int expected = 1;
  int actual = 0;
  std::wstring expected_string;
  SStringPrintf(&expected_string, L"%d", expected);
  std::wstring jscript = CreateJSString(expected_string);
  ASSERT_TRUE(tab->ExecuteAndExtractInt(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

// TODO(vibhor): Add a test for ExecuteAndExtractValue() for JSON Dictionary
// type value

class AutomationProxyTest3 : public UITest {
 protected:
  AutomationProxyTest3() : UITest() {
    document1_ = test_data_directory_;
    document1_ = document1_.AppendASCII("frame_dom_access");
    document1_ = document1_.AppendASCII("frame_dom_access.html");

    dom_automation_enabled_ = true;
    launch_arguments_ = CommandLine(CommandLine::ARGUMENTS_ONLY);
    launch_arguments_.AppendLooseValue(document1_.ToWStringHack());
  }

  FilePath document1_;
};

std::wstring CreateJSStringForDOMQuery(const std::wstring& id) {
  std::wstring jscript(L"window.domAutomationController");
  StringAppendF(&jscript, L".send(document.getElementById('%ls').nodeName);",
                id.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest3, FrameDocumentCanBeAccessed) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring actual;
  std::wstring xpath1 = L"";  // top level frame
  std::wstring jscript1 = CreateJSStringForDOMQuery(L"myinput");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath1, jscript1, &actual));
  ASSERT_EQ(L"INPUT", actual);

  std::wstring xpath2 = L"/html/body/iframe";
  std::wstring jscript2 = CreateJSStringForDOMQuery(L"myspan");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath2, jscript2, &actual));
  ASSERT_EQ(L"SPAN", actual);

  std::wstring xpath3 = L"/html/body/iframe\n/html/body/iframe";
  std::wstring jscript3 = CreateJSStringForDOMQuery(L"mydiv");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath3, jscript3, &actual));
  ASSERT_EQ(L"DIV", actual);
}

TEST_F(AutomationProxyTest, BlockedPopupTest) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("constrained_files");
  filename = filename.AppendASCII("constrained_window.html");

  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  ASSERT_TRUE(tab->WaitForBlockedPopupCountToChangeTo(2,
                                                      action_max_timeout_ms()));
}

// TODO(port): Remove HWND if possible
#if defined(OS_WIN)

const char simple_data_url[] =
    "data:text/html,<html><head><title>External tab test</title></head>"
    "<body>A simple page for testing a floating/invisible tab<br></div>"
    "</body></html>";

ExternalTabUITestMockClient::ExternalTabUITestMockClient(int execution_timeout)
    : AutomationProxy(execution_timeout),
      host_window_style_(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE),
      host_window_(NULL) {
}

void ExternalTabUITestMockClient::ReplyStarted(
    const IPC::AutomationURLResponse* response,
    int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestStarted(0, tab_handle,
      request_id, *response));
}

void ExternalTabUITestMockClient::ReplyData(
    const std::string* data, int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestData(0, tab_handle,
                                                      request_id, *data));
}

void ExternalTabUITestMockClient::ReplyEOF(int tab_handle, int request_id) {
  ReplyEnd(URLRequestStatus(), tab_handle, request_id);
}

void ExternalTabUITestMockClient::ReplyEnd(const URLRequestStatus& status,
                                           int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestEnd(0, tab_handle,
                                                     request_id, status));
}

void ExternalTabUITestMockClient::Reply404(int tab_handle, int request_id) {
  const IPC::AutomationURLResponse notfound = {"", "HTTP/1.1 404\r\n\r\n"};
  ReplyStarted(&notfound, tab_handle, request_id);
  ReplyEOF(tab_handle, request_id);
}

void ExternalTabUITestMockClient::ServeHTMLData(int tab_handle,
                                                const GURL& url,
                                                const std::string& data) {
  EXPECT_CALL(*this, OnRequestStart(tab_handle, _, testing::AllOf(
    testing::Field(&IPC::AutomationURLRequest::url, url.spec()),
      testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
    .Times(1)
    .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(this,
        &ExternalTabUITestMockClient::ReplyStarted, &http_200))));

  EXPECT_CALL(*this, OnRequestRead(tab_handle, _, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(this,
          &ExternalTabUITestMockClient::ReplyData, &data))))
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(this,
          &ExternalTabUITestMockClient::ReplyEOF))));
}

void ExternalTabUITestMockClient::IgnoreFavIconNetworkRequest() {
  // Ignore favicon.ico
  EXPECT_CALL(*this, OnRequestStart(_, _, testing::AllOf(
          testing::Field(&IPC::AutomationURLRequest::url,
                         testing::EndsWith("favicon.ico")),
          testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
          CreateFunctor(this, &ExternalTabUITestMockClient::ReplyEnd,
                URLRequestStatus(URLRequestStatus::FAILED, 0)))));
}

void ExternalTabUITestMockClient::InvalidateHandle(
    const IPC::Message& message) {
  void* iter = NULL;
  int handle;
  ASSERT_TRUE(message.ReadInt(&iter, &handle));

  // Call base class
  AutomationProxy::InvalidateHandle(message);
  HandleClosed(handle);
}

// Most of the time we need external tab with these settings.
const IPC::ExternalTabSettings ExternalTabUITestMockClient::default_settings = {
  NULL, gfx::Rect(),  // will be replaced by CreateHostWindowAndTab
  WS_CHILD | WS_VISIBLE,
  false,  // is_off_the_record
  true,   // load_requests_via_automation
  true,  // handle_top_level_requests
  GURL()  // initial_url
};

// static
const IPC::AutomationURLResponse ExternalTabUITestMockClient::http_200 =
  {"", "HTTP/0.9 200\r\n\r\n", };

void ExternalTabUITestMockClient::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(ExternalTabUITestMockClient, msg)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate, OnDidNavigate)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
        OnForwardMessageToExternalHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStart, OnRequestStart)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestRead, OnRequestRead)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookieAsync, OnSetCookieAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabLoaded, OnLoad)
    IPC_MESSAGE_HANDLER(AutomationMsg_AttachExternalTab, OnAttachExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationStateChanged,
        OnNavigationStateChanged)
  IPC_END_MESSAGE_MAP()
}

scoped_refptr<TabProxy> ExternalTabUITestMockClient::CreateHostWindowAndTab(
    const IPC::ExternalTabSettings& settings) {
  EXPECT_THAT(settings.parent, testing::IsNull());

  host_window_ = CreateWindowW(L"Button", NULL, host_window_style_,
      CW_USEDEFAULT,  CW_USEDEFAULT, CW_USEDEFAULT,  CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);
  EXPECT_THAT(host_window_, testing::Truly(::IsWindow));
  RECT client_area = {0};
  ::GetClientRect(host_window_, &client_area);

  IPC::ExternalTabSettings s = settings;
  s.parent = host_window_;
  s.dimensions = client_area;

  HWND container_wnd = NULL;
  HWND tab_wnd = NULL;
  scoped_refptr<TabProxy> tab(CreateExternalTab(s, &container_wnd, &tab_wnd));

  EXPECT_TRUE(tab != NULL);
  EXPECT_NE(FALSE, ::IsWindow(container_wnd));
  EXPECT_NE(FALSE, ::IsWindow(tab_wnd));
  return tab;
}

scoped_refptr<TabProxy> ExternalTabUITestMockClient::CreateTabWithUrl(
    const GURL& initial_url) {
  IPC::ExternalTabSettings settings =
      ExternalTabUITestMockClient::default_settings;
  settings.initial_url = initial_url;
  return CreateHostWindowAndTab(settings);
}

void ExternalTabUITestMockClient::NavigateInExternalTab(int tab_handle,
    const GURL& url, const GURL& referrer /* = GURL()*/) {
  channel_->ChannelProxy::Send(new AutomationMsg_NavigateInExternalTab(0,
        tab_handle, url, referrer, NULL));
}

void ExternalTabUITestMockClient::ConnectToExternalTab(gfx::NativeWindow parent,
    const IPC::AttachExternalTabParams& attach_params) {
  gfx::NativeWindow tab_container = NULL;
  gfx::NativeWindow tab_window = NULL;
  int tab_handle = 0;

  IPC::SyncMessage* message = new AutomationMsg_ConnectExternalTab(0,
      attach_params.cookie, true, &tab_container, &tab_window, &tab_handle);
  channel_->Send(message);

  RECT rect;
  ::GetClientRect(parent, &rect);
  IPC::Reposition_Params params = {0};
  params.window = tab_container;
  params.flags = SWP_NOZORDER | SWP_SHOWWINDOW;
  params.width = rect.right - rect.left;
  params.height = rect.bottom - rect.top;
  params.set_parent = true;
  params.parent_window = parent;

  channel_->Send(new AutomationMsg_TabReposition(0, tab_handle, params));
  ::ShowWindow(parent, SW_SHOWNORMAL);
}

void ExternalTabUITestMockClient::NavigateThroughUserGesture() {
  ASSERT_THAT(host_window_, testing::Truly(::IsWindow));
  HWND tab_container = ::GetWindow(host_window_, GW_CHILD);
  ASSERT_THAT(tab_container, testing::Truly(::IsWindow));
  HWND tab = ::GetWindow(tab_container, GW_CHILD);
  ASSERT_THAT(tab, testing::Truly(::IsWindow));
  HWND renderer_window = ::GetWindow(tab, GW_CHILD);
  ASSERT_THAT(renderer_window, testing::Truly(::IsWindow));
  ::SetFocus(renderer_window);
  ::PostMessage(renderer_window, WM_KEYDOWN, VK_TAB, 0);
  ::PostMessage(renderer_window, WM_KEYUP, VK_TAB, 0);
  ::PostMessage(renderer_window, WM_KEYDOWN, VK_RETURN, 0);
  ::PostMessage(renderer_window, WM_KEYUP, VK_RETURN, 0);
}

void ExternalTabUITestMockClient::DestroyHostWindow() {
  ::DestroyWindow(host_window_);
  host_window_ = NULL;
}

bool ExternalTabUITestMockClient::HostWindowExists() {
  return (host_window_ != NULL) && ::IsWindow(host_window_);
}

// Handy macro
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
    CreateFunctor(loop, &TimedMessageLoopRunner::Quit))
#define QUIT_LOOP_SOON(loop, ms) testing::InvokeWithoutArgs(\
    CreateFunctor(loop, &TimedMessageLoopRunner::QuitAfter, ms))

template <typename T> T** ReceivePointer(scoped_ptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

template <typename T> T** ReceivePointer(scoped_refptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

AutomationProxy* ExternalTabUITest::CreateAutomationProxy(int exec_timeout) {
  mock_ = new ExternalTabUITestMockClient(exec_timeout);
  return mock_;
}

// Create with specifying a url
// Flaky, http://crbug.com/32293.
TEST_F(ExternalTabUITest, FLAKY_CreateExternalTab1) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(mock_,
                &ExternalTabUITestMockClient::DestroyHostWindow));

  EXPECT_CALL(*mock_, HandleClosed(1))
      .Times(1)
      .WillOnce(QUIT_LOOP(&loop));

  tab = mock_->CreateTabWithUrl(GURL(simple_data_url));
  loop.RunFor(action_max_timeout_ms());
}

// Create with empty url and then navigate
TEST_F(ExternalTabUITest, FLAKY_CreateExternalTab2) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_, testing::_))
    .Times(1)
    .WillOnce(testing::InvokeWithoutArgs(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow));

  EXPECT_CALL(*mock_, HandleClosed(1))
    .Times(1)
    .WillOnce(QUIT_LOOP(&loop));

  tab = mock_->CreateTabWithUrl(GURL());
  mock_->NavigateInExternalTab(tab->handle(), GURL(simple_data_url));
  loop.RunFor(action_max_timeout_ms());
}

TEST_F(ExternalTabUITest, IncognitoMode) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  GURL url("http://anatomyofmelancholy.net");
  std::string cookie = "robert=burton; expires=Thu, 13 Oct 2011 05:04:03 UTC;";

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  IPC::ExternalTabSettings incognito =
      ExternalTabUITestMockClient::default_settings;
  incognito.is_off_the_record = true;
  // SetCookie is a sync call and deadlock can happen if window is visible,
  // since it shares same thread with AutomationProxy.
  mock_->host_window_style_ &= ~WS_VISIBLE;
  tab = mock_->CreateHostWindowAndTab(incognito);
  std::string value_result;

  EXPECT_TRUE(tab->SetCookie(url, cookie));
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("burton", value_result);
  mock_->DestroyHostWindow();
  CloseBrowserAndServer();
  tab = NULL;

  value_result.clear();
  clear_profile_ = false;
  LaunchBrowserAndServer();
  // SetCookie is a sync call and deadlock can happen if window is visible,
  // since it shares same thread with AutomationProxy.
  mock_->host_window_style_ &= ~WS_VISIBLE;
  tab = mock_->CreateTabWithUrl(GURL());
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("", value_result);
  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);
  mock_->DestroyHostWindow();
  CloseBrowserAndServer();
  tab = NULL;
}

// FLAKY: http://crbug.com/44617
TEST_F(ExternalTabUITest, FLAKY_TabPostMessage) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_, _)).Times(testing::AnyNumber());

  std::string content =
      "data:text/html,<html><head><script>"
      "function onload() {"
      "  window.externalHost.onmessage = onMessage;"
      "}"
      "function onMessage(evt) {"
      "  window.externalHost.postMessage(evt.data, '*');"
      "}"
      "</script></head>"
      "<body onload='onload()'>external tab test<br></div>"
      "</body></html>";

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(
          ReceivePointer(tab),
          &TabProxy::HandleMessageFromExternalHost,
          std::string("Hello from gtest"),
          std::string("null"), std::string("*"))));

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(testing::_,
          testing::StrEq("Hello from gtest"), testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow)),
          QUIT_LOOP_SOON(&loop, 50)));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  tab = mock_->CreateTabWithUrl(GURL(content));
  loop.RunFor(action_max_timeout_ms());
}

TEST_F(ExternalTabUITest, FLAKY_PostMessageTarget)  {
  const wchar_t kDocRoot[] = L"chrome/test/data/external_tab";
  scoped_refptr<HTTPTestServer> server(
      HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_THAT(server.get(), testing::NotNull());

  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_, _)).Times(testing::AnyNumber());

  std::string kTestMessage("Hello from gtest");
  std::string kTestOrigin("http://www.external.tab");

  EXPECT_CALL(*mock_, OnDidNavigate(1, testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(
          ReceivePointer(tab),
          &TabProxy::HandleMessageFromExternalHost,
          kTestMessage, kTestOrigin, std::string("http://localhost:1337/"))));

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(1,
                    testing::StrEq(kTestMessage),
                    testing::_,
                    testing::StrEq(GURL(kTestOrigin).GetOrigin().spec())))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow)),
          QUIT_LOOP_SOON(&loop, 50)));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  IPC::ExternalTabSettings s = ExternalTabUITestMockClient::default_settings;
  s.load_requests_via_automation = false;
  s.initial_url = GURL("http://localhost:1337/files/post_message.html");
  tab = mock_->CreateHostWindowAndTab(s);
  loop.RunFor(action_max_timeout_ms());
}

// Flaky, http://crbug.com/42545.
TEST_F(ExternalTabUITest, FLAKY_HostNetworkStack) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_, _)).Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(1, 2, testing::AllOf(
          testing::Field(&IPC::AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted,
          &ExternalTabUITestMockClient::http_200))));

  // Return some trivial page, that have a link to a "logo.gif" image
  const std::string data = "<!DOCTYPE html><title>Hello</title>"
                           "<img src=\"logo.gif\">";

  EXPECT_CALL(*mock_, OnRequestRead(1, 2, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)))
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyEOF))));

  // Expect navigation is ok.
  EXPECT_CALL(*mock_, OnDidNavigate(1, testing::Field(&IPC::NavigationInfo::url,
                                                      GURL(url))))
      .Times(1);

  // Expect GET request for logo.gif
  EXPECT_CALL(*mock_, OnRequestStart(1, 3, testing::AllOf(
      testing::Field(&IPC::AutomationURLRequest::url, StrEq(url + "/logo.gif")),
      testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::Reply404, 1, 3)));

  EXPECT_CALL(*mock_, OnRequestRead(1, 3, testing::Gt(0)))
    .Times(1);

  // Chrome makes a brave request for favicon.ico
  EXPECT_CALL(*mock_, OnRequestStart(1, 4, testing::AllOf(
      testing::Field(&IPC::AutomationURLRequest::url,
                     StrEq(url + "/favicon.ico")),
      testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::Reply404, 1, 4)),
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow))));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  EXPECT_CALL(*mock_, OnRequestRead(1, 4, testing::Gt(0)))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(&loop, 300));

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(action_max_timeout_ms());
}

TEST_F(ExternalTabUITest, HostNetworkStackAbortRequest) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";
  const IPC::AutomationURLResponse http_200 = {"", "HTTP/0.9 200\r\n\r\n", };

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(1, 2, testing::AllOf(
          testing::Field(&IPC::AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted, &http_200))));

  // Return some trivial page, that have a link to a "logo.gif" image
  const std::string data = "<!DOCTYPE html><title>Hello";

  EXPECT_CALL(*mock_, OnRequestRead(1, 2, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)))
      .WillOnce(testing::WithArgs<0, 1>(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow))));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  EXPECT_CALL(*mock_, OnRequestEnd(1, 2, testing::_))
      .Times(1)
      .WillOnce(QUIT_LOOP_SOON(&loop, 300));

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(action_max_timeout_ms());
}

TEST_F(ExternalTabUITest, HostNetworkStackUnresponsiveRenderer) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_, _)).Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";
  const IPC::AutomationURLResponse http_200 = {"", "HTTP/0.9 200\r\n\r\n", };

  EXPECT_CALL(*mock_, OnRequestStart(1, 3, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_, _)).Times(testing::AnyNumber());

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(1, 2, testing::AllOf(
          testing::Field(&IPC::AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&IPC::AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted, &http_200))));

  const std::string head = "<html><title>Hello</title><body>";

  const std::string data = "<table border=\"1\"><tr><th>Month</th>"
                           "<th>Savings</th></tr><tr><td>January</td>"
                           "<td>$100</td></tr><tr><td>February</td>"
                           "<td>$100</td></tr><tr><td>March</td>"
                           "<td>$100</td></tr><tr><td>April</td>"
                           "<td>$100</td></tr><tr><td>May</td>"
                           "<td>$100</td></tr><tr><td>June</td>"
                           "<td>$100</td></tr><tr><td>July</td>"
                           "<td>$100</td></tr><tr><td>Aug</td>"
                           "<td>$100</td></tr><tr><td>Sept</td>"
                           "<td>$100</td></tr><tr><td>Oct</td>"
                           "<td>$100</td></tr><tr><td>Nov</td>"
                           "<td>$100</td></tr><tr><td>Dec</td>"
                           "<td>$100</td></tr></table>";

  const std::string tail = "</body></html>";

  EXPECT_CALL(*mock_, OnRequestRead(1, 2, testing::Gt(0)))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &head, 1, 2)));

  EXPECT_CALL(*mock_, OnRequestRead(1, 2, testing::Gt(0)))
      .Times(100)
      .WillRepeatedly(testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)));

  EXPECT_CALL(*mock_, OnRequestRead(1, 2, testing::Gt(0)))
      .Times(testing::AnyNumber())
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyData, &tail, 1, 2)),
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyEOF, 1, 2)),
          QUIT_LOOP_SOON(&loop, 300)));
  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(action_max_timeout_ms());
  mock_->DestroyHostWindow();
}

class ExternalTabUITestPopupEnabled : public ExternalTabUITest {
 public:
  ExternalTabUITestPopupEnabled() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

// Testing AutomationMsg_AttachExternalTab callback from Chrome.
// Open a popup window with window.open() call. The created popup window opens
// another popup window (again using window.open() call).
TEST_F(ExternalTabUITestPopupEnabled, WindowDotOpen) {
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  mock_->IgnoreFavIconNetworkRequest();
  // Ignore navigation state changes.
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_, _)).Times(testing::AnyNumber());

  GURL main_url("http://placetogo.com/");
  std::string main_html =
    "<html><head><script type='text/javascript' language='JavaScript'>"
    "window.open('popup1.html','','toolbar=no,menubar=no,location=yes,"
    "height=320,width=300,left=1');"
    "</script></head><body>Main.</body></html>";
  mock_->ServeHTMLData(1, main_url, main_html);
  EXPECT_CALL(*mock_, OnLoad(1, _)).Times(1);

  GURL popup1_url("http://placetogo.com/popup1.html");
  std::string popup1_html =
    "<html><head><script type='text/javascript' language='JavaScript'>"
    "window.open('popup2.html','','');"
    "</script></head><body>Popup1.</body></html>";
  mock_->ServeHTMLData(2, popup1_url, popup1_html);
  EXPECT_CALL(*mock_, OnLoad(2, _)).Times(1);

  GURL popup2_url("http://placetogo.com/popup2.html");
  std::string popup2_html = "<html><body>Popup2.</body></html>";
  mock_->ServeHTMLData(3, popup2_url, popup2_html);
  EXPECT_CALL(*mock_, OnLoad(3, _)).Times(1)
      .WillOnce(QUIT_LOOP_SOON(&loop, 500));

  DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  HWND popup1_host = CreateWindowW(L"Button", L"popup1_host", style,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);

  HWND popup2_host = CreateWindowW(L"Button", L"popup2_host", style,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,  CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);

  EXPECT_CALL(*mock_, OnAttachExternalTab(1, _))
      .Times(1)
      .WillOnce(testing::WithArgs<1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ConnectToExternalTab, popup1_host))));

  EXPECT_CALL(*mock_, OnAttachExternalTab(2, _))
    .Times(1)
    .WillOnce(testing::WithArgs<1>(testing::Invoke(CreateFunctor(mock_,
        &ExternalTabUITestMockClient::ConnectToExternalTab, popup2_host))));

  mock_->CreateTabWithUrl(main_url);

  loop.RunFor(action_max_timeout_ms());

  EXPECT_CALL(*mock_, HandleClosed(1));
  EXPECT_CALL(*mock_, HandleClosed(2));
  EXPECT_CALL(*mock_, HandleClosed(3));

  mock_->DestroyHostWindow();
  ::DestroyWindow(popup1_host);
  ::DestroyWindow(popup2_host);
}

// Open a new window by simulating a user gesture through keyboard.
TEST_F(ExternalTabUITestPopupEnabled, UserGestureTargetBlank) {
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  mock_->IgnoreFavIconNetworkRequest();
  // Ignore navigation state changes.
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_, _)).Times(testing::AnyNumber());

  GURL main_url("http://placetogo.com/");
  std::string main_html = "<!DOCTYPE html><title>Hello</title>"
      "<a href='http://foo.com/' target='_blank'>Link</a>";
  mock_->ServeHTMLData(1, main_url, main_html);

  GURL foo_url("http://foo.com/");
  std::string foo_html = "<!DOCTYPE html>Foo lives here";
  mock_->ServeHTMLData(2, foo_url, foo_html);

  HWND foo_host = CreateWindowW(L"Button", L"foo_host",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      CW_USEDEFAULT,  CW_USEDEFAULT, NULL, NULL, NULL, NULL);

  testing::InSequence s;
  EXPECT_CALL(*mock_, OnLoad(1, _))
      .WillOnce(testing::InvokeWithoutArgs(testing::CreateFunctor(mock_,
          &ExternalTabUITestMockClient::NavigateThroughUserGesture)));

  EXPECT_CALL(*mock_, OnAttachExternalTab(1, _))
      .Times(1)
      .WillOnce(testing::WithArgs<1>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ConnectToExternalTab, foo_host))));

  EXPECT_CALL(*mock_, OnLoad(2, _)).WillOnce(QUIT_LOOP_SOON(&loop, 500));

  mock_->CreateTabWithUrl(main_url);
  loop.RunFor(action_max_timeout_ms());

  EXPECT_CALL(*mock_, HandleClosed(2));
  EXPECT_CALL(*mock_, HandleClosed(1));
  ::DestroyWindow(foo_host);
  mock_->DestroyHostWindow();
}

#endif  // defined(OS_WIN)

// TODO(port): Need to port autocomplete_edit_proxy.* first.
#if defined(OS_WIN) || defined(OS_LINUX)
TEST_F(AutomationProxyTest, AutocompleteGetSetText) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(edit->is_valid());
  const std::wstring text_to_set = L"Lollerskates";
  std::wstring actual_text;
  EXPECT_TRUE(edit->SetText(text_to_set));
  EXPECT_TRUE(edit->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser->GetAutocompleteEdit());
  EXPECT_TRUE(edit2->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
}

TEST_F(AutomationProxyTest, AutocompleteParallelProxy) {
  scoped_refptr<BrowserProxy> browser1(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser1.get());
  scoped_refptr<AutocompleteEditProxy> edit1(
      browser1->GetAutocompleteEdit());
  ASSERT_TRUE(edit1.get());
  EXPECT_TRUE(browser1->RunCommand(IDC_NEW_WINDOW));
  scoped_refptr<BrowserProxy> browser2(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser2.get());
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser2->GetAutocompleteEdit());
  ASSERT_TRUE(edit2.get());
  EXPECT_TRUE(browser2->GetTab(0)->WaitForTabToBeRestored(
      action_max_timeout_ms()));
  const std::wstring text_to_set1 = L"Lollerskates";
  const std::wstring text_to_set2 = L"Roflcopter";
  std::wstring actual_text1, actual_text2;
  EXPECT_TRUE(edit1->SetText(text_to_set1));
  EXPECT_TRUE(edit2->SetText(text_to_set2));
  EXPECT_TRUE(edit1->GetText(&actual_text1));
  EXPECT_TRUE(edit2->GetText(&actual_text2));
  EXPECT_EQ(text_to_set1, actual_text1);
  EXPECT_EQ(text_to_set2, actual_text2);
}

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_MACOSX)
// TODO(port): Implement AutocompleteEditProxy on Mac.
#define AutocompleteMatchesTest DISABLED_AutocompleteMatchesTest
#else
// So flaky, http://crbug.com/19876.
#define AutocompleteMatchesTest FLAKY_AutocompleteMatchesTest
#endif
TEST_F(AutomationProxyVisibleTest, AutocompleteMatchesTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(browser->ApplyAccelerator(IDC_FOCUS_LOCATION));
  EXPECT_TRUE(edit->is_valid());
  EXPECT_TRUE(edit->SetText(L"Roflcopter"));
  EXPECT_TRUE(edit->WaitForQuery(action_max_timeout_ms()));
  bool query_in_progress;
  EXPECT_TRUE(edit->IsQueryInProgress(&query_in_progress));
  EXPECT_FALSE(query_in_progress);
  std::vector<AutocompleteMatchData> matches;
  EXPECT_TRUE(edit->GetAutocompleteMatches(&matches));
  EXPECT_FALSE(matches.empty());
}

TEST_F(AutomationProxyTest, AppModalDialogTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool modal_dialog_showing = false;
  MessageBoxFlags::DialogButton button = MessageBoxFlags::DIALOGBUTTON_NONE;
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_NONE, button);

  // Show a simple alert.
  std::string content =
      "data:text/html,<html><head><script>function onload() {"
      "setTimeout(\"alert('hello');\", 1000); }</script></head>"
      "<body onload='onload()'></body></html>";
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK, button);

  // Test that clicking missing button fails graciously and does not close the
  // dialog.
  EXPECT_FALSE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);

  // Now click OK, that should close the dialog.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);

  // Show a confirm dialog.
  content =
      "data:text/html,<html><head><script>var result = -1; function onload() {"
      "setTimeout(\"result = confirm('hello') ? 0 : 1;\", 1000);} </script>"
      "</head><body onload='onload()'></body></html>";
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK |
            MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click OK.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  int result = -1;
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(0, result);

  // Try again.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK |
            MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click Cancel this time.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(1, result);
}

class AutomationProxyTest5 : public UITest {
 protected:
  AutomationProxyTest5() {
    show_window_ = true;
    dom_automation_enabled_ = true;
    // We need to disable popup blocking to ensure that the RenderView
    // instance for the popup actually closes.
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(AutomationProxyTest5, TestLifetimeOfDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("dom_automation_test_with_popup.html");

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(filename)));

  // Allow some time for the popup to show up and close.
  PlatformThread::Sleep(sleep_timeout_ms());

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}
