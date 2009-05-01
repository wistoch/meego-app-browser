// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webworker_proxy.h"

#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorkerClient.h"

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorkerClient;

WebWorkerProxy::WebWorkerProxy(
    WebWorkerClient* client,
    ChildThread* child_thread,
    int render_view_route_id)
    : route_id_(MSG_ROUTING_NONE),
      child_thread_(child_thread),
      render_view_route_id_(render_view_route_id),
      client_(client) {
}

WebWorkerProxy::~WebWorkerProxy() {
}

void WebWorkerProxy::startWorkerContext(
    const WebURL& script_url,
    const WebString& user_agent,
    const WebString& source_code) {
  child_thread_->Send(new ViewHostMsg_CreateDedicatedWorker(
      script_url, render_view_route_id_, &route_id_));
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  child_thread_->AddRoute(route_id_, this);
  Send(new WorkerMsg_StartWorkerContext(
      route_id_, script_url, user_agent, source_code));

  for (size_t i = 0; i < queued_messages_.size(); ++i) {
    queued_messages_[i]->set_routing_id(route_id_);
    Send(queued_messages_[i]);
  }
  queued_messages_.clear();
}

void WebWorkerProxy::terminateWorkerContext() {
  if (route_id_ != MSG_ROUTING_NONE) {
    Send(new WorkerMsg_TerminateWorkerContext(route_id_));
    child_thread_->RemoveRoute(route_id_);
    route_id_ = MSG_ROUTING_NONE;
  }
}

void WebWorkerProxy::postMessageToWorkerContext(
    const WebString& message) {
  Send(new WorkerMsg_PostMessageToWorkerContext(route_id_, message));
}

void WebWorkerProxy::workerObjectDestroyed() {
  Send(new WorkerMsg_WorkerObjectDestroyed(route_id_));
  delete this;
}

bool WebWorkerProxy::Send(IPC::Message* message) {
  if (route_id_ == MSG_ROUTING_NONE) {
    queued_messages_.push_back(message);
    return true;
  }

  // For now we proxy all messages to the worker process through the browser.
  // Revisit if we find this slow.
  // TODO(jabdelmalek): handle sync messages if we need them.
  IPC::Message* wrapped_msg = new ViewHostMsg_ForwardToWorker(*message);
  delete message;
  return child_thread_->Send(wrapped_msg);
}

void WebWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  if (!client_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerProxy, message)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostMessageToWorkerObject,
                        client_,
                        WebWorkerClient::postMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostExceptionToWorkerObject,
                        client_,
                        WebWorkerClient::postExceptionToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                        client_,
                        WebWorkerClient::postConsoleMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                        client_,
                        WebWorkerClient::confirmMessageFromWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ReportPendingActivity,
                        client_,
                        WebWorkerClient::reportPendingActivity)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextDestroyed,
                        client_,
                        WebWorkerClient::workerContextDestroyed)
  IPC_END_MESSAGE_MAP()
}
