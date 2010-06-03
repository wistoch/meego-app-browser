// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_

#include <string>

#include "chrome/common/net/notifier/listener/notification_defines.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmpptask.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace notifier {

class SendUpdateTask : public buzz::XmppTask {
 public:
  SendUpdateTask(TaskParent* parent, const OutgoingNotificationData& data);
  virtual ~SendUpdateTask();

  // Overridden from buzz::XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon subscription success.
  sigslot::signal1<bool> SignalStatusUpdate;

 private:
  // Allocates and constructs an buzz::XmlElement containing the update stanza.
  static buzz::XmlElement* MakeUpdateMessage(
      const OutgoingNotificationData& notification_data,
      const buzz::Jid& to_jid_bare, const std::string& task_id);

  OutgoingNotificationData notification_data_;

  FRIEND_TEST(SendUpdateTaskTest, MakeUpdateMessage);

  DISALLOW_COPY_AND_ASSIGN(SendUpdateTask);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_SEND_UPDATE_TASK_H_
