// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "chrome/common/net/fake_network_change_notifier_thread.h"
#include "chrome/common/net/notifier/listener/mediator_thread_mock.h"
#include "chrome/common/net/notifier/listener/talk_mediator_impl.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "talk/xmpp/xmppengine.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

class TalkMediatorImplTest : public testing::Test {
 public:
  void HandleTalkMediatorEvent(
      const notifier::TalkMediatorEvent& event) {
    last_message_ = event.what_happened;
  }

 protected:
  TalkMediatorImplTest() {}
  ~TalkMediatorImplTest() {}

  virtual void SetUp() {
    last_message_ = -1;
  }

  virtual void TearDown() {
  }

  chrome_common_net::FakeNetworkChangeNotifierThread
      fake_network_change_notifier_thread_;
  int last_message_;
};

TEST_F(TalkMediatorImplTest, ConstructionOfTheClass) {
  // Constructing a single talk mediator enables SSL through the singleton.
  scoped_ptr<TalkMediatorImpl> talk1(
      new TalkMediatorImpl(&fake_network_change_notifier_thread_, false));
  talk1.reset(NULL);
}

TEST_F(TalkMediatorImplTest, SetAuthTokenWithBadInput) {
  scoped_ptr<TalkMediatorImpl> talk1(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_FALSE(talk1->SetAuthToken("@missinguser.com", "", "fake_service"));
  ASSERT_TRUE(talk1->state_.initialized == 0);

  scoped_ptr<TalkMediatorImpl> talk2(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_FALSE(talk2->SetAuthToken("", "1234567890", "fake_service"));
  ASSERT_TRUE(talk2->state_.initialized == 0);

  scoped_ptr<TalkMediatorImpl> talk3(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_FALSE(talk3->SetAuthToken("missingdomain", "abcde",  "fake_service"));
  ASSERT_TRUE(talk3->state_.initialized == 0);
}

TEST_F(TalkMediatorImplTest, SetAuthTokenWithGoodInput) {
  scoped_ptr<TalkMediatorImpl> talk1(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_TRUE(talk1->SetAuthToken("chromium@gmail.com", "token",
                                  "fake_service"));
  ASSERT_TRUE(talk1->state_.initialized == 1);

  scoped_ptr<TalkMediatorImpl> talk2(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_TRUE(talk2->SetAuthToken("chromium@mail.google.com", "token",
                                  "fake_service"));
  ASSERT_TRUE(talk2->state_.initialized == 1);

  scoped_ptr<TalkMediatorImpl> talk3(new TalkMediatorImpl(
      new MockMediatorThread()));
  ASSERT_TRUE(talk3->SetAuthToken("chromium@chromium.org", "token",
                                  "fake_service"));
  ASSERT_TRUE(talk3->state_.initialized == 1);
}

TEST_F(TalkMediatorImplTest, LoginWiring) {
  // The TalkMediatorImpl owns the mock.
  MockMediatorThread* mock = new MockMediatorThread();
  scoped_ptr<TalkMediatorImpl> talk1(new TalkMediatorImpl(mock));

  // Login checks states for initialization.
  ASSERT_TRUE(talk1->Login() == false);
  ASSERT_TRUE(mock->login_calls == 0);

  ASSERT_TRUE(talk1->SetAuthToken("chromium@gmail.com", "token",
                                  "fake_service") == true);
  ASSERT_TRUE(talk1->Login() == true);
  ASSERT_TRUE(mock->login_calls == 1);

  // Successive calls to login will fail.  One needs to create a new talk
  // mediator object.
  ASSERT_TRUE(talk1->Login() == false);
  ASSERT_TRUE(mock->login_calls == 1);

  ASSERT_TRUE(talk1->Logout() == true);
  ASSERT_TRUE(mock->logout_calls == 1);

  // Successive logout calls do nothing.
  ASSERT_TRUE(talk1->Logout() == false);
  ASSERT_TRUE(mock->logout_calls == 1);
}

TEST_F(TalkMediatorImplTest, SendNotification) {
  // The TalkMediatorImpl owns the mock.
  MockMediatorThread* mock = new MockMediatorThread();
  scoped_ptr<TalkMediatorImpl> talk1(new TalkMediatorImpl(mock));

  // Failure due to not being logged in.
  OutgoingNotificationData data;
  ASSERT_TRUE(talk1->SendNotification(data) == false);
  ASSERT_TRUE(mock->send_calls == 0);

  ASSERT_TRUE(talk1->SetAuthToken("chromium@gmail.com", "token",
                                  "fake_service") == true);
  ASSERT_TRUE(talk1->Login() == true);
  talk1->OnLogin();
  ASSERT_TRUE(mock->login_calls == 1);

  // Failure due to not being subscribed.
  ASSERT_TRUE(talk1->SendNotification(data) == false);
  ASSERT_TRUE(mock->send_calls == 0);

  // Fake subscription
  talk1->OnSubscriptionSuccess();
  ASSERT_TRUE(talk1->state_.subscribed == 1);
  ASSERT_TRUE(talk1->SendNotification(data) == true);
  ASSERT_TRUE(mock->send_calls == 1);
  ASSERT_TRUE(talk1->SendNotification(data) == true);
  ASSERT_TRUE(mock->send_calls == 2);

  ASSERT_TRUE(talk1->Logout() == true);
  ASSERT_TRUE(mock->logout_calls == 1);

  // Failure due to being logged out.
  ASSERT_TRUE(talk1->SendNotification(data) == false);
  ASSERT_TRUE(mock->send_calls == 2);
}

TEST_F(TalkMediatorImplTest, MediatorThreadCallbacks) {
  // The TalkMediatorImpl owns the mock.
  MockMediatorThread* mock = new MockMediatorThread();
  scoped_ptr<TalkMediatorImpl> talk1(new TalkMediatorImpl(mock));

  scoped_ptr<EventListenerHookup> callback(NewEventListenerHookup(
      talk1->channel(), this, &TalkMediatorImplTest::HandleTalkMediatorEvent));

  ASSERT_TRUE(talk1->SetAuthToken("chromium@gmail.com", "token",
                                  "fake_service") == true);
  ASSERT_TRUE(talk1->Login() == true);
  ASSERT_TRUE(mock->login_calls == 1);

  mock->ChangeState(MediatorThread::MSG_LOGGED_IN);
  ASSERT_TRUE(last_message_ == TalkMediatorEvent::LOGIN_SUCCEEDED);

  // The message triggers calls to listen and subscribe.
  ASSERT_TRUE(mock->listen_calls == 1);
  ASSERT_TRUE(mock->subscribe_calls == 1);
  ASSERT_TRUE(talk1->state_.subscribed == 0);

  mock->ChangeState(MediatorThread::MSG_SUBSCRIPTION_SUCCESS);
  ASSERT_TRUE(last_message_ == TalkMediatorEvent::SUBSCRIPTIONS_ON);
  ASSERT_TRUE(talk1->state_.subscribed == 1);

  // After subscription success is receieved, the talk mediator will allow
  // sending of notifications.
  OutgoingNotificationData outgoing_data;
  ASSERT_TRUE(talk1->SendNotification(outgoing_data) == true);
  ASSERT_TRUE(mock->send_calls == 1);

  IncomingNotificationData incoming_data;
  incoming_data.service_url = "service_url";
  incoming_data.service_specific_data = "service_data";
  mock->Notify(incoming_data);
  ASSERT_TRUE(last_message_ == TalkMediatorEvent::NOTIFICATION_RECEIVED);

  // A |TALKMEDIATOR_DESTROYED| message is received during tear down.
  talk1.reset();
  ASSERT_TRUE(last_message_ == TalkMediatorEvent::TALKMEDIATOR_DESTROYED);
}

}  // namespace notifier
