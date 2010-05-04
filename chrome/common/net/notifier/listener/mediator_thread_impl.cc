// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/notifier/listener/mediator_thread_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "chrome/common/net/notifier/base/async_dns_lookup.h"
#include "chrome/common/net/notifier/base/task_pump.h"
#include "chrome/common/net/notifier/communicator/connection_options.h"
#include "chrome/common/net/notifier/communicator/const_communicator.h"
#include "chrome/common/net/notifier/communicator/xmpp_connection_generator.h"
#include "chrome/common/net/notifier/listener/listen_task.h"
#include "chrome/common/net/notifier/listener/send_update_task.h"
#include "chrome/common/net/notifier/listener/subscribe_task.h"
#include "talk/base/thread.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"

using std::string;

namespace notifier {

MediatorThreadImpl::MediatorThreadImpl() {}

MediatorThreadImpl::~MediatorThreadImpl() {
}

void MediatorThreadImpl::Start() {
  talk_base::Thread::Start();
}

void MediatorThreadImpl::Run() {
  PlatformThread::SetName("Notifier_MediatorThread");
  // For win32, this sets up the win32socketserver. Note that it needs to
  // dispatch windows messages since that is what the win32 socket server uses.

  MessageLoop message_loop;

  Post(this, CMD_PUMP_AUXILIARY_LOOPS);
  ProcessMessages(talk_base::kForever);
}

void MediatorThreadImpl::PumpAuxiliaryLoops() {
  if (pump_.get() && pump_->HasPendingTimeoutTask()) {
    pump_->WakeTasks();
  }
  MessageLoop::current()->RunAllPending();
  // We want to pump auxiliary loops every 100ms until this thread is stopped,
  // at which point this call will do nothing.
  PostDelayed(100, this, CMD_PUMP_AUXILIARY_LOOPS);
}

void MediatorThreadImpl::Login(const buzz::XmppClientSettings& settings) {
  Post(this, CMD_LOGIN, new LoginData(settings));
}

void MediatorThreadImpl::Stop() {
  Thread::Stop();
  CHECK(!login_.get() && !pump_.get()) << "Logout should be called prior to"
      << "message queue exit.";
}

void MediatorThreadImpl::Logout() {
  CHECK(!IsQuitting())
      << "Logout should be called prior to message queue exit.";
  Post(this, CMD_DISCONNECT);
  Stop();
}

void MediatorThreadImpl::ListenForUpdates() {
  Post(this, CMD_LISTEN_FOR_UPDATES);
}

void MediatorThreadImpl::SubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  Post(this, CMD_SUBSCRIBE_FOR_UPDATES,
       new SubscriptionData(subscribed_services_list));
}

void MediatorThreadImpl::SendNotification(
    const OutgoingNotificationData& data) {
  Post(this, CMD_SEND_NOTIFICATION, new OutgoingNotificationMessageData(data));
}

void MediatorThreadImpl::ProcessMessages(int milliseconds) {
  talk_base::Thread::ProcessMessages(milliseconds);
}

void MediatorThreadImpl::OnMessage(talk_base::Message* msg) {
  scoped_ptr<LoginData> data;
  switch (msg->message_id) {
    case CMD_LOGIN:
      DCHECK(msg->pdata);
      data.reset(reinterpret_cast<LoginData*>(msg->pdata));
      DoLogin(data.get());
      break;
    case CMD_DISCONNECT:
      DoDisconnect();
      break;
    case CMD_LISTEN_FOR_UPDATES:
      DoListenForUpdates();
      break;
    case CMD_SEND_NOTIFICATION: {
      DCHECK(msg->pdata);
      scoped_ptr<OutgoingNotificationMessageData> data(
          reinterpret_cast<OutgoingNotificationMessageData*>(msg->pdata));
      DoSendNotification(*data);
      break;
    }
    case CMD_SUBSCRIBE_FOR_UPDATES: {
      DCHECK(msg->pdata);
      scoped_ptr<SubscriptionData> subscription_data(
          reinterpret_cast<SubscriptionData*>(msg->pdata));
      DoSubscribeForUpdates(*subscription_data);
      break;
    }
    case CMD_PUMP_AUXILIARY_LOOPS:
      PumpAuxiliaryLoops();
      break;
    default:
      LOG(ERROR) << "P2P: Someone passed a bad message to the thread.";
      break;
  }
}

