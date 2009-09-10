// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_

#include <time.h>

#include "talk/base/basictypes.h"

typedef uint64 time64;

#define kMicrosecsTo100ns   (static_cast<time64>(10))
#define kMillisecsTo100ns   (static_cast<time64>(10000))
#define kSecsTo100ns        (1000 * kMillisecsTo100ns)
#define kMinsTo100ns        (60 * kSecsTo100ns)
#define kHoursTo100ns       (60 * kMinsTo100ns)
#define kDaysTo100ns        (24 * kHoursTo100ns)
const time64 kMaxTime100ns = UINT64_C(9223372036854775807);

// Time difference in 100NS granularity between platform-dependent starting
// time and Jan 1, 1970.
#ifdef WIN32
// On Windows time64 is seconds since Jan 1, 1601.
#define kStart100NsTimeToEpoch (116444736000000000uI64) // Jan 1, 1970 in time64
#else
// On Unix time64 is seconds since Jan 1, 1970.
#define kStart100NsTimeToEpoch (0)                      // Jan 1, 1970 in time64
#endif

// Time difference in 100NS granularity between platform-dependent starting
// time and Jan 1, 1980.
#define kStart100NsTimeTo1980  \
    kStart100NsTimeToEpoch + UINT64_C(3155328000000000)

#define kTimeGranularity    (kDaysTo100ns)

namespace notifier {

// Get the current time represented in 100NS granularity
// Different platform might return the value since different starting time.
// Win32 platform returns the value since Jan 1, 1601.
time64 GetCurrent100NSTime();

// Get the current time represented in 100NS granularity since epoch
// (Jan 1, 1970).
time64 GetCurrent100NSTimeSinceEpoch();

// Convert from struct tm to time64.
time64 TmToTime64(const struct tm& tm);

// Convert from time64 to struct tm.
bool Time64ToTm(time64 t, struct tm* tm);

// Convert from UTC time to local time.
bool UtcTimeToLocalTime(struct tm* tm);

// Convert from local time to UTC time.
bool LocalTimeToUtcTime(struct tm* tm);

// Returns the local time as a string suitable for logging
// Note: This is *not* threadsafe, so only call it from the main thread.
char* GetLocalTimeAsString();

// Parses RFC 822 Date/Time format
//    5.  DATE AND TIME SPECIFICATION
//     5.1.  SYNTAX
//
//     date-time   =  [ day "," ] date time        ; dd mm yy
//                                                 ;  hh:mm:ss zzz
//     day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
//                 /  "Fri"  / "Sat" /  "Sun"
//
//     date        =  1*2DIGIT month 2DIGIT        ; day month year
//                                                 ;  e.g. 20 Jun 82
//
//     month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
//                 /  "May"  /  "Jun" /  "Jul"  /  "Aug"
//                 /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"
//
//     time        =  hour zone                    ; ANSI and Military
//
//     hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
//                                                 ; 00:00:00 - 23:59:59
//
//     zone        =  "UT"  / "GMT"                ; Universal Time
//                                                 ; North American : UT
//                 /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
//                 /  "CST" / "CDT"                ;  Central:  - 6/ - 5
//                 /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
//                 /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
//                 /  1ALPHA                       ; Military: Z = UT;
//                                                 ;  A:-1; (J not used)
//                                                 ;  M:-12; N:+1; Y:+12
//                 / ( ("+" / "-") 4DIGIT )        ; Local differential
//                                                 ;  hours+min. (HHMM)
// Return local time if ret_local_time == true, return UTC time otherwise
bool ParseRFC822DateTime(const char* str, struct tm* time, bool ret_local_time);

// Parse a string to time span.
//
// A TimeSpan value can be represented as
//    [d.]hh:mm:ss
//
//    d    = days (optional)
//    hh   = hours as measured on a 24-hour clock
//    mm   = minutes
//    ss   = seconds
bool ParseStringToTimeSpan(const char* str, time64* time_span);

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_
