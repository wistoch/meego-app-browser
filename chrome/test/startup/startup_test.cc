// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env_var.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
  }
  void SetUp() {}
  void TearDown() {}

  enum TestColdness {
    WARM,
    COLD
  };

  enum TestImportance {
    NOT_IMPORTANT,
    IMPORTANT
  };

  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading and the loading of gears.dll.
  void SetUpWithFileURL() {
    const FilePath file_url = ui_test_utils::GetTestFilePath(
        FilePath(FilePath::kCurrentDirectory),
        FilePath(FILE_PATH_LITERAL("simple.html")));
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_.AppendLooseValue(file_url.ToWStringHack());
  }

  // Load a complex html file on startup represented by |which_tab|.
  void SetUpWithComplexFileURL(unsigned int which_tab) {
    const char* const kTestPageCyclerDomains[] = {
        "www.google.com", "www.nytimes.com",
        "www.yahoo.com", "espn.go.com", "www.amazon.com"
    };
    unsigned int which_el = which_tab % arraysize(kTestPageCyclerDomains);
    const char* this_domain = kTestPageCyclerDomains[which_el];

    FilePath page_cycler_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &page_cycler_path);
    page_cycler_path = page_cycler_path.AppendASCII("data")
        .AppendASCII("page_cycler").AppendASCII("moz")
        .AppendASCII(this_domain).AppendASCII("index.html");
    GURL file_url = net::FilePathToFileURL(page_cycler_path).Resolve("?skip");
    launch_arguments_.AppendLooseValue(ASCIIToWide(file_url.spec()));
  }

  // Use the given profile in the test data extensions/profiles dir.  This tests
  // startup with extensions installed.
  void SetUpWithExtensionsProfile(const char* profile) {
    FilePath data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
    data_dir = data_dir.AppendASCII("extensions").AppendASCII("profiles").
        AppendASCII(profile);
    set_template_user_data(data_dir);

    // For now, these tests still depend on toolstrips.
    launch_arguments_.AppendSwitch(switches::kEnableExtensionToolstrips);
  }

  // Runs a test which loads |tab_count| tabs on startup, either as command line
  // arguments or, if |restore_session| is true, by using session restore.
  // |nth_timed_tab|, if non-zero, will measure time to load the first n+1 tabs.
  void RunPerfTestWithManyTabs(const char* graph, const char* trace,
                               int tab_count, int nth_timed_tab,
                               bool restore_session);

  void RunStartupTest(const char* graph, const char* trace,
                      TestColdness test_cold, TestImportance test_importance,
                      UITest::ProfileType profile_type,
                      int num_tabs, int nth_timed_tab) {
    bool important = (test_importance == IMPORTANT);
    profile_type_ = profile_type;

    // Sets the profile data for the run.  For now, this is only used for
    // the non-default themes test.
    if (profile_type != UITest::DEFAULT_THEME) {
      set_template_user_data(UITest::ComputeTypicalUserDataSource(
          profile_type));
    }

    const int kNumCyclesMax = 20;
    int numCycles = kNumCyclesMax;
    scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
    std::string numCyclesEnv;
    if (env->GetEnv(env_vars::kStartupTestsNumCycles, &numCyclesEnv) &&
        StringToInt(numCyclesEnv, &numCycles)) {
      if (numCycles <= kNumCyclesMax) {
        LOG(INFO) << env_vars::kStartupTestsNumCycles
                  << " set in environment, so setting numCycles to "
                  << numCycles;
      } else {
        LOG(INFO) << env_vars::kStartupTestsNumCycles
                  << " is higher than the max, setting numCycles to "
                  << kNumCyclesMax;
        numCycles = kNumCyclesMax;
      }
    }

    struct TimingInfo {
      TimeDelta end_to_end;
      float first_start_ms;
      float last_stop_ms;
      float first_stop_ms;
      float nth_tab_stop_ms;
    };
    TimingInfo timings[kNumCyclesMax];

    for (int i = 0; i < numCycles; ++i) {
      if (test_cold == COLD) {
        FilePath dir_app;
        ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app));

        FilePath chrome_exe(dir_app.Append(
            FilePath::FromWStringHack(chrome::kBrowserProcessExecutablePath)));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_exe));
#if defined(OS_WIN)
        // chrome.dll is windows specific.
        FilePath chrome_dll(dir_app.Append(FILE_PATH_LITERAL("chrome.dll")));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_dll));
