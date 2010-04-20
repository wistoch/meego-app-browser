// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class listens for notifications from the talk service, and signals when
// they arrive.  It checks all incoming stanza's to see if they look like
// notifications, and filters out those which are not valid.
//
// The task is deleted automatically by the buzz::XmppClient. This occurs in the
// destructor of TaskRunner, which is a superclass of buzz::XmppClient.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_LISTEN_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_LISTEN_TASK_H_

#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/notifier/listener/notification_defines.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {
class XmlElement;
class Jid;
}

namespace browser_sync {

class ListenTask : public buzz::XmppTask {
 public:
  ListenTask(Task* parent, NotificationMethod notification_method);
  virtual ~ListenTask();

  // Overriden from buzz::XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

  // Signal callback upon receipt of a notification.
  // SignalUpdateAvailable(const NotificationData& data);
  sigslot::signal1<const NotificationData&> SignalUpdateAvailable;

 private:
  // Decide whether a notification should start a sync.  We only validate that
  // this notification came from our own Jid().
  bool IsValidNotification(const buzz::XmlElement* stanza);

  NotificationMethod notification_method_;

  DISALLOW_COPY_AND_ASSIGN(ListenTask);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_LISTEN_TASK_H_