void MediatorThreadImpl::DoLogin(LoginData* login_data) {
  LOG(INFO) << "P2P: Thread logging into talk network.";
  buzz::XmppClientSettings& user_settings = login_data->user_settings;

  // Start a new pump for the login.
  login_.reset();
  pump_.reset(new notifier::TaskPump());

  notifier::ServerInformation server_list[2];
  int server_list_count = 2;

  // The default servers know how to serve over port 443 (that's the magic).
  server_list[0].server = talk_base::SocketAddress("talk.google.com",
                                                   notifier::kDefaultXmppPort,
                                                   true);  // Use DNS.
  server_list[0].special_port_magic = true;
  server_list[1].server = talk_base::SocketAddress("talkx.l.google.com",
                                                   notifier::kDefaultXmppPort,
                                                   true);  // Use DNS.
  server_list[1].special_port_magic = true;

  // Autodetect proxy is on by default.
  notifier::ConnectionOptions options;

  // Language is not used in the stanza so we default to |en|.
  std::string lang = "en";
  login_.reset(new notifier::Login(pump_.get(),
                                   user_settings,
                                   options,
                                   lang,
                                   server_list,
                                   server_list_count,
                                   // NetworkStatusDetectionTask will be
                                   // created for you if NULL is passed in.
                                   // It helps shorten the autoreconnect
                                   // time after going offline and coming
                                   // back online.
                                   NULL,
                                   // talk_base::FirewallManager* is NULL.
                                   NULL,
                                   // Both the proxy and a non-proxy route
                                   // will be attempted.
                                   false,
                                   // |previous_login_successful| is true
                                   // because we have already done a
                                   // successful gaia login at this point
                                   // through another mechanism.
                                   true));

  login_->SignalClientStateChange.connect(
      this, &MediatorThreadImpl::OnClientStateChangeMessage);
  login_->SignalLoginFailure.connect(
      this, &MediatorThreadImpl::OnLoginFailureMessage);
  login_->StartConnection();
}

void MediatorThreadImpl::OnInputDebug(const char* msg, int length) {
  string output(msg, length);
  LOG(INFO) << "P2P: OnInputDebug:" << output << ".";
}

void MediatorThreadImpl::OnOutputDebug(const char* msg, int length) {
  string output(msg, length);
  LOG(INFO) << "P2P: OnOutputDebug:" << output << ".";
}

void MediatorThreadImpl::DoDisconnect() {
  LOG(INFO) << "P2P: Thread logging out of talk network.";
  login_.reset();
  // Delete the old pump while on the thread to ensure that everything is
  // cleaned-up in a predicatable manner.
  pump_.reset();
}

void MediatorThreadImpl::DoSubscribeForUpdates(
    const SubscriptionData& subscription_data) {
  buzz::XmppClient* client = xmpp_client();
  // If there isn't an active xmpp client, return.
  if (!client) {
    return;
  }
  SubscribeTask* subscription =
      new SubscribeTask(client, subscription_data.subscribed_services_list);
  subscription->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnSubscriptionStateChange);
  subscription->Start();
}

void MediatorThreadImpl::DoListenForUpdates() {
  buzz::XmppClient* client = xmpp_client();
  // If there isn't an active xmpp client, return.
  if (!client) {
    return;
  }
  ListenTask* listener = new ListenTask(client);
  listener->SignalUpdateAvailable.connect(
      this,
      &MediatorThreadImpl::OnUpdateListenerMessage);
  listener->Start();
}

void MediatorThreadImpl::DoSendNotification(
    const OutgoingNotificationMessageData& data) {
  buzz::XmppClient* client = xmpp_client();
  // If there isn't an active xmpp client, return.
  if (!client) {
    return;
  }
  SendUpdateTask* task = new SendUpdateTask(client, data.notification_data);
  task->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnUpdateNotificationSent);
  task->Start();
}

void MediatorThreadImpl::OnUpdateListenerMessage(
    const IncomingNotificationData& notification_data) {
  SignalNotificationReceived(notification_data);
}

void MediatorThreadImpl::OnUpdateNotificationSent(bool success) {
  if (success) {
    SignalStateChange(MSG_NOTIFICATION_SENT);
  }
}

void MediatorThreadImpl::OnLoginFailureMessage(
    const notifier::LoginFailure& failure) {
  SignalStateChange(MSG_LOGGED_OUT);
}

void MediatorThreadImpl::OnClientStateChangeMessage(
    notifier::Login::ConnectionState state) {
  switch (state) {
    case notifier::Login::STATE_CLOSED:
      SignalStateChange(MSG_LOGGED_OUT);
      break;
    case notifier::Login::STATE_RETRYING:
    case notifier::Login::STATE_OPENING:
      LOG(INFO) << "P2P: Thread trying to connect.";
      // Maybe first time logon, maybe intermediate network disruption. Assume
      // the server went down, and lost our subscription for updates.
      SignalStateChange(MSG_SUBSCRIPTION_FAILURE);
      break;
    case notifier::Login::STATE_OPENED:
      SignalStateChange(MSG_LOGGED_IN);
      break;
    default:
      LOG(WARNING) << "P2P: Unknown client state change.";
      break;
  }
}

void MediatorThreadImpl::OnSubscriptionStateChange(bool success) {
  if (success) {
    SignalStateChange(MSG_SUBSCRIPTION_SUCCESS);
  } else {
    SignalStateChange(MSG_SUBSCRIPTION_FAILURE);
  }
}

buzz::XmppClient* MediatorThreadImpl::xmpp_client() {
  if (!login_.get()) {
    return NULL;
  }
  return login_->xmpp_client();
}

}  // namespace notifier
