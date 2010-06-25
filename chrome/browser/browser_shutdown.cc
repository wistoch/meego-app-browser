// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_shutdown.h"

#include <string>

#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "net/dns_global.h"

#if defined(OS_WIN)
#include "chrome/browser/rlz/rlz.h"
#endif

using base::Time;
using base::TimeDelta;

namespace browser_shutdown {

#if defined(OS_MACOSX)
// Whether the browser is trying to quit (e.g., Quit chosen from menu).
bool g_trying_to_quit = false;
#endif  // OS_MACOSX

Time shutdown_started_;
ShutdownType shutdown_type_ = NOT_VALID;
int shutdown_num_processes_;
int shutdown_num_processes_slow_;

bool delete_resources_on_shutdown = true;

const char kShutdownMsFile[] = "chrome_shutdown_ms.txt";

void RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kShutdownType, NOT_VALID);
  local_state->RegisterIntegerPref(prefs::kShutdownNumProcesses, 0);
  local_state->RegisterIntegerPref(prefs::kShutdownNumProcessesSlow, 0);
}

ShutdownType GetShutdownType() {
  return shutdown_type_;
}

void OnShutdownStarting(ShutdownType type) {
  if (shutdown_type_ != NOT_VALID)
    return;

  shutdown_type_ = type;
  // For now, we're only counting the number of renderer processes
  // since we can't safely count the number of plugin processes from this
  // thread, and we'd really like to avoid anything which might add further
  // delays to shutdown time.
  shutdown_started_ = Time::Now();

  // Call FastShutdown on all of the RenderProcessHosts.  This will be
  // a no-op in some cases, so we still need to go through the normal
  // shutdown path for the ones that didn't exit here.
  shutdown_num_processes_ = 0;
  shutdown_num_processes_slow_ = 0;
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    ++shutdown_num_processes_;
    if (!i.GetCurrentValue()->FastShutdownIfPossible())
      ++shutdown_num_processes_slow_;
  }
}

FilePath GetShutdownMsPath() {
  FilePath shutdown_ms_file;
  PathService::Get(chrome::DIR_USER_DATA, &shutdown_ms_file);
  return shutdown_ms_file.AppendASCII(kShutdownMsFile);
}

void Shutdown() {
  // Unload plugins. This needs to happen on the IO thread.
  ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableFunction(&ChromePluginLib::UnloadAllPlugins));

  // WARNING: During logoff/shutdown (WM_ENDSESSION) we may not have enough
  // time to get here. If you have something that *must* happen on end session,
  // consider putting it in BrowserProcessImpl::EndSession.
  DCHECK(g_browser_process);

  // Notifies we are going away.
  g_browser_process->shutdown_event()->Signal();

  PrefService* prefs = g_browser_process->local_state();

  chrome_browser_net::SavePredictorStateForNextStartupAndTrim(prefs);

  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    metrics->RecordCleanShutdown();
    metrics->RecordCompletedSessionEnd();
  }

  if (shutdown_type_ > NOT_VALID && shutdown_num_processes_ > 0) {
    // Record the shutdown info so that we can put it into a histogram at next
    // startup.
    prefs->SetInteger(prefs::kShutdownType, shutdown_type_);
    prefs->SetInteger(prefs::kShutdownNumProcesses, shutdown_num_processes_);
    prefs->SetInteger(prefs::kShutdownNumProcessesSlow,
                      shutdown_num_processes_slow_);
  }

  // Check local state for the restart flag so we can restart the session below.
  bool restart_last_session = false;
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown)) {
    restart_last_session =
        prefs->GetBoolean(prefs::kRestartLastSessionOnShutdown);
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  }

  prefs->SavePersistentPrefs();

#if defined(OS_WIN)
  // Cleanup any statics created by RLZ. Must be done before NotificationService
  // is destroyed.
  RLZTracker::CleanupRlz();
#endif

  // The jank'o'meter requires that the browser process has been destroyed
  // before calling UninstallJankometer().
  delete g_browser_process;
  g_browser_process = NULL;

  // Uninstall Jank-O-Meter here after the IO thread is no longer running.
  UninstallJankometer();

  if (delete_resources_on_shutdown)
    ResourceBundle::CleanupSharedInstance();

