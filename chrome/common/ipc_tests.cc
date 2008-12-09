// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <stdio.h>
#include <iostream>
#include <string>

#include "chrome/common/ipc_tests.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/perftimer.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/test_suite.h"
#include "base/thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_message_utils.h"
#include "testing/multiprocess_func_list.h"

// Define to enable IPC performance testing instead of the regular unit tests
// #define PERFORMANCE_TEST

const wchar_t kTestClientChannel[] = L"T1";
const wchar_t kReflectorChannel[] = L"T2";
const wchar_t kFuzzerChannel[] = L"F3";

#ifndef PERFORMANCE_TEST

void IPCChannelTest::SetUp() {
  MultiProcessTest::SetUp();

  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_ = new MessageLoopForIO();
}

void IPCChannelTest::TearDown() {
  delete message_loop_;
  message_loop_ = NULL;

  MultiProcessTest::TearDown();
}

base::ProcessHandle IPCChannelTest::SpawnChild(ChildType child_type) {
  // kDebugChildren support.
  bool debug_on_start = CommandLine().HasSwitch(switches::kDebugChildren);

  switch (child_type) {
  case TEST_CLIENT:
    return MultiProcessTest::SpawnChild(L"RunTestClient", debug_on_start);
    break;
  case TEST_REFLECTOR:
    return MultiProcessTest::SpawnChild(L"RunReflector", debug_on_start);
    break;
  case FUZZER_SERVER:
    return MultiProcessTest::SpawnChild(L"RunFuzzServer", debug_on_start);
    break;
  default:
    return NULL;
    break;
  }
}

TEST_F(IPCChannelTest, BasicMessageTest) {
  int v1 = 10;
  std::string v2("foobar");
  std::wstring v3(L"hello world");

  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteString(v2));
  EXPECT_TRUE(m.WriteWString(v3));

  void* iter = NULL;

  int vi;
  std::string vs;
  std::wstring vw;

  EXPECT_TRUE(m.ReadInt(&iter, &vi));
  EXPECT_EQ(v1, vi);

  EXPECT_TRUE(m.ReadString(&iter, &vs));
  EXPECT_EQ(v2, vs);

  EXPECT_TRUE(m.ReadWString(&iter, &vw));
  EXPECT_EQ(v3, vw);

  // should fail
  EXPECT_FALSE(m.ReadInt(&iter, &vi));
  EXPECT_FALSE(m.ReadString(&iter, &vs));
  EXPECT_FALSE(m.ReadWString(&iter, &vw));
}

static void Send(IPC::Message::Sender* sender, const char* text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(message_index++);
  message->WriteString(std::string(text));

  // make sure we can handle large messages
  char junk[50000];
  junk[sizeof(junk)-1] = 0;
  message->WriteString(std::string(junk));

  // DEBUG: printf("[%u] sending message [%s]\n", GetCurrentProcessId(), text);
  sender->Send(message);
}

class MyChannelListener : public IPC::Channel::Listener {
 public:
  virtual void OnMessageReceived(const IPC::Message& message) {
    IPC::MessageIterator iter(message);

    iter.NextInt();
    const std::string data = iter.NextString();
    if (--messages_left_ == 0) {
      MessageLoop::current()->Quit();
    } else {
      Send(sender_, "Foo");
    }
  }

  virtual void OnChannelError() {
    // There is a race when closing the channel so the last message may be lost.
    EXPECT_LE(messages_left_, 1);
    MessageLoop::current()->Quit();
  }

  void Init(IPC::Message::Sender* s) {
    sender_ = s;
    messages_left_ = 50;
  }

 private:
  IPC::Message::Sender* sender_;
  int messages_left_;
};
static MyChannelListener channel_listener;

TEST_F(IPCChannelTest, ChannelTest) {
  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                    &channel_listener);
  chan.Connect();

  channel_listener.Init(&chan);

  base::ProcessHandle process_handle = SpawnChild(TEST_CLIENT);
  ASSERT_TRUE(process_handle);

  Send(&chan, "hello from parent");

  // run message loop
  MessageLoop::current()->Run();

  // cleanup child process
  EXPECT_TRUE(base::WaitForSingleProcess(process_handle, 5000));
}

// TODO(playmobil): Implement
#if defined(OS_WIN)
TEST_F(IPCChannelTest, ChannelProxyTest) {
  // The thread needs to out-live the ChannelProxy.
  base::Thread thread("ChannelProxyTestServer");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread.StartWithOptions(options);
  {
    // setup IPC channel proxy
    IPC::ChannelProxy chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                           &channel_listener, NULL, thread.message_loop());

    channel_listener.Init(&chan);

    HANDLE process_handle = SpawnChild(TEST_CLIENT);
    ASSERT_TRUE(process_handle);

    Send(&chan, "hello from parent");

    // run message loop
    MessageLoop::current()->Run();

    // cleanup child process
    WaitForSingleObject(process_handle, 5000);
    CloseHandle(process_handle);
  }
  thread.Stop();
}
#endif  // defined(OS_WIN)

