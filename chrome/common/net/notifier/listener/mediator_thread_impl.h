// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This object runs on a thread and knows how to interpret messages sent by the
// talk mediator. The mediator posts messages to a queue which the thread polls
// (in a super class).
//
// Example usage:
//
//   MediatorThread m = new MediatorThreadImpl(pass in stuff);
//   m.start(); // Start the thread
//   // Once the thread is started, you can do server stuff.
//   m.Login(loginInformation);
//   // Events happen, the mediator finds out through its pump more messages
//   // are dispatched to the thread eventually we want to log out.
//   m.Logout();
//   delete m; // Also stops the thread.

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/notifier/communicator/login.h"
#include "chrome/common/net/notifier/communicator/login_failure.h"
#include "chrome/common/net/notifier/listener/mediator_thread.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace chrome_common_net {
class NetworkChangeNotifierThread;
}  // namespace chrome_common_net

namespace net {
class HostResolver;
class NetworkChangeNotifier;
}  // namespace net

namespace notifier {
class TaskPump;
}  // namespace notifier

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace talk_base {
class SocketServer;
}  // namespace talk_base

namespace notifier {

enum MEDIATOR_CMD {
  CMD_LOGIN,
  CMD_DISCONNECT,
  CMD_LISTEN_FOR_UPDATES,
  CMD_SEND_NOTIFICATION,
  CMD_SUBSCRIBE_FOR_UPDATES,
  CMD_PUMP_AUXILIARY_LOOPS,
};

// Used to pass authentication information from the mediator to the thread.
// Use new to allocate it on the heap, the thread will delete it for you.
struct LoginData : public talk_base::MessageData {
  explicit LoginData(const buzz::XmppClientSettings& settings)
      : user_settings(settings) {
  }
  virtual ~LoginData() {}

  buzz::XmppClientSettings user_settings;
};

// Used to pass subscription information from the mediator to the thread.
// Use new to allocate it on the heap, the thread will delete it for you.
struct SubscriptionData : public talk_base::MessageData {
  explicit SubscriptionData(const std::vector<std::string>& services)
      : subscribed_services_list(services) {
  }
  virtual ~SubscriptionData() {}

  std::vector<std::string> subscribed_services_list;
};

// Used to pass outgoing notification information from the mediator to the
// thread. Use new to allocate it on the heap, the thread will delete it
// for you.
struct OutgoingNotificationMessageData : public talk_base::MessageData {
  explicit OutgoingNotificationMessageData(
      const OutgoingNotificationData& data) : notification_data(data) {
  }
  virtual ~OutgoingNotificationMessageData() {}

  OutgoingNotificationData notification_data;
};

class MediatorThreadImpl
    : public MediatorThread,
      public sigslot::has_slots<>,
      public talk_base::MessageHandler,
      public talk_base::Thread {
 public:
  explicit MediatorThreadImpl(
      chrome_common_net::NetworkChangeNotifierThread*
          network_change_notifier_thread);
  virtual ~MediatorThreadImpl();

  // Start the thread.
  virtual void Start();
  virtual void Stop();
  virtual void Run();

  // These are called from outside threads, by the talk mediator object.
  // They add messages to a queue which we poll in this thread.
  void Login(const buzz::XmppClientSettings& settings);
  void Logout();
  void ListenForUpdates();
  void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);
  void SendNotification(const OutgoingNotificationData& data);
  void LogStanzas();

 private:
  // Called from within the thread on internal events.
  void ProcessMessages(int cms);
  void OnMessage(talk_base::Message* msg);
  void DoLogin(LoginData* login_data);
  void DoDisconnect();
  void DoSubscribeForUpdates(const SubscriptionData& subscription_data);
  void DoListenForUpdates();
  void DoSendNotification(
      const OutgoingNotificationMessageData& data);
  void DoStanzaLogging();
  void PumpAuxiliaryLoops();

  // These handle messages indicating an event happened in the outside world.
  void OnUpdateListenerMessage(
      const IncomingNotificationData& notification_data);
  void OnUpdateNotificationSent(bool success);
  void OnLoginFailureMessage(const notifier::LoginFailure& failure);
  void OnClientStateChangeMessage(LoginConnectionState state);
  void OnSubscriptionStateChange(bool success);
  void OnInputDebug(const char* msg, int length);
  void OnOutputDebug(const char* msg, int length);

  buzz::XmppClient* xmpp_client();

  chrome_common_net::NetworkChangeNotifierThread*
      network_change_notifier_thread_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_refptr<net::HostResolver> host_resolver_;

  // All buzz::XmppClients are owned by their parent.  The root parent is the
  // SingleLoginTask created by the notifier::Login object.  This in turn is
  // owned by the TaskPump.  They are destroyed either when processing is
  // complete or the pump shuts down.
  scoped_ptr<notifier::TaskPump> pump_;
  scoped_ptr<notifier::Login> login_;
  DISALLOW_COPY_AND_ASSIGN(MediatorThreadImpl);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
