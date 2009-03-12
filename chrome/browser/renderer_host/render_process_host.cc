// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_process_host.h"

#include "base/rand_util.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"

namespace {

unsigned int GetMaxRendererProcessCount() {
  // Defines the maximum number of renderer processes according to the
  // amount of installed memory as reported by the OS. The table
  // values are calculated by assuming that you want the renderers to
  // use half of the installed ram and assuming that each tab uses
  // ~40MB, however the curve is not linear but piecewise linear with
  // interleaved slopes of 3 and 2.
  // If you modify this table you need to adjust browser\browser_uitest.cc
  // to match the expected number of processes.

  static const int kMaxRenderersByRamTier[] = {
    3,                        // less than 256MB
    6,                        //  256MB
    9,                        //  512MB
    12,                       //  768MB
    14,                       // 1024MB
    18,                       // 1280MB
    20,                       // 1536MB
    22,                       // 1792MB
    24,                       // 2048MB
    26,                       // 2304MB
    29,                       // 2560MB
    32,                       // 2816MB
    35,                       // 3072MB
    38,                       // 3328MB
    40                        // 3584MB
  };

  static unsigned int max_count = 0;
  if (!max_count) {
    size_t memory_tier = base::SysInfo::AmountOfPhysicalMemoryMB() / 256;
    if (memory_tier >= arraysize(kMaxRenderersByRamTier))
      max_count = chrome::kMaxRendererProcessCount;
    else
      max_count = kMaxRenderersByRamTier[memory_tier];
  }
  return max_count;
}

// Returns true if the given host is suitable for launching a new view
// associated with the given profile.
static bool IsSuitableHost(Profile* profile, RenderProcessHost* host) {
  return host->profile() == profile;
}

// the global list of all renderer processes
IDMap<RenderProcessHost> all_hosts;

}  // namespace

bool RenderProcessHost::run_renderer_in_process_ = false;

RenderProcessHost::RenderProcessHost(Profile* profile)
    : max_page_id_(-1),
      notified_termination_(false),
      pid_(-1),
      profile_(profile) {
}

RenderProcessHost::~RenderProcessHost() {
}

void RenderProcessHost::Attach(IPC::Channel::Listener* listener,
                               int routing_id) {
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHost::Release(int listener_id) {
  DCHECK(listeners_.Lookup(listener_id) != NULL);
  listeners_.Remove(listener_id);

  // Make sure that all associated resource requests are stopped.
  CancelResourceRequests(listener_id);

  // When no other owners of this object, we can delete ourselves
  if (listeners_.IsEmpty()) {
    if (!notified_termination_) {
      bool close_expected = true;
      NotificationService::current()->Notify(
          NotificationType::RENDERER_PROCESS_TERMINATED,
          Source<RenderProcessHost>(this),
          Details<bool>(&close_expected));
      notified_termination_ = true;
    }
    if (pid_ >= 0) {
      all_hosts.Remove(pid_);
      pid_ = -1;
    }
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void RenderProcessHost::ReportExpectingClose(int32 listener_id) {
  listeners_expecting_close_.insert(listener_id);
}

void RenderProcessHost::UpdateMaxPageID(int32 page_id) {
  if (page_id > max_page_id_)
    max_page_id_ = page_id;
}

// static
RenderProcessHost::iterator RenderProcessHost::begin() {
  return all_hosts.begin();
}

// static
RenderProcessHost::iterator RenderProcessHost::end() {
  return all_hosts.end();
}

// static
size_t RenderProcessHost::size() {
  return all_hosts.size();
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  return all_hosts.Lookup(render_process_id);
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost() {
  unsigned int renderer_process_count =
      static_cast<unsigned int>(all_hosts.size());

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a profile that has no existing renderers.
  //       This is OK in moderation, since the GetMaxRendererProcessCount()
  //       is conservative.

  return run_renderer_in_process() ||
         (renderer_process_count >= GetMaxRendererProcessCount());
}

// static
RenderProcessHost* RenderProcessHost::GetExistingProcessHost(Profile* profile) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(size());

  for (iterator iter = begin(); iter != end(); ++iter) {
    if (IsSuitableHost(profile, iter->second))
      suitable_renderers.push_back(iter->second);
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return NULL;
}

void RenderProcessHost::SetProcessID(int pid) {
  if (pid_ != -1) {
    // This object is being reused after a renderer crash.  Remove the old pid.
    all_hosts.Remove(pid_);
  }

  pid_ = pid;
  all_hosts.AddWithID(this, pid);
}
