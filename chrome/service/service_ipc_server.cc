// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_ipc_server.h"

#include "chrome/common/service_messages.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"
#include "ipc/ipc_logging.h"

ServiceIPCServer::ServiceIPCServer(const std::string& channel_name)
    : channel_name_(channel_name), client_connected_(false) {
}

bool ServiceIPCServer::Init() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(this);
#endif
  sync_message_filter_ =
      new IPC::SyncMessageFilter(g_service_process->shutdown_event());
  CreateChannel();
  return true;
}

void ServiceIPCServer::CreateChannel() {
  channel_.reset(new IPC::SyncChannel(channel_name_,
      IPC::Channel::MODE_SERVER, this,
      g_service_process->io_thread()->message_loop(), true,
      g_service_process->shutdown_event()));
  DCHECK(sync_message_filter_.get());
  channel_->AddFilter(sync_message_filter_.get());
}

ServiceIPCServer::~ServiceIPCServer() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCMessageLoop();
}

void ServiceIPCServer::OnChannelConnected(int32 peer_pid) {
  DCHECK(!client_connected_);
  client_connected_ = true;
}

void ServiceIPCServer::OnChannelError() {
  // When a client (typically a browser process) disconnects, the pipe is
  // closed and we get an OnChannelError. Since we want to keep servicing
  // client requests, we will recreate the channel.
  bool client_was_connected = client_connected_;
  client_connected_ = false;
  // TODO(sanjeevr): Instead of invoking the service process for such handlers,
  // define a Client interface that the ServiceProcess can implement.
  if (client_was_connected && g_service_process->HandleClientDisconnect()) {
    CreateChannel();
  }
}

bool ServiceIPCServer::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

void ServiceIPCServer::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(ServiceIPCServer, msg)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxy,
                        OnEnableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxyWithTokens,
                        OnEnableCloudPrintProxyWithTokens)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableRemotingWithTokens,
                        OnEnableRemotingWithTokens)
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableCloudPrintProxy,
                        OnDisableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_IsCloudPrintProxyEnabled,
                        OnIsCloudPrintProxyEnabled)
    IPC_MESSAGE_HANDLER(ServiceMsg_Hello, OnHello);
    IPC_MESSAGE_HANDLER(ServiceMsg_Shutdown, OnShutdown);
    IPC_MESSAGE_HANDLER(ServiceMsg_UpdateAvailable, OnUpdateAvailable);
  IPC_END_MESSAGE_MAP()
}

void ServiceIPCServer::OnEnableCloudPrintProxy(const std::string& lsid) {
  g_service_process->GetCloudPrintProxy()->EnableForUser(lsid);
}

void ServiceIPCServer::OnEnableCloudPrintProxyWithTokens(
    const std::string& cloud_print_token, const std::string& talk_token) {
  // TODO(sanjeevr): Implement this.
  NOTIMPLEMENTED();
}

void ServiceIPCServer::OnIsCloudPrintProxyEnabled() {
  std::string email;
  bool is_enabled = g_service_process->GetCloudPrintProxy()->IsEnabled(&email);
  channel_->Send(new ServiceHostMsg_CloudPrintProxy_IsEnabled(is_enabled,
                                                              email));
}

void ServiceIPCServer::OnEnableRemotingWithTokens(
    const std::string& login,
    const std::string& remoting_token,
    const std::string& talk_token) {
#if defined(ENABLE_REMOTING)
  g_service_process->EnableChromotingHostWithTokens(login, remoting_token,
                                                    talk_token);
#endif
}

void ServiceIPCServer::OnDisableCloudPrintProxy() {
  g_service_process->GetCloudPrintProxy()->DisableForUser();
}

void ServiceIPCServer::OnHello() {
  channel_->Send(new ServiceHostMsg_GoodDay());
}

void ServiceIPCServer::OnShutdown() {
  g_service_process->Shutdown();
}

void ServiceIPCServer::OnUpdateAvailable() {
  g_service_process->SetUpdateAvailable();
}

