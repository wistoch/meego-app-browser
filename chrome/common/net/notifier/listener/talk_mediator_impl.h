// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the interface between talk code and the client code proper
// It will manage all aspects of the connection and call back into the client
// when it needs attention (for instance if updates are available for syncing).

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/notifier/listener/mediator_thread.h"
#include "chrome/common/net/notifier/listener/talk_mediator.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace chrome_common_net {
class NetworkChangeNotifierThread;
}  // namespace chrome_common_net

namespace notifier {

class TalkMediatorImpl
    : public TalkMediator,
      public sigslot::has_slots<> {
 public:
  TalkMediatorImpl(
      chrome_common_net::NetworkChangeNotifierThread*
          network_change_notifier_thread,
      bool invalidate_xmpp_auth_token);
  explicit TalkMediatorImpl(MediatorThread* thread);
  virtual ~TalkMediatorImpl();

  // Overriden from TalkMediator.

  virtual void SetDelegate(Delegate* delegate);

  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service);
  virtual bool Login();
  virtual bool Logout();

  virtual bool SendNotification(const OutgoingNotificationData& data);

  virtual void AddSubscribedServiceUrl(const std::string& service_url);

 private:
  struct TalkMediatorState {
    TalkMediatorState()
        : started(0), connected(0), initialized(0), logging_in(0),
          logged_in(0), subscribed(0) {
    }

    unsigned int started : 1;      // Background thread has started.
    unsigned int connected : 1;    // Connected to the mediator thread signal.
    unsigned int initialized : 1;  // Initialized with login information.
    unsigned int logging_in : 1;   // Logging in to the mediator's
                                   // authenticator.
    unsigned int logged_in : 1;    // Logged in the mediator's authenticator.
    unsigned int subscribed : 1;   // Subscribed to the xmpp receiving channel.
  };

  // Completes common initialization between the constructors.  Set should
  // connect to true if the talk mediator should connect to the controlled
  // mediator thread's SignalStateChange object.
  void TalkMediatorInitialization(bool should_connect);

  // Callbacks for the mediator thread.
  void MediatorThreadMessageHandler(MediatorThread::MediatorMessage message);
  void MediatorThreadNotificationHandler(
      const IncomingNotificationData& notification_data);

  // Responses to messages from the MediatorThread.
  void OnNotificationSent();
  void OnLogin();
  void OnLogout();
  void OnSubscriptionFailure();
  void OnSubscriptionSuccess();

  // Mutex for synchronizing event access. This class listens to events
  // from MediatorThread.  It can also be called by through the
  // TalkMediatorInteface.  All these access points are serialized by
  // this mutex.
  Lock mutex_;

  // Delegate.  May be NULL.
  Delegate* delegate_;

  // Internal state.
  TalkMediatorState state_;

  // Cached and verfied from the SetAuthToken method.
  buzz::XmppClientSettings xmpp_settings_;

  // The worker thread through which talk events are posted and received.
  scoped_ptr<MediatorThread> mediator_thread_;

  bool invalidate_xmpp_auth_token_;

  std::vector<std::string> subscribed_services_list_;

  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithBadInput);
  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithGoodInput);
  FRIEND_TEST(TalkMediatorImplTest, SendNotification);
  FRIEND_TEST(TalkMediatorImplTest, MediatorThreadCallbacks);
  DISALLOW_COPY_AND_ASSIGN(TalkMediatorImpl);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
