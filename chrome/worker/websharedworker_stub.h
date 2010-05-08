// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEBSHAREDWORKER_STUB_H_
#define CHROME_WORKER_WEBSHAREDWORKER_STUB_H_

#include "chrome/worker/webworker_stub_base.h"
#include "chrome/worker/webworkerclient_proxy.h"
#include "googleurl/src/gurl.h"

namespace WebKit {
class WebSharedWorker;
}

// This class creates a WebSharedWorker, and translates incoming IPCs to the
// appropriate WebSharedWorker APIs.
class WebSharedWorkerStub : public WebWorkerStubBase {
 public:
  WebSharedWorkerStub(const string16& name, int route_id,
                      const WorkerAppCacheInitInfo& appcache_init_info);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

 private:
  virtual ~WebSharedWorkerStub();

  // Invoked when the WebWorkerClientProxy is shutting down.
  void OnConnect(int sent_message_port_id, int routing_id);
  void OnStartWorkerContext(
      const GURL& url, const string16& user_agent, const string16& source_code);
  void OnTerminateWorkerContext();

  WebKit::WebSharedWorker* impl_;
  string16 name_;
  bool started_;

  typedef std::pair<int, int> PendingConnectInfo;
  typedef std::vector<PendingConnectInfo> PendingConnectInfoList;
  PendingConnectInfoList pending_connects_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerStub);
};

#endif  // CHROME_WORKER_WEBSHAREDWORKER_STUB_H_
