// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_unittest.h"
#include "net/base/net_log_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

CapturingNetLog::Entry MakeEventEntry(int t,
                                      NetLog::EventType event_type,
                                      NetLog::EventPhase event_phase) {
  return CapturingNetLog::Entry(event_type,
                                MakeTime(t),
                                NetLog::Source(),
                                event_phase,
                                NULL);
}

TEST(NetLogUtilTest, Basic) {
  CapturingNetLog::EntryList log;

  log.push_back(MakeEventEntry(1, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_BEGIN));
  log.push_back(
      MakeEventEntry(5, NetLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                     NetLog::PHASE_BEGIN));
  log.push_back(
       MakeEventEntry(8, NetLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                      NetLog::PHASE_END));

  log.push_back(MakeEventEntry(12, NetLog::TYPE_CANCELLED,
                               NetLog::PHASE_NONE));

  log.push_back(MakeEventEntry(131, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_END));

  EXPECT_EQ(
    "t=  1: +HOST_RESOLVER_IMPL                    [dt=130]\n"
    "t=  5:    HOST_RESOLVER_IMPL_OBSERVER_ONSTART [dt=  3]\n"
    "t= 12:    CANCELLED\n"
    "t=131: -HOST_RESOLVER_IMPL",
    NetLogUtil::PrettyPrintAsEventTree(log, 0));
}

TEST(NetLogUtilTest, Basic2) {
  CapturingNetLog::EntryList log;

  log.push_back(MakeEventEntry(1, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_BEGIN));

  // Attach a string parameter to a CANCELLED event.
  CapturingNetLog::Entry e =
      MakeEventEntry(12, NetLog::TYPE_CANCELLED, NetLog::PHASE_NONE);
  e.extra_parameters =
      new NetLogStringParameter("string_name", "string_value");
  log.push_back(e);

  log.push_back(MakeEventEntry(131, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_END));

  EXPECT_EQ(
      "t=  1: +HOST_RESOLVER_IMPL   [dt=130]\n"
      "t= 12:    CANCELLED\n"
      "{\n"
      "   \"string_name\": \"string_value\"\n"
      "}\n"
      "t=131: -HOST_RESOLVER_IMPL",
      NetLogUtil::PrettyPrintAsEventTree(log, 0));
}

TEST(NetLogUtilTest, UnmatchedOpen) {
  CapturingNetLog::EntryList log;

  log.push_back(MakeEventEntry(3, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_BEGIN));
  // Note that there is no matching call to PHASE_END for all of the following.
  log.push_back(
      MakeEventEntry(
          6, NetLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
          NetLog::PHASE_BEGIN));
  log.push_back(
      MakeEventEntry(7, NetLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                     NetLog::PHASE_BEGIN));
  log.push_back(
      MakeEventEntry(8, NetLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                     NetLog::PHASE_BEGIN));
  log.push_back(MakeEventEntry(10, NetLog::TYPE_CANCELLED,
                               NetLog::PHASE_NONE));
  log.push_back(MakeEventEntry(16, NetLog::TYPE_HOST_RESOLVER_IMPL,
                               NetLog::PHASE_END));

  EXPECT_EQ(
    "t= 3: +HOST_RESOLVER_IMPL                          [dt=13]\n"
    "t= 6:   +HOST_RESOLVER_IMPL_OBSERVER_ONSTART       [dt=10]\n"
    "t= 7:     +HOST_RESOLVER_IMPL_OBSERVER_ONSTART     [dt= 9]\n"
    "t= 8:       +HOST_RESOLVER_IMPL_OBSERVER_ONSTART   [dt= 8]\n"
    "t=10:          CANCELLED\n"
    "t=16: -HOST_RESOLVER_IMPL",
    NetLogUtil::PrettyPrintAsEventTree(log, 0));
}

TEST(NetLogUtilTest, DisplayOfTruncated) {
  CapturingNetLog::EntryList log;

  log.push_back(MakeEventEntry(0,
                               NetLog::TYPE_TCP_CONNECT,
                               NetLog::PHASE_BEGIN));
  for (size_t i = 1; i <= 3; ++i) {
    log.push_back(MakeEventEntry(i,
                                 NetLog::TYPE_CANCELLED,
                                 NetLog::PHASE_NONE));
  }
  log.push_back(MakeEventEntry(9,
                               NetLog::TYPE_TCP_CONNECT,
                               NetLog::PHASE_END));

  EXPECT_EQ(
    "t=0: +TCP_CONNECT   [dt=9]\n"
    "t=1:    CANCELLED\n"
    "t=2:    CANCELLED\n"
    "t=3:    CANCELLED\n"
    " ... Truncated 4 entries ...\n"
    "t=9: -TCP_CONNECT",
    NetLogUtil::PrettyPrintAsEventTree(log, 4));
}

}  // namespace
}  // namespace net
