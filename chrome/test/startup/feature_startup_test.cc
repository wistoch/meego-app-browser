// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;

namespace {

// Returns the directory name where the "typical" user data is that we use for
// testing.
FilePath ComputeTypicalUserDataSource() {
  FilePath source_history_file;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                               &source_history_file));
  source_history_file = source_history_file.AppendASCII("profiles")
      .AppendASCII("typical_history");
  return source_history_file;
}

class NewTabUIStartupTest : public UITest {
 public:
  NewTabUIStartupTest() {
    show_window_ = true;
  }

  void SetUp() {}
  void TearDown() {}

  static const int kNumCycles = 5;

  void PrintTimings(const char* label, TimeDelta timings[kNumCycles],
                    bool important) {
    std::string times;
    for (int i = 0; i < kNumCycles; ++i)
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList("new_tab", "", label, times, "ms", important);
  }

  // Run the test, by bringing up a browser and timing the new tab startup.
  // |want_warm| is true if we should output warm-disk timings, false if
  // we should report cold timings.
  void RunStartupTest(const char* label, bool want_warm, bool important) {
    // Install the location of the test profile file.
    set_template_user_data(ComputeTypicalUserDataSource().ToWStringHack());

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      UITest::SetUp();

      // Switch to the "new tab" tab, which should be any new tab after the
      // first (the first is about:blank).
      BrowserProxy* window = automation()->GetBrowserWindow(0);
      ASSERT_TRUE(window);
      int tab_count = -1;
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(1, tab_count);

      // Hit ctl-t and wait for the tab to load.
      window->ApplyAccelerator(IDC_NEW_TAB);
      ASSERT_TRUE(window->WaitForTabCountToBecome(2, 5000));
      int load_time;
      ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));
      timings[i] = TimeDelta::FromMilliseconds(load_time);

      if (want_warm) {
        // Bring up a second tab, now that we've already shown one tab.
        window->ApplyAccelerator(IDC_NEW_TAB);
        ASSERT_TRUE(window->WaitForTabCountToBecome(3, 5000));
        ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));
        timings[i] = TimeDelta::FromMilliseconds(load_time);
      }

      delete window;
      UITest::TearDown();
    }

    PrintTimings(label, timings, important);
  }
};

}  // namespace

// TODO(pamg): run these tests with a reference build?

// TODO(tc): Fix this.
#if !defined(OS_MACOSX)
TEST_F(NewTabUIStartupTest, PerfCold) {
  RunStartupTest("tab_cold", false /* not cold */, true /* important */);
}

TEST_F(NewTabUIStartupTest, DISABLED_PerfWarm) {
  RunStartupTest("tab_warm", true /* cold */, false /* not important */);
}
#endif