#if defined(OS_WIN)
  if (!Upgrade::IsBrowserAlreadyRunning() &&
      shutdown_type_ != browser_shutdown::END_SESSION) {
    Upgrade::SwapNewChromeExeIfPresent();
  }
#endif

  if (restart_last_session) {
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
    // Make sure to relaunch the browser with the same command line and add
    // Restore Last Session flag if session restore is not set.
    CommandLine command_line(*CommandLine::ForCurrentProcess());
    if (!command_line.HasSwitch(switches::kRestoreLastSession))
      command_line.AppendSwitch(switches::kRestoreLastSession);
    Upgrade::RelaunchChromeBrowser(command_line);
#else
    NOTIMPLEMENTED();
#endif
  }

  if (shutdown_type_ > NOT_VALID && shutdown_num_processes_ > 0) {
    // Measure total shutdown time as late in the process as possible
    // and then write it to a file to be read at startup.
    // We can't use prefs since all services are shutdown at this point.
    TimeDelta shutdown_delta = Time::Now() - shutdown_started_;
    std::string shutdown_ms = Int64ToString(shutdown_delta.InMilliseconds());
    int len = static_cast<int>(shutdown_ms.length()) + 1;
    FilePath shutdown_ms_file = GetShutdownMsPath();
    file_util::WriteFile(shutdown_ms_file, shutdown_ms.c_str(), len);
  }

  UnregisterURLRequestChromeJob();
}

void ReadLastShutdownFile(
    ShutdownType type,
    int num_procs,
    int num_procs_slow) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  FilePath shutdown_ms_file = GetShutdownMsPath();
  std::string shutdown_ms_str;
  int64 shutdown_ms = 0;
  if (file_util::ReadFileToString(shutdown_ms_file, &shutdown_ms_str))
    shutdown_ms = StringToInt64(shutdown_ms_str);
  file_util::Delete(shutdown_ms_file, false);

  if (type == NOT_VALID || shutdown_ms == 0 || num_procs == 0)
    return;

  const char *time_fmt = "Shutdown.%s.time";
  const char *time_per_fmt = "Shutdown.%s.time_per_process";
  std::string time;
  std::string time_per;
  if (type == WINDOW_CLOSE) {
    time = StringPrintf(time_fmt, "window_close");
    time_per = StringPrintf(time_per_fmt, "window_close");
  } else if (type == BROWSER_EXIT) {
    time = StringPrintf(time_fmt, "browser_exit");
    time_per = StringPrintf(time_per_fmt, "browser_exit");
  } else if (type == END_SESSION) {
    time = StringPrintf(time_fmt, "end_session");
    time_per = StringPrintf(time_per_fmt, "end_session");
  } else {
    NOTREACHED();
  }

  if (time.empty())
    return;

  // TODO(erikkay): change these to UMA histograms after a bit more testing.
  UMA_HISTOGRAM_TIMES(time.c_str(),
                      TimeDelta::FromMilliseconds(shutdown_ms));
  UMA_HISTOGRAM_TIMES(time_per.c_str(),
                      TimeDelta::FromMilliseconds(shutdown_ms / num_procs));
  UMA_HISTOGRAM_COUNTS_100("Shutdown.renderers.total", num_procs);
  UMA_HISTOGRAM_COUNTS_100("Shutdown.renderers.slow", num_procs_slow);
}

void ReadLastShutdownInfo() {
  PrefService* prefs = g_browser_process->local_state();
  ShutdownType type =
      static_cast<ShutdownType>(prefs->GetInteger(prefs::kShutdownType));
  int num_procs = prefs->GetInteger(prefs::kShutdownNumProcesses);
  int num_procs_slow = prefs->GetInteger(prefs::kShutdownNumProcessesSlow);
  // clear the prefs immediately so we don't pick them up on a future run
  prefs->SetInteger(prefs::kShutdownType, NOT_VALID);
  prefs->SetInteger(prefs::kShutdownNumProcesses, 0);
  prefs->SetInteger(prefs::kShutdownNumProcessesSlow, 0);

  // Read and delete the file on the file thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(
          &ReadLastShutdownFile, type, num_procs, num_procs_slow));
}

#if defined(OS_MACOSX)
void SetTryingToQuit(bool quitting) {
  g_trying_to_quit = quitting;
}

bool IsTryingToQuit() {
  return g_trying_to_quit;
}
#endif

}  // namespace browser_shutdown