MULTIPROCESS_TEST_MAIN(RunTestClient) {
  MessageLoopForIO main_message_loop;

  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_CLIENT,
                    &channel_listener);
  chan.Connect();
  channel_listener.Init(&chan);
  Send(&chan, "hello from child");
  // run message loop
  MessageLoop::current()->Run();
  // return true;
  return NULL;
}
#endif  // !PERFORMANCE_TEST

#ifdef PERFORMANCE_TEST

//-----------------------------------------------------------------------------
// Manually performance test
//
//    This test times the roundtrip IPC message cycle. It is enabled with a
//    special preprocessor define to enable it instead of the standard IPC
//    unit tests. This works around some funny termination conditions in the
//    regular unit tests.
//
//    This test is not automated. To test, you will want to vary the message
//    count and message size in TEST to get the numbers you want.
//
//    FIXME(brettw): Automate this test and have it run by default.

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public IPC::Channel::Listener {
 public:
  explicit ChannelReflectorListener(IPC::Channel *channel) :
    channel_(channel),
    count_messages_(0),
    latency_messages_(0) {
    std::cout << "Reflector up" << std::endl;
  }

  ~ChannelReflectorListener() {
    std::cout << "Client Messages: " << count_messages_ << std::endl;
    std::cout << "Client Latency: " << latency_messages_ << std::endl;
  }

  virtual void OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    IPC::MessageIterator iter(message);
    int time = iter.NextInt();
    int msgid = iter.NextInt();
    std::string payload = iter.NextString();
    latency_messages_ += GetTickCount() - time;

    // cout << "reflector msg received: " << msgid << endl;
    if (payload == "quit")
      MessageLoop::current()->Quit();

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt(GetTickCount());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
  }
 private:
  IPC::Channel *channel_;
  int count_messages_;
  int latency_messages_;
};

class ChannelPerfListener : public IPC::Channel::Listener {
 public:
  ChannelPerfListener(IPC::Channel* channel, int msg_count, int msg_size) :
       count_down_(msg_count),
       channel_(channel),
       count_messages_(0),
       latency_messages_(0) {
    payload_.resize(msg_size);
    for (int i = 0; i < static_cast<int>(payload_.size()); i++)
      payload_[i] = 'a';
    std::cout << "perflistener up" << std::endl;
  }

  ~ChannelPerfListener() {
    std::cout << "Server Messages: " << count_messages_ << std::endl;
    std::cout << "Server Latency: " << latency_messages_ << std::endl;
  }

  virtual void OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    // decode the string so this gets counted in the total time
    IPC::MessageIterator iter(message);
    int time = iter.NextInt();
    int msgid = iter.NextInt();
    std::string cur = iter.NextString();
    latency_messages_ += GetTickCount() - time;

    // cout << "perflistener got message" << endl;

    count_down_--;
    if (count_down_ == 0) {
      IPC::Message* msg = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
      msg->WriteInt(GetTickCount());
      msg->WriteInt(count_down_);
      msg->WriteString("quit");
      channel_->Send(msg);
      SetTimer(NULL, 1, 250, (TIMERPROC) PostQuitMessage);
      return;
    }

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt(GetTickCount());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    channel_->Send(msg);
  }

 private:
  int count_down_;
  std::string payload_;
  IPC::Channel *channel_;
  int count_messages_;
  int latency_messages_;
};

TEST_F(IPCChannelTest, Performance) {
  // setup IPC channel
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_SERVER, NULL);
  ChannelPerfListener perf_listener(&chan, 10000, 100000);
  chan.set_listener(&perf_listener);
  chan.Connect();

  HANDLE process = SpawnChild(TEST_REFLECTOR);
  ASSERT_TRUE(process);

  Sleep(1000);

  PerfTimeLogger logger("IPC_Perf");

  // this initial message will kick-start the ping-pong of messages
  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(GetTickCount());
  message->WriteInt(-1);
  message->WriteString("Hello");
  chan.Send(message);

  // run message loop
  MessageLoop::current()->Run();

  // cleanup child process
  WaitForSingleObject(process, 5000);
  CloseHandle(process);
}

// This message loop bounces all messages back to the sender
MULTIPROCESS_TEST_MAIN(RunReflector) {
  MessageLoopForIO main_message_loop;
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_CLIENT, NULL);
  ChannelReflectorListener channel_reflector_listener(&chan);
  chan.set_listener(&channel_reflector_listener);
  chan.Connect();

  MessageLoop::current()->Run();
  return true;
}

#endif  // PERFORMANCE_TEST

#if defined(OS_WIN)
// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void IPCTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

// Disable crash dialogs so that it doesn't gum up the buildbot
static void SuppressErrorDialogs() {
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
}
#endif  // defined(OS_WIN)

int main(int argc, char** argv) {
  base::ScopedNSAutoreleasePool scoped_pool;
  base::EnableTerminationOnHeapCorruption();

#if defined(OS_WIN)
  // suppress standard crash dialogs and such unless a debugger is present.
  if (!IsDebuggerPresent()) {
    SuppressErrorDialogs();
    logging::SetLogAssertHandler(IPCTestAssertHandler);
  }
#endif  // defined(OS_WIN)

  int retval = TestSuite(argc, argv).Run();

#ifdef PERFORMANCE_TEST
  if (!InitPerfLog("ipc_perf_child.log"))
    return 1;
#endif
  return retval;
}