#endif

#if defined(OS_WIN)
        // TODO(port): Re-enable once gears is working on mac/linux.
        FilePath gears_dll;
        ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(gears_dll));
#else
        NOTIMPLEMENTED() << "gears not enabled yet";
#endif
      }
      UITest::SetUp();
      TimeTicks end_time = TimeTicks::Now();

      // HACK: Chrome < 5.0.368.0 did not yet implement SendJSONRequest.
      {
        std::string server_version = automation()->server_version();
        std::vector<std::string> version_numbers;
        SplitString(server_version, '.', &version_numbers);
        int chrome_buildnum = 0;
        ASSERT_TRUE(StringToInt(version_numbers[2], &chrome_buildnum));
        if (chrome_buildnum < 368) {
          num_tabs = 0;
        }
      }
      if (num_tabs > 0) {
        float min_start;
        float max_stop;
        std::vector<float> times;
        scoped_refptr<BrowserProxy> browser_proxy(
            automation()->GetBrowserWindow(0));
        ASSERT_TRUE(browser_proxy.get());

        if (browser_proxy->GetInitialLoadTimes(&min_start, &max_stop, &times) &&
            !times.empty()) {
          ASSERT_LT(nth_timed_tab, num_tabs);
          ASSERT_EQ(times.size(), static_cast<size_t>(num_tabs));
          timings[i].first_start_ms = min_start;
          timings[i].last_stop_ms = max_stop;
          timings[i].first_stop_ms = times[0];
          timings[i].nth_tab_stop_ms = times[nth_timed_tab];
        } else {
          // Browser might not support initial load times.
          // Only use end-to-end time for this test.
          num_tabs = 0;
        }
      }
      timings[i].end_to_end = end_time - browser_launch_time_;
      UITest::TearDown();

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
      StringAppendF(&times, "%.2f,", timings[i].end_to_end.InMillisecondsF());
    PrintResultList(graph, "", trace, times, "ms", important);

    if (num_tabs > 0) {
      std::string name_base = trace;
      std::string name;

      times.clear();
      name = name_base + "-start";
      for (int i = 0; i < numCycles; ++i)
        StringAppendF(&times, "%.2f,", timings[i].first_start_ms);
      PrintResultList(graph, "", name.c_str(), times, "ms", important);

      times.clear();
      name = name_base + "-first";
      for (int i = 0; i < numCycles; ++i)
        StringAppendF(&times, "%.2f,", timings[i].first_stop_ms);
      PrintResultList(graph, "", name.c_str(), times, "ms", important);

      if (nth_timed_tab > 0) {
        // Display only the time necessary to load the first n tabs.
        times.clear();
        name = name_base + "-" + IntToString(nth_timed_tab);
        for (int i = 0; i < numCycles; ++i)
          StringAppendF(&times, "%.2f,", timings[i].nth_tab_stop_ms);
        PrintResultList(graph, "", name.c_str(), times, "ms", important);
      }

      if (num_tabs > 1) {
        // Display the time necessary to load all of the tabs.
        times.clear();
        name = name_base + "-all";
        for (int i = 0; i < numCycles; ++i)
          StringAppendF(&times, "%.2f,", timings[i].last_stop_ms);
        PrintResultList(graph, "", name.c_str(), times, "ms", important);
      }
    }
  }
};

TEST_F(StartupTest, PerfWarm) {
  RunStartupTest("warm", "t", WARM, IMPORTANT, UITest::DEFAULT_THEME, 0, 0);
}

TEST_F(StartupTest, PerfReferenceWarm) {
  UseReferenceBuild();
  RunStartupTest("warm", "t_ref", WARM, IMPORTANT, UITest::DEFAULT_THEME, 0, 0);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest("cold", "t", COLD, NOT_IMPORTANT, UITest::DEFAULT_THEME, 0, 0);
}

