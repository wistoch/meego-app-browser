// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <set>
#include <string>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_host.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/process_info_snapshot.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"

// TODO(viettrungluu): Many of the TODOs below are subsumed by a general need to
// refactor the about:memory code (not just on Mac, but probably on other
// platforms as well). I've filed crbug.com/25456.

class RenderViewHostDelegate;

// Known browsers which we collect details for. |CHROME_BROWSER| *must* be the
// first browser listed. The order here must match those in |process_template|
// (in |MemoryDetails::MemoryDetails()| below).
// TODO(viettrungluu): In the big refactoring (see above), get rid of this order
// dependence.
enum BrowserType {
  // TODO(viettrungluu): possibly add more?
  CHROME_BROWSER = 0,
  SAFARI_BROWSER,
  FIREFOX_BROWSER,
  CAMINO_BROWSER,
  OPERA_BROWSER,
  OMNIWEB_BROWSER,
  MAX_BROWSERS
} BrowserProcess;


MemoryDetails::MemoryDetails() {
  static const std::wstring google_browser_name =
      l10n_util::GetString(IDS_PRODUCT_NAME);
  // (Human and process) names of browsers; should match the ordering for
  // |BrowserProcess| (i.e., |BrowserType|).
  // TODO(viettrungluu): The current setup means that we can't detect both
  // Chrome and Chromium at the same time!
  // TODO(viettrungluu): Get localized browser names for other browsers
  // (crbug.com/25779).
  ProcessData process_template[MAX_BROWSERS] = {
    { google_browser_name.c_str(), chrome::kBrowserProcessExecutableName, },
    { L"Safari", L"Safari", },
    { L"Firefox", L"firefox-bin", },
    { L"Camino", L"Camino", },
    { L"Opera", L"Opera", },
    { L"OmniWeb", L"OmniWeb", },
  };

  for (size_t index = 0; index < arraysize(process_template); ++index) {
    ProcessData process;
    process.name = process_template[index].name;
    process.process_name = process_template[index].process_name;
    process_data_.push_back(process);
  }
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[CHROME_BROWSER];
}

void MemoryDetails::CollectProcessData(
    std::vector<ProcessMemoryInformation> child_info) {
  // This must be run on the file thread to avoid jank (|ProcessInfoSnapshot|
  // runs /bin/ps, which isn't instantaneous).
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Clear old data.
  for (size_t index = 0; index < MAX_BROWSERS; index++)
    process_data_[index].processes.clear();

  // First, we use |NamedProcessIterator| to get the PIDs of the processes we're
  // interested in; we save our results to avoid extra calls to
  // |NamedProcessIterator| (for performance reasons) and to avoid additional
  // inconsistencies caused by racing. Then we run |/bin/ps| *once* to get
  // information on those PIDs. Then we used our saved information to iterate
  // over browsers, then over PIDs.

  // Get PIDs of main browser processes.
  std::vector<base::ProcessId> pids_by_browser[MAX_BROWSERS];
  std::vector<base::ProcessId> all_pids;
  for (size_t index = CHROME_BROWSER; index < MAX_BROWSERS; index++) {
    base::NamedProcessIterator process_it(process_data_[index].process_name,
                                          NULL);

    while (const ProcessEntry* process_entry = process_it.NextProcessEntry()) {
      pids_by_browser[index].push_back(process_entry->pid);
      all_pids.push_back(process_entry->pid);
    }
  }

  // Get PIDs of helpers.
  std::vector<base::ProcessId> helper_pids;
  {
    base::NamedProcessIterator helper_it(chrome::kHelperProcessExecutableName,
                                         NULL);
    while (const ProcessEntry* process_entry = helper_it.NextProcessEntry()) {
      helper_pids.push_back(process_entry->pid);
      all_pids.push_back(process_entry->pid);
    }
  }

  // Capture information about the processes we're interested in.
  ProcessInfoSnapshot process_info;
  process_info.Sample(all_pids);

  // Handle the other processes first.
  for (size_t index = CHROME_BROWSER + 1; index < MAX_BROWSERS; index++) {
    for (std::vector<base::ProcessId>::const_iterator it =
         pids_by_browser[index].begin();
         it != pids_by_browser[index].end(); ++it) {
      ProcessMemoryInformation info;
      info.pid = *it;
      info.type = ChildProcessInfo::UNKNOWN_PROCESS;

      // Try to get version information. To do this, we need first to get the
      // executable's name (we can only believe |proc_info.command| if it looks
      // like an absolute path). Then we need strip the executable's name back
      // to the bundle's name. And only then can we try to get the version.
      scoped_ptr<FileVersionInfo> version_info;
      ProcessInfoSnapshot::ProcInfoEntry proc_info;
      if (process_info.GetProcInfo(info.pid, &proc_info)) {
        if (proc_info.command.length() > 1 && proc_info.command[0] == '/') {
          FilePath bundle_name =
              mac_util::GetAppBundlePath(FilePath(proc_info.command));
          if (!bundle_name.empty()) {
            version_info.reset(FileVersionInfo::CreateFileVersionInfo(
                bundle_name));
          }
        }
      }
      if (version_info.get()) {
        info.product_name = version_info->product_name();
        info.version = version_info->product_version();
      } else {
        info.product_name = process_data_[index].name;
        info.version = L"";
      }

      // Memory info.
      process_info.GetCommittedKBytesOfPID(info.pid, &info.committed);
      process_info.GetWorkingSetKBytesOfPID(info.pid, &info.working_set);

      // Add the process info to our list.
      process_data_[index].processes.push_back(info);
    }
  }

  // Collect data about Chrome/Chromium.
  for (std::vector<base::ProcessId>::const_iterator it =
       pids_by_browser[CHROME_BROWSER].begin();
       it != pids_by_browser[CHROME_BROWSER].end(); ++it) {
    CollectProcessDataChrome(child_info, *it, process_info);
  }

  // And collect data about the helpers.
  for (std::vector<base::ProcessId>::const_iterator it = helper_pids.begin();
       it != helper_pids.end(); ++it) {
    CollectProcessDataChrome(child_info, *it, process_info);
  }

  // Finally return to the browser thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnUIThread));
}

void MemoryDetails::CollectProcessDataChrome(
    const std::vector<ProcessMemoryInformation>& child_info,
    base::ProcessId pid,
    const ProcessInfoSnapshot& process_info) {
  ProcessMemoryInformation info;
  info.pid = pid;
  if (info.pid == base::GetCurrentProcId())
    info.type = ChildProcessInfo::BROWSER_PROCESS;
  else
    info.type = ChildProcessInfo::UNKNOWN_PROCESS;

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    info.product_name = version_info->product_name();
    info.version = version_info->product_version();
  } else {
    info.product_name = process_data_[CHROME_BROWSER].name;
    info.version = L"";
  }

  // Check if this is one of the child processes whose data we collected
  // on the IO thread, and if so copy over that data.
  for (size_t child = 0; child < child_info.size(); child++) {
    if (child_info[child].pid == info.pid) {
      info.titles = child_info[child].titles;
      info.type = child_info[child].type;
      break;
    }
  }

  // Memory info.
  process_info.GetCommittedKBytesOfPID(info.pid, &info.committed);
  process_info.GetWorkingSetKBytesOfPID(info.pid, &info.working_set);

  // Add the process info to our list.
  process_data_[CHROME_BROWSER].processes.push_back(info);
}
