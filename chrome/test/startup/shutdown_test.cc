// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env_var.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"

using base::TimeDelta;

namespace {

class ShutdownTest : public UITest {
 public:
  ShutdownTest() {
    show_window_ = true;
  }
  void SetUp() {}
  void TearDown() {}

  void SetUpTwentyTabs() {
    const FilePath kFastShutdownDir(FILE_PATH_LITERAL("fast_shutdown"));
    const FilePath kCurrentDir(FilePath::kCurrentDirectory);
    const FilePath test_cases[] = {
      ui_test_utils::GetTestFilePath(kFastShutdownDir,
          FilePath(FILE_PATH_LITERAL("on_before_unloader.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("animated-gifs.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("french_page.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("onunload_cookie.html"))),
    };

    for (size_t i = 0; i < arraysize(test_cases); i++) {
      ASSERT_TRUE(file_util::PathExists(test_cases[i]));
      for (size_t j = 0; j < 5; j++) {
        launch_arguments_.AppendLooseValue(test_cases[i].ToWStringHack());
      }
    }
  }

  void RunShutdownTest(const char* graph, const char* trace, bool important,
                       UITest::ShutdownType shutdown_type) {
    const int kNumCyclesMax = 20;
    int numCycles = kNumCyclesMax;
    scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
    std::string numCyclesEnv;
    if (env->GetEnv(env_vars::kStartupTestsNumCycles, &numCyclesEnv) &&
        StringToInt(numCyclesEnv, &numCycles)) {
      LOG(INFO) << env_vars::kStartupTestsNumCycles << " set in environment, "
                << "so setting numCycles to " << numCycles;
    }

    TimeDelta timings[kNumCyclesMax];
    for (int i = 0; i < numCycles; ++i) {
      UITest::SetUp();
      set_shutdown_type(shutdown_type);
      UITest::TearDown();
      timings[i] = browser_quit_time_;

      if (i == 0) {
        // Re-use the profile data after first run so that the noise from
        // creating databases doesn't impact all the runs.
        clear_profile_ = false;
        // Clear template_user_data_ so we don't try to copy it over each time
        // through.
        set_template_user_data(FilePath());
      }
    }

    std::string times;
    for (int i = 0; i < numCycles; ++i)
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList(graph, "", trace, times, "ms", important);
  }
};

TEST_F(ShutdownTest, SimpleWindowClose) {
  RunShutdownTest("shutdown", "simple-window-close",
                  true, /* important */ UITest::WINDOW_CLOSE);
}

TEST_F(ShutdownTest, SimpleUserQuit) {
  RunShutdownTest("shutdown", "simple-user-quit",
                  true, /* important */ UITest::USER_QUIT);
}

TEST_F(ShutdownTest, SimpleSessionEnding) {
  RunShutdownTest("shutdown", "simple-session-ending",
                  true, /* important */ UITest::SESSION_ENDING);
}

TEST_F(ShutdownTest, TwentyTabsWindowClose) {
  SetUpTwentyTabs();
  RunShutdownTest("shutdown", "twentytabs-window-close",
                  true, /* important */ UITest::WINDOW_CLOSE);
}

TEST_F(ShutdownTest, TwentyTabsUserQuit) {
  SetUpTwentyTabs();
  RunShutdownTest("shutdown", "twentytabs-user-quit",
                  true, /* important */ UITest::USER_QUIT);
}

// http://crbug.com/40671
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TwentyTabsSessionEnding DISABLED_TwentyTabsSessionEnding
#else
#define MAYBE_TwentyTabsSessionEnding TwentyTabsSessionEnding
#endif
TEST_F(ShutdownTest, MAYBE_TwentyTabsSessionEnding) {
  SetUpTwentyTabs();
  RunShutdownTest("shutdown", "twentytabs-session-ending",
                  true, /* important */ UITest::SESSION_ENDING);
}

}  // namespace
