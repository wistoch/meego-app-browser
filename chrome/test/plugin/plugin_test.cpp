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

// Tests for the top plugins to catch regressions in our plugin host code, as
// well as in the out of process code.  Currently this tests:
//  Flash
//  Real
//  QuickTime
//  Windows Media Player
//    -this includes both WMP plugins.  npdsplay.dll is the older one that
//     comes with XP.  np-mswmp.dll can be downloaded from Microsoft and
//     needs SP2 or Vista.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <atlbase.h>
#include <comutil.h>

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "base/file_util.h"
#include "base/registry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kShortWaitTimeout = 10 * 1000;
const int kLongWaitTimeout  = 30 * 1000;

class PluginTest : public UITest {
 protected:
  virtual void SetUp() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "MediaPlayerNew") == 0) {
      // The installer adds our process names to the registry key below.  Since
      // the installer might not have run on this machine, add it manually.
      RegKey regkey;
      if (regkey.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList",
                      KEY_WRITE)) {
        regkey.CreateKey(L"CHROME.EXE", KEY_READ);
      }
      launch_arguments_.AppendSwitch(kNoNativeActiveXShimSwitch);

    } else if (strcmp(test_info->name(), "MediaPlayerOld") == 0) {
      // When testing the old WMP plugin, we need to force Chrome to not load
      // the new plugin.
      launch_arguments_.AppendSwitch(kUseOldWMPPluginSwitch);
      launch_arguments_.AppendSwitch(kNoNativeActiveXShimSwitch);
    } else if (strcmp(test_info->name(), "FlashSecurity") == 0) {
      launch_arguments_.AppendSwitchWithValue(switches::kTestSandbox,
                                              L"security_tests.dll");
    }

    UITest::SetUp();
  }

  void TestPlugin(const std::wstring& test_case, int timeout) {
    GURL url = GetTestUrl(test_case);
    NavigateToURL(url);
    WaitForFinish(timeout);
  }

  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in test_directory\plugin\<testcase>
  GURL GetTestUrl(const std::wstring &test_case) {
    std::wstring path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    file_util::AppendToPath(&path, L"plugin");
    file_util::AppendToPath(&path, test_case);
    return net::FilePathToFileURL(path);
  }

  // Waits for the test case to finish.
  void WaitForFinish(const int wait_time) {
    const int kSleepTime = 500;      // 2 times per second
    const int kMaxIntervals = wait_time / kSleepTime;

    GURL url = GetTestUrl(L"done");
    scoped_ptr<TabProxy> tab(GetActiveTab());

    std::string done_str;
    for (int i = 0; i < kMaxIntervals; ++i) {
      Sleep(kSleepTime);

      // The webpage being tested has javascript which sets a cookie
      // which signals completion of the test.
      std::string cookieName = kTestCompleteCookie;
      tab->GetCookieByName(url, cookieName, &done_str);
      if (!done_str.empty())
        break;
    }

    EXPECT_EQ(kTestCompleteSuccess, done_str);
  }
};

// Disabling all tests to make buildbot green. See -
// http://code.google.com/p/chromium/issues/detail?id=7983
TEST_F(PluginTest, DISABLED_Quicktime) {
  TestPlugin(L"quicktime.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_MediaPlayerNew) {
  TestPlugin(L"wmp_new.html", kShortWaitTimeout);
}

// http://crbug.com/4809
TEST_F(PluginTest, DISABLED_MediaPlayerOld) {
  TestPlugin(L"wmp_old.html", kLongWaitTimeout);
}

TEST_F(PluginTest, DISABLED_Real) {
  TestPlugin(L"real.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_Flash) {
  TestPlugin(L"flash.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_FlashOctetStream) {
  TestPlugin(L"flash-octet-stream.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_FlashSecurity) {
  TestPlugin(L"flash.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin(L"Java.html", kShortWaitTimeout);
}

TEST_F(PluginTest, DISABLED_Silverlight) {
  TestPlugin(L"silverlight.html", kShortWaitTimeout);
}

typedef HRESULT (__stdcall* DllRegUnregServerFunc)();

class ActiveXTest : public PluginTest {
 public:
  ActiveXTest() {
    dll_registered = false;
  }
 protected:
  void TestActiveX(const std::wstring& test_case, int timeout, bool reg_dll) {
    if (reg_dll) {
      RegisterTestControl(true);
      dll_registered = true;
    }
    TestPlugin(test_case, timeout);
  }
  virtual void TearDown() {
    PluginTest::TearDown();
    if (dll_registered)
      RegisterTestControl(false);
  }
  void RegisterTestControl(bool register_server) {
    std::wstring test_control_path = browser_directory_ +
        L"\\activex_test_control.dll";
    HMODULE h = LoadLibrary(test_control_path.c_str());
    ASSERT_TRUE(h != NULL) << "Failed to load activex_test_control.dll";
    const char* func_name = register_server ? 
                                "DllRegisterServer" : "DllUnregisterServer";
    DllRegUnregServerFunc func = reinterpret_cast<DllRegUnregServerFunc>(
        GetProcAddress(h, func_name));
    // This should never happen actually.
    ASSERT_TRUE(func != NULL) << "Failed to find reg/unreg function.";
    HRESULT hr = func();
    const char* error_message = register_server ? "Failed to register dll."
                                                : "Failed to unregister dll";
    ASSERT_TRUE(SUCCEEDED(hr)) << error_message;
    FreeLibrary(h);
  }
 private:
  bool dll_registered;
};

TEST_F(ActiveXTest, DISABLED_EmbeddedWMP) {
  TestActiveX(L"activex_embedded_wmp.html", kLongWaitTimeout, false);
}

TEST_F(ActiveXTest, DISABLED_WMP) {
  TestActiveX(L"activex_wmp.html", kLongWaitTimeout, false);
}

TEST_F(ActiveXTest, DISABLED_CustomScripting) {
  TestActiveX(L"activex_custom_scripting.html", kShortWaitTimeout, true);
}

