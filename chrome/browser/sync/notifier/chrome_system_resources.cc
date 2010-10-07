// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"

namespace sync_notifier {

ChromeSystemResources::ChromeSystemResources() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

ChromeSystemResources::~ChromeSystemResources() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  StopScheduler();
}

invalidation::Time ChromeSystemResources::current_time() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return base::Time::Now();
}

void ChromeSystemResources::StartScheduler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  scoped_runnable_method_factory_.reset(
      new ScopedRunnableMethodFactory<ChromeSystemResources>(this));
}

void ChromeSystemResources::StopScheduler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  scoped_runnable_method_factory_.reset();
  STLDeleteElements(&posted_tasks_);
}

void ChromeSystemResources::ScheduleWithDelay(
    invalidation::TimeDelta delay,
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Task* task_to_post = MakeTaskToPost(task);
  if (!task_to_post) {
    return;
  }
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, task_to_post, delay.InMillisecondsRoundedUp());
}

void ChromeSystemResources::ScheduleImmediately(
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Task* task_to_post = MakeTaskToPost(task);
  if (!task_to_post) {
    return;
  }
  MessageLoop::current()->PostTask(FROM_HERE, task_to_post);
}

// The listener thread is just our current thread.
void ChromeSystemResources::ScheduleOnListenerThread(
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  ScheduleImmediately(task);
}

// We're already on a separate thread, so always return true.
bool ChromeSystemResources::IsRunningOnInternalThread() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return true;
}

void ChromeSystemResources::Log(
    LogLevel level, const char* file, int line,
    const char* format, ...) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  logging::LogSeverity log_severity = logging::LOG_INFO;
  switch (level) {
    case INFO_LEVEL:
      log_severity = logging::LOG_INFO;
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      break;
    case ERROR_LEVEL:
      log_severity = logging::LOG_ERROR;
      break;
  }
  // We treat LOG(INFO) as VLOG(1).
  if ((log_severity >= logging::GetMinLogLevel()) &&
      ((log_severity != logging::LOG_INFO) ||
       (1 <= logging::GetVlogLevelHelper(file, ::strlen(file))))) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

void ChromeSystemResources::WriteState(
    const invalidation::string& state,
    invalidation::StorageCallback* callback) {
  // TODO(akalin): Write the given state to persistent storage.
  callback->Run(false);
  delete callback;
}

Task* ChromeSystemResources::MakeTaskToPost(
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(task));
  if (!scoped_runnable_method_factory_.get()) {
    delete task;
    return NULL;
  }
  posted_tasks_.insert(task);
  Task* task_to_post =
      scoped_runnable_method_factory_->NewRunnableMethod(
          &ChromeSystemResources::RunPostedTask, task);
  return task_to_post;
}

void ChromeSystemResources::RunPostedTask(invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RunAndDeleteClosure(task);
  posted_tasks_.erase(task);
}

}  // namespace sync_notifier
