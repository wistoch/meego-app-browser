// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class handles subscribing to talk notifications.  It does the getAll iq
// stanza which establishes the endpoint and directs future notifications to be
// pushed.

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_

#include <string>
#include <vector>

#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmpptask.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace notifier {
// TODO(akalin): Remove NOTIFICATION_LEGACY and remove/refactor relevant code
// in this class and any other class that uses notification_method.
class SubscribeTask : public buzz::XmppTask {
 public:
  SubscribeTask(TaskParent* parent,
                const std::vector<std::string>& subscribed_services_list);
  virtual ~SubscribeTask();

  // Overridden from XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon subscription success.
  sigslot::signal1<bool> SignalStatusUpdate;

 private:
  // Assembles an Xmpp stanza which can be sent to subscribe to notifications.
  static buzz::XmlElement* MakeSubscriptionMessage(
      const std::vector<std::string>& subscribed_services_list,
      const buzz::Jid& to_jid_bare, const std::string& task_id);

  std::vector<std::string> subscribed_services_list_;

  FRIEND_TEST(SubscribeTaskTest, MakeSubscriptionMessage);

  DISALLOW_COPY_AND_ASSIGN(SubscribeTask);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_SUBSCRIBE_TASK_H_
