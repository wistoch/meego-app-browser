// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
// windows headers
#include <comutil.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#endif

// runtime headers
#include <memory.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>

#include "base/file_path.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "chrome/test/ui_test_utils.h"

using npapi_test::kTestCompleteCookie;
using npapi_test::kTestCompleteSuccess;

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("npapi");

// Test passing arguments to a plugin.
#if defined(OS_MACOSX)
// http://crbug.com/42340 - fails on 10.6 most of the time
#define MAYBE_Arguments FLAKY_Arguments
#else
#define MAYBE_Arguments Arguments
#endif
TEST_F(NPAPITester, MAYBE_Arguments) {
  const FilePath test_case(FILE_PATH_LITERAL("arguments.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("arguments", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Test invoking many plugins within a single page.
// Flaky, http://crbug.com/28372
TEST_F(NPAPITester, FLAKY_ManyPlugins) {
  const FilePath test_case(FILE_PATH_LITERAL("many_plugins.html"));
  GURL url(ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  for (int i = 1; i <= 15; i++) {
    SCOPED_TRACE(StringPrintf("Waiting for plugin #%d", i));
    ASSERT_NO_FATAL_FAILURE(WaitForFinish("arguments", IntToString(i),
                                          url, kTestCompleteCookie,
                                          kTestCompleteSuccess,
                                          action_max_timeout_ms()));
  }
}

// Test various calls to GetURL from a plugin.
TEST_F(NPAPITester, GetURL) {
  const FilePath test_case(FILE_PATH_LITERAL("geturl.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("geturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
TEST_F(NPAPITester, GetJavaScriptURL) {
  const FilePath test_case(FILE_PATH_LITERAL("get_javascript_url.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Flaky test: http://crbug.com/29020
// Test that calling GetURL with a javascript URL and target=_self
// works properly when the plugin is embedded in a subframe.
TEST_F(NPAPITester, FLAKY_GetJavaScriptURL2) {
  const FilePath test_case(FILE_PATH_LITERAL("get_javascript_url2.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl2", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
#if defined(OS_MACOSX)
// http://crbug.com/42086 - fails on 10.6 most of the time
#define MAYBE_NPObjectProxy FLAKY_NPObjectProxy
#else
#define MAYBE_NPObjectProxy NPObjectProxy
#endif
TEST_F(NPAPITester, MAYBE_NPObjectProxy) {
  const FilePath test_case(FILE_PATH_LITERAL("npobject_proxy.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_proxy", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginGetUrl) {
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_geturl.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_geturl", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
// Flaky. See http://crbug.com/30702
TEST_F(NPAPITester, FLAKY_SelfDeletePluginInvoke) {
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_invoke.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_invoke", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
TEST_F(NPAPITester, DISABLED_SelfDeletePluginInvokeAlert) {
  const FilePath test_case(
      FILE_PATH_LITERAL("self_delete_plugin_invoke_alert.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // Wait for the alert dialog and then close it.
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_ESCAPE, 0));

  WaitForFinish("self_delete_plugin_invoke_alert", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
TEST_F(NPAPIVisiblePluginTester,
       DISABLED_SelfDeletePluginInvokeInSynchronousPaint) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  const FilePath test_case(
      FILE_PATH_LITERAL("execute_script_delete_in_paint.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("execute_script_delete_in_paint", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}
#endif

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNewStream) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_stream.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_stream", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

#if defined(OS_WIN)
// Tests if a plugin has a non zero window rect.
TEST_F(NPAPIVisiblePluginTester, VerifyPluginWindowRect) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("verify_plugin_window_rect.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("checkwindowrect", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
TEST_F(NPAPIVisiblePluginTester, CreateInstanceInPaint) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("create_instance_in_paint.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("create_instance_in_paint", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Tests that putting up an alert in response to a paint doesn't deadlock.
TEST_F(NPAPIVisiblePluginTester, AlertInWindowMessage) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("alert_in_window_message.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  bool modal_dialog_showing = false;
  MessageBoxFlags::DialogButton available_buttons;
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));

  modal_dialog_showing = false;
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
}

TEST_F(NPAPIVisiblePluginTester, VerifyNPObjectLifetimeTest) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("npobject_lifetime_test.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_lifetime_test", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

// Tests that we don't crash or assert if NPP_New fails
TEST_F(NPAPIVisiblePluginTester, NewFails) {
  const FilePath test_case(FILE_PATH_LITERAL("new_fails.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("new_fails", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNPNEvaluate) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(
      FILE_PATH_LITERAL("execute_script_delete_in_npn_evaluate.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_delete_plugin_in_evaluate", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}
#endif

// Flaky. See http://crbug.com/17645
TEST_F(NPAPIVisiblePluginTester, DISABLED_OpenPopupWindowWithPlugin) {
  const FilePath test_case(
      FILE_PATH_LITERAL("get_javascript_open_popup_with_plugin.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_popup_with_plugin_target", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_timeout_ms());
}

// Test checking the privacy mode is off.
TEST_F(NPAPITester, PrivateDisabled) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(FILE_PATH_LITERAL("private.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

TEST_F(NPAPITester, ScheduleTimer) {
  const FilePath test_case(FILE_PATH_LITERAL("schedule_timer.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("schedule_timer", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

TEST_F(NPAPITester, PluginThreadAsyncCall) {
  const FilePath test_case(FILE_PATH_LITERAL("plugin_thread_async_call.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_thread_async_call", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

// Test checking the privacy mode is on.
TEST_F(NPAPIIncognitoTester, PrivateEnabled) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(FILE_PATH_LITERAL("private.html?private"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
TEST_F(NPAPIVisiblePluginTester, MultipleInstancesSyncCalls) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(
      FILE_PATH_LITERAL("multiple_instances_sync_calls.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("multiple_instances_sync_calls", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}
#endif

TEST_F(NPAPIVisiblePluginTester, GetURLRequestFailWrite) {
  if (UITest::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_fail_write.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("geturl_fail_write", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

#if defined(OS_WIN)
TEST_F(NPAPITester, EnsureScriptingWorksInDestroy) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(
      FILE_PATH_LITERAL("ensure_scripting_works_in_destroy.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("ensure_scripting_works_in_destroy", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

// This test uses a Windows Event to signal to the plugin that it should crash
// on NP_Initialize.
// This is flaky. http://crbug.com/32048
TEST_F(NPAPITester, FLAKY_NoHangIfInitCrashes) {
  if (UITest::in_process_renderer())
    return;

  // Only Windows implements the crash service for now.
#if defined(OS_WIN)
  expected_crashes_ = 1;
#endif

  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  SetEvent(crash_event);
  const FilePath test_case(FILE_PATH_LITERAL("no_hang_if_init_crashes.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  NavigateToURL(url);
  WaitForFinish("no_hang_if_init_crashes", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
  CloseHandle(crash_event);
}

#endif

TEST_F(NPAPITester, NPObjectReleasedOnDestruction) {
  if (UITest::in_process_renderer())
    return;

  const FilePath test_case(
      FILE_PATH_LITERAL("npobject_released_on_destruction.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window_proxy);
  ASSERT_TRUE(window_proxy->AppendTab(GURL(chrome::kAboutBlankURL)));

  scoped_refptr<TabProxy> tab_proxy(window_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->Close(true));
}

// Test that a dialog is properly created when a plugin throws an
// exception.  Should be run for in and out of process plugins, but
// the more interesting case is out of process, where we must route
// the exception to the correct renderer.
TEST_F(NPAPITester, NPObjectSetException) {
  const FilePath test_case(FILE_PATH_LITERAL("npobject_set_exception.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_set_exception", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}

TEST_F(NPAPIVisiblePluginTester, PluginReferrerTest) {
  if (UITest::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_referrer_test.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("plugin_referrer_test", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

#if defined(OS_MACOSX)
// http://crbug.com/36670 - failes on 10.6
TEST_F(NPAPIVisiblePluginTester, FLAKY_PluginConvertPointTest) {
  if (UITest::in_process_renderer())
    return;

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  window->SetBounds(gfx::Rect(100, 100, 600, 600));

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("npapi/convert_point.html"))));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // TODO(stuartmorgan): When the automation system supports sending clicks,
  // change the test to trigger on mouse-down rather than window focus.
  ASSERT_TRUE(browser->BringToFront());
  WaitForFinish("convert_point", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}
#endif
