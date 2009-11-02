// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_UNITTEST_H_
#define NET_BASE_LOAD_LOG_UNITTEST_H_

#include <cstddef>
#include "net/base/load_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Create a timestamp with internal value of |t| milliseconds from the epoch.
inline base::TimeTicks MakeTime(int t) {
  base::TimeTicks ticks;  // initialized to 0.
  ticks += base::TimeDelta::FromMilliseconds(t);
  return ticks;
}

// Call gtest's EXPECT_* to verify that |log| contains the specified entry
// at index |i|.
inline void ExpectLogContains(const LoadLog* log,
                              size_t i,
                              base::TimeTicks expected_time,
                              LoadLog::EventType expected_event,
                              LoadLog::EventPhase expected_phase) {
  ASSERT_LT(i, log->events().size());
  EXPECT_TRUE(expected_time == log->events()[i].time);
  EXPECT_EQ(expected_event, log->events()[i].type);
  EXPECT_EQ(expected_phase, log->events()[i].phase);
}

// Same as above, but without an expectation for the timestamp.
inline void ExpectLogContains(const LoadLog* log,
                              size_t i,
                              LoadLog::EventType expected_event,
                              LoadLog::EventPhase expected_phase) {
  ASSERT_LT(i, log->events().size());
  EXPECT_EQ(expected_event, log->events()[i].type);
  EXPECT_EQ(expected_phase, log->events()[i].phase);
}

inline ::testing::AssertionResult LogContains(
    const LoadLog& log,
    int i,  // Negative indices are reverse indices.
    LoadLog::EventType expected_event,
    LoadLog::EventPhase expected_phase) {
  // Negative indices are reverse indices.
  size_t j = (i < 0) ? log.events().size() + i : i;
  if (j >= log.events().size())
    return ::testing::AssertionFailure() << j << " is out of bounds.";
  if (expected_event != log.events()[j].type) {
    return ::testing::AssertionFailure()
        << "Actual event: " << LoadLog::EventTypeToString(log.events()[j].type)
        << ". Expected event: " << LoadLog::EventTypeToString(expected_event)
        << ".";
  }
  if (expected_phase != log.events()[j].phase) {
    return ::testing::AssertionFailure()
        << "Actual phase: " << log.events()[j].phase
        << ". Expected phase: " << expected_phase << ".";
  }
  return ::testing::AssertionSuccess();
}

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_UNITTEST_H_
