/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// The PerformanceTimer class measures elapsed time between Start and
// Stop (using the highest resolution timer available on the
// platform).

#include "core/cross/performance_timer.h"

#include "base/logging.h"

namespace o3d {

PerformanceTimer::PerformanceTimer(const char *name)
    : name_(name) {
  start_time_.lo = start_time_.hi = 0;
  accum_time_.lo = accum_time_.hi = 0;
}

void PerformanceTimer::Start() {
  start_time_ = UpTime();
}

void PerformanceTimer::Stop() {
  AbsoluteTime time = UpTime();
  AbsoluteTime deltaTime = SubAbsoluteFromAbsolute(time, start_time_);
  accum_time_ = AddAbsoluteToAbsolute(accum_time_, deltaTime);
}

double PerformanceTimer::GetElapsedTime() {
  Nanoseconds ns = AbsoluteToNanoseconds(accum_time_);
  uint64 ns64 = UnsignedWideToUInt64(ns);

  return static_cast<double>(ns64) * 0.000000001;
}

void PerformanceTimer::Print() {
  LOG(INFO) << name_.c_str() << " " << GetElapsedTime() << " seconds";
}

void PerformanceTimer::StopAndPrint() {
  Stop();
  Print();
}
}
