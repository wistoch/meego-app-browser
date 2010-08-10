// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"

// Mainline routine for running as the service process.
int ServiceProcessMain(const MainFunctionParams& parameters) {
  MessageLoopForUI main_message_loop;
  PlatformThread::SetName("CrServiceMain");

#if defined(OS_WIN)
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services)
    sandbox::InitBrokerServices(broker_services);
#endif   // defined(OS_WIN)

  ServiceProcess service_process;
  service_process.Initialize(&main_message_loop);

  // Enable Cloud Print if needed.
  if (parameters.command_line_.HasSwitch(switches::kEnableCloudPrintProxy)) {
    std::string lsid =
        parameters.command_line_.GetSwitchValueASCII(
            switches::kServiceAccountLsid);
    service_process.GetCloudPrintProxy()->EnableForUser(lsid);
  }

  MessageLoop::current()->Run();
  service_process.Teardown();
  return 0;
}
