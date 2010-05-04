// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// These methods should post messages to a queue which a different thread will
// later come back and read from.

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/common/net/notifier/listener/notification_defines.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

class MediatorThread {
 public:
  enum MediatorMessage {
    MSG_LOGGED_IN,
    MSG_LOGGED_OUT,
    MSG_SUBSCRIPTION_SUCCESS,
    MSG_SUBSCRIPTION_FAILURE,
    MSG_NOTIFICATION_SENT
  };

  virtual ~MediatorThread() {}

  virtual void Login(const buzz::XmppClientSettings& settings) = 0;
  virtual void Logout() = 0;
  virtual void Start() = 0;
  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list) = 0;
  virtual void ListenForUpdates() = 0;
  virtual void SendNotification(const OutgoingNotificationData& data) = 0;

  // Connect to this for messages about talk events (except notifications).
  sigslot::signal1<MediatorMessage> SignalStateChange;
  // Connect to this for notifications
  sigslot::signal1<const IncomingNotificationData&> SignalNotificationReceived;

 protected:
  MediatorThread() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MediatorThread);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
