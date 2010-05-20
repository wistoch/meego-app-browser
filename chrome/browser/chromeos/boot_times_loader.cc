// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_loader.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_switches.h"

namespace {

struct Stats {
  std::string uptime;
  std::string disk;

  Stats() : uptime(std::string()), disk(std::string()) {}
};

}

namespace chromeos {

// File uptime logs are located in.
static const char kLogPath[] = "/tmp";
// Prefix for the time measurement files.
static const char kUptimePrefix[] = "uptime-";
// Prefix for the disk usage files.
static const char kDiskPrefix[] = "disk-";
// Name of the time that Chrome's main() is called.
static const char kChromeMain[] = "chrome-main";
// Delay in milliseconds between file read attempts.
static const int64 kReadAttemptDelayMs = 250;

BootTimesLoader::BootTimesLoader() : backend_(new Backend()) {
}

BootTimesLoader::Handle BootTimesLoader::GetBootTimes(
    CancelableRequestConsumerBase* consumer,
    BootTimesLoader::GetBootTimesCallback* callback) {
  if (!g_browser_process->file_thread()) {
    // This should only happen if Chrome is shutting down, so we don't do
    // anything.
    return 0;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTestType)) {
    // TODO(davemoore) This avoids boottimes for tests. This needs to be
    // replaced with a mock of BootTimesLoader.
    return 0;
  }

  scoped_refptr<CancelableRequest<GetBootTimesCallback> > request(
      new CancelableRequest<GetBootTimesCallback>(callback));
  AddRequest(request, consumer);

  ChromeThread::PostTask(
      ChromeThread::FILE,
      FROM_HERE,
      NewRunnableMethod(backend_.get(), &Backend::GetBootTimes, request));
  return request->handle();
}

// Extracts the uptime value from files located in /tmp, returning the
// value as a double in value.
static bool GetTime(const std::string& log, double* value) {
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(log);
  std::string contents;
  *value = 0.0;
  if (file_util::ReadFileToString(log_file, &contents)) {
    size_t space_index = contents.find(' ');
    size_t chars_left =
        space_index != std::string::npos ? space_index : std::string::npos;
    std::string value_string = contents.substr(0, chars_left);
    return StringToDouble(value_string, value);
  }
  return false;
}

void BootTimesLoader::Backend::GetBootTimes(
    scoped_refptr<GetBootTimesRequest> request) {
  const char* kFirmwareBootTime = "firmware-boot-time";
  const char* kPreStartup = "pre-startup";
  const char* kChromeExec = "chrome-exec";
  const char* kChromeMain = "chrome-main";
  const char* kXStarted = "x-started";
  const char* kLoginPromptReady = "login-prompt-ready";
  std::string uptime_prefix = kUptimePrefix;

  if (request->canceled())
    return;

  // Wait until login_prompt_ready is output by reposting.
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(uptime_prefix + kLoginPromptReady);
  if (!file_util::PathExists(log_file)) {
    ChromeThread::PostDelayedTask(
        ChromeThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this, &Backend::GetBootTimes, request),
        kReadAttemptDelayMs);
    return;
  }

  BootTimes boot_times;

  GetTime(kFirmwareBootTime, &boot_times.firmware);
  GetTime(uptime_prefix + kPreStartup, &boot_times.pre_startup);
  GetTime(uptime_prefix + kXStarted, &boot_times.x_started);
  GetTime(uptime_prefix + kChromeExec, &boot_times.chrome_exec);
  GetTime(uptime_prefix + kChromeMain, &boot_times.chrome_main);
  GetTime(uptime_prefix + kLoginPromptReady, &boot_times.login_prompt_ready);

  request->ForwardResult(
      GetBootTimesCallback::TupleType(request->handle(), boot_times));
}

static void RecordStatsDelayed(
    const std::string& name,
    const std::string& uptime,
    const std::string& disk) {
  const FilePath log_path(kLogPath);
  std::string disk_prefix = kDiskPrefix;
  const FilePath uptime_output =
      log_path.Append(FilePath(kUptimePrefix + name));
  const FilePath disk_output = log_path.Append(FilePath(kDiskPrefix + name));

  // Write out the files, ensuring that they don't exist already.
  if (!file_util::PathExists(uptime_output))
    file_util::WriteFile(uptime_output, uptime.data(), uptime.size());
  if (!file_util::PathExists(disk_output))
    file_util::WriteFile(disk_output, disk.data(), disk.size());
}

static void RecordStats(
    const std::string& name, const Stats& stats) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(RecordStatsDelayed, name, stats.uptime, stats.disk));
}

static Stats GetCurrentStats() {
  const FilePath kProcUptime("/proc/uptime");
  const FilePath kDiskStat("/sys/block/sda/stat");
  Stats stats;

  file_util::ReadFileToString(kProcUptime, &stats.uptime);
  file_util::ReadFileToString(kDiskStat, &stats.disk);
  return stats;
}

// static
void BootTimesLoader::RecordCurrentStats(const std::string& name) {
  Stats stats = GetCurrentStats();
  RecordStats(name, stats);
}

// Used to hold the stats at main().
static Stats chrome_main_stats_;

void BootTimesLoader::SaveChromeMainStats() {
  chrome_main_stats_ = GetCurrentStats();
}

void BootTimesLoader::RecordChromeMainStats() {
  RecordStats(kChromeMain, chrome_main_stats_);
}

}  // namespace chromeos
