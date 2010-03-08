// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_broker_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/common/chrome_switches.h"

NaClBrokerService* NaClBrokerService::GetInstance() {
  return Singleton<NaClBrokerService>::get();
}

NaClBrokerService::NaClBrokerService()
    : broker_started_(false),
      broker_host_(NULL),
      loaders_running_(0),
      resource_dispatcher_host_(NULL),
      initialized_(false) {
}

void NaClBrokerService::Init(ResourceDispatcherHost* resource_dispatcher_host) {
  if (!initialized_)
    resource_dispatcher_host_ = resource_dispatcher_host;

  if (broker_host_ == NULL)
    StartBroker();

  initialized_ = true;
}

bool NaClBrokerService::StartBroker() {
  broker_host_.reset(new NaClBrokerHost(resource_dispatcher_host_));
  if (!broker_host_->Init()) {
    // Initialization failed, we will not retry in the future
    broker_host_.reset(NULL);
  }
  return (broker_host_ != NULL);
}

bool NaClBrokerService::LaunchLoader(NaClProcessHost* nacl_process_host,
                                     const std::wstring& loader_channel_id) {
  // Add task to the list
  pending_launches_[loader_channel_id] = nacl_process_host;
  if (broker_started_) {
    // If the broker is not ready yet
    // we will call LaunchLoader in OnBrokerStarted
    broker_host_->LaunchLoader(loader_channel_id);
  }
  return true;
}

void NaClBrokerService::OnBrokerStarted() {
  PendingLaunchesMap::iterator it;
  for (it  = pending_launches_.begin(); it != pending_launches_.end(); it++)
    broker_host_->LaunchLoader(it->first);

  broker_started_ = true;
}

void NaClBrokerService::OnLoaderLaunched(const std::wstring& channel_id,
                                         base::ProcessHandle handle) {
  NaClProcessHost* client;
  PendingLaunchesMap::iterator it = pending_launches_.find(channel_id);
  if (pending_launches_.end() == it)
    NOTREACHED();

  client = it->second;
  client->OnProcessLaunchedByBroker(handle);
  pending_launches_.erase(it);
  ++loaders_running_;
}

void NaClBrokerService::OnLoaderDied() {
  --loaders_running_;
  // Stop the broker only if there are no loaders running or being launched.
  if (loaders_running_ + pending_launches_.size() == 0 &&
      broker_host_ != NULL) {
    broker_host_->StopBroker();
    // Reset the pointer to the broker host.
    OnBrokerDied();
  }
}

void NaClBrokerService::OnBrokerDied() {
  // NaClBrokerHost object will be destructed by ChildProcessHost
  broker_started_ = false;
  broker_host_.release();
}
