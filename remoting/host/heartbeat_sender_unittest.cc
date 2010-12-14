// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/ref_counted.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/test_key_pair.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

using testing::_;
using testing::DeleteArg;
using testing::DoAll;
using testing::NotNull;
using testing::Return;

namespace remoting {

namespace {
const char kHostId[] = "0";
const char kTestJid[] = "user@gmail.com/chromoting123";
const int64 kTestTime = 123123123;
}  // namespace

class MockJingleClient : public JingleClient {
 public:
  explicit MockJingleClient(JingleThread* thread) : JingleClient(thread) { }
  MOCK_METHOD0(CreateIqRequest, IqRequest*());
};

class MockIqRequest : public IqRequest {
 public:
  explicit MockIqRequest(JingleClient* jingle_client)
      : IqRequest(jingle_client) {
  }
  MOCK_METHOD3(SendIq, void(const std::string& type,
                            const std::string& addressee,
                            XmlElement* iq_body));
};

class HeartbeatSenderTest : public testing::Test {
 protected:
  class TestConfigUpdater :
      public base::RefCountedThreadSafe<TestConfigUpdater> {
   public:
    void DoUpdate(scoped_refptr<InMemoryHostConfig> target) {
      target->SetString(kHostIdConfigPath, kHostId);
      target->SetString(kPrivateKeyConfigPath, kTestHostKeyPair);
    }
  };

  virtual void SetUp() {
    config_ = new InMemoryHostConfig();
    scoped_refptr<TestConfigUpdater> config_updater(new TestConfigUpdater());
    config_->Update(
        NewRunnableMethod(config_updater.get(), &TestConfigUpdater::DoUpdate,
                          config_));

    jingle_thread_.message_loop_ = &message_loop_;

    jingle_client_ = new MockJingleClient(&jingle_thread_);
    jingle_client_->full_jid_ = kTestJid;
  }

  JingleThread jingle_thread_;
  scoped_refptr<MockJingleClient> jingle_client_;
  MessageLoop message_loop_;
  scoped_refptr<InMemoryHostConfig> config_;
};

// Call Start() followed by Stop(), and makes sure an Iq stanza is
// being send.
TEST_F(HeartbeatSenderTest, DoSendStanza) {
  // |iq_request| is freed by HeartbeatSender.
  MockIqRequest* iq_request = new MockIqRequest(jingle_client_);

  scoped_refptr<HeartbeatSender> heartbeat_sender(new HeartbeatSender());
  ASSERT_TRUE(heartbeat_sender->Init(config_, jingle_client_));

  EXPECT_CALL(*jingle_client_, CreateIqRequest())
      .WillOnce(Return(iq_request));

  EXPECT_CALL(*iq_request, SendIq(buzz::STR_SET, kChromotingBotJid, NotNull()))
      .WillOnce(DoAll(DeleteArg<2>(), Return()));

  heartbeat_sender->Start();
  message_loop_.RunAllPending();

  heartbeat_sender->Stop();
  message_loop_.RunAllPending();
}

// Validate format of the heartbeat stanza.
TEST_F(HeartbeatSenderTest, CreateHeartbeatMessage) {
  scoped_refptr<HeartbeatSender> heartbeat_sender(new HeartbeatSender());
  ASSERT_TRUE(heartbeat_sender->Init(config_, jingle_client_));

  int64 start_time = static_cast<int64>(base::Time::Now().ToDoubleT());

  scoped_ptr<XmlElement> stanza(
      heartbeat_sender->CreateHeartbeatMessage());
  ASSERT_TRUE(stanza.get() != NULL);

  EXPECT_TRUE(QName(kChromotingXmlNamespace, "heartbeat") ==
              stanza->Name());
  EXPECT_EQ(std::string(kHostId),
            stanza->Attr(QName(kChromotingXmlNamespace, "hostid")));

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = stanza->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != NULL);
  EXPECT_TRUE(stanza->NextNamed(signature_tag) == NULL);

  std::string time_str =
      signature->Attr(QName(kChromotingXmlNamespace, "time"));
  int64 time;
  EXPECT_TRUE(base::StringToInt64(time_str, &time));
  int64 now = static_cast<int64>(base::Time::Now().ToDoubleT());
  EXPECT_LE(start_time, time);
  EXPECT_GE(now, time);

  HostKeyPair key_pair;
  key_pair.LoadFromString(kTestHostKeyPair);
  std::string expected_signature =
      key_pair.GetSignature(std::string(kTestJid) + ' ' + time_str);
  EXPECT_EQ(expected_signature, signature->BodyText());
}

// Verify that ProcessResponse parses set-interval result.
TEST_F(HeartbeatSenderTest, ProcessResponse) {
  XmlElement* response = new XmlElement(QName("", "iq"));
  response->AddAttr(QName("", "type"), "result");

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);

  XmlElement* set_interval = new XmlElement(
      QName(kChromotingXmlNamespace, "set-interval"));
  result->AddElement(set_interval);

  const int kTestInterval = 123;
  set_interval->AddText(base::IntToString(kTestInterval));

  scoped_refptr<HeartbeatSender> heartbeat_sender(new HeartbeatSender());
  heartbeat_sender->ProcessResponse(response);

  EXPECT_EQ(kTestInterval * 1000, heartbeat_sender->interval_ms_);
}

}  // namespace remoting