void StartupTest::RunPerfTestWithManyTabs(const char* graph, const char* trace,
                                          int tab_count, int nth_timed_tab,
                                          bool restore_session) {
  // Initialize session with |tab_count| tabs.
  for (int i = 0; i < tab_count; ++i)
    SetUpWithComplexFileURL(i);

  if (restore_session) {
    // Start the browser with these urls so we can save the session and exit.
    UITest::SetUp();
    // Set flags to ensure profile is saved and can be restored.
#if defined(OS_MACOSX)
    shutdown_type_ = UITestBase::USER_QUIT;
#endif
    clear_profile_ = false;
    // Quit and set flags to restore session.
    UITest::TearDown();
    // Clear all arguments for session restore, or the number of open tabs
    // will grow with each restore.
    launch_arguments_ = CommandLine(CommandLine::ARGUMENTS_ONLY);
    // The session will be restored once per cycle for numCycles test cycles,
    // and each time, UITest::SetUp will wait for |tab_count| tabs to
    // finish loading.
    launch_arguments_.AppendSwitchWithValue(switches::kRestoreLastSession,
                                            IntToWString(tab_count));
  }
  RunStartupTest(graph, trace, WARM, NOT_IMPORTANT, UITest::DEFAULT_THEME,
                 tab_count, nth_timed_tab);
}

TEST_F(StartupTest, PerfFewTabs) {
  RunPerfTestWithManyTabs("few_tabs", "cmdline", 5, 2, false);
}

TEST_F(StartupTest, PerfFewTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("few_tabs", "cmdline-ref", 5, 2, false);
}

TEST_F(StartupTest, PerfRestoreFewTabs) {
  RunPerfTestWithManyTabs("few_tabs", "restore", 5, 2, true);
}

TEST_F(StartupTest, PerfRestoreFewTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("few_tabs", "restore-ref", 5, 2, true);
}

// http://crbug.com/46609
#if defined(OS_MACOSX)
#define MAYBE_PerfSeveralTabsReference FLAKY_PerfSeveralTabsReference
#define MAYBE_PerfSeveralTabs FLAKY_PerfSeveralTabs
#else
#define MAYBE_PerfSeveralTabsReference PerfSeveralTabsReference
#define MAYBE_PerfSeveralTabs PerfSeveralTabs
#endif

TEST_F(StartupTest, MAYBE_PerfSeveralTabs) {
  RunPerfTestWithManyTabs("several_tabs", "cmdline", 10, 4, false);
}

TEST_F(StartupTest, MAYBE_PerfSeveralTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("several_tabs", "cmdline-ref", 10, 4, false);
}

TEST_F(StartupTest, PerfRestoreSeveralTabs) {
  RunPerfTestWithManyTabs("several_tabs", "restore", 10, 4, true);
}

TEST_F(StartupTest, PerfRestoreSeveralTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("several_tabs", "restore-ref", 10, 4, true);
}

TEST_F(StartupTest, PerfExtensionEmpty) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("empty");
  RunStartupTest("warm", "extension_empty", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfExtensionToolstrips1) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("toolstrips1");
  RunStartupTest("warm", "extension_toolstrip1", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfExtensionToolstrips50) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("toolstrips50");
  RunStartupTest("warm", "extension_toolstrip50", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfExtensionContentScript1) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts1");
  RunStartupTest("warm", "extension_content_scripts1", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfExtensionContentScript50) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts50");
  RunStartupTest("warm", "extension_content_scripts50", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

#if defined(OS_WIN)
// TODO(port): Enable gears tests on linux/mac once gears is working.
TEST_F(StartupTest, PerfGears) {
  SetUpWithFileURL();
  RunStartupTest("warm", "gears", WARM, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfColdGears) {
  SetUpWithFileURL();
  RunStartupTest("cold", "gears", COLD, NOT_IMPORTANT,
                 UITest::DEFAULT_THEME, 1, 0);
}
#endif

TEST_F(StartupTest, PerfComplexTheme) {
  RunStartupTest("warm", "t-theme", WARM, NOT_IMPORTANT,
                 UITest::COMPLEX_THEME, 0, 0);
}

#if defined(OS_LINUX)
TEST_F(StartupTest, PerfGtkTheme) {
  RunStartupTest("warm", "gtk-theme", WARM, NOT_IMPORTANT,
                 UITest::NATIVE_THEME, 0, 0);
}

TEST_F(StartupTest, PrefNativeFrame) {
  RunStartupTest("warm", "custom-frame", WARM, NOT_IMPORTANT,
                 UITest::CUSTOM_FRAME, 0, 0);
}

TEST_F(StartupTest, PerfNativeFrameGtkTheme) {
  RunStartupTest("warm", "custom-frame-gtk-theme", WARM, NOT_IMPORTANT,
                 UITest::CUSTOM_FRAME_NATIVE_THEME, 0, 0);
}
#endif

}  // namespace
