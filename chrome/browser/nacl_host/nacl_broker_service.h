// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_H_

#include <map>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/nacl_host/nacl_broker_host.h"

class NaClProcessHost;

class NaClBrokerService {
 public:
  // Returns the NaClBrokerService singleton.
  static NaClBrokerService* GetInstance();

  void Init(ResourceDispatcherHost* resource_dispatcher_host);

  // Can be called several times, must be called before LaunchLoader.
  bool StartBroker();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(NaClProcessHost* client,
                    const std::wstring& loader_channel_id);

  // Called by NaClBrokerHost to notify the service
  // that the broker was launched.
  void OnBrokerStarted();

  // Called by NaClBrokerHost to notify the service that a loader was launched.
  void OnLoaderLaunched(const std::wstring& channel_id,
                        base::ProcessHandle handle);

  // Called by NaClProcessHost when a loader process is terminated
  void OnLoaderDied();

  // Called by NaClBrokerHost when the broker process is terminated.
  void OnBrokerDied();

 private:
  typedef std::map<std::wstring, NaClProcessHost*>
      PendingLaunchesMap;

  friend struct DefaultSingletonTraits<NaClBrokerService>;

  NaClBrokerService();
  ~NaClBrokerService() {}

  bool broker_started_;
  scoped_ptr<NaClBrokerHost> broker_host_;
  int loaders_running_;
  bool initialized_;
  ResourceDispatcherHost* resource_dispatcher_host_;
  PendingLaunchesMap pending_launches_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerService);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_H_
