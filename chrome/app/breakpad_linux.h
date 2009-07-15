// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_LINUX_H_
#define CHROME_APP_BREAKPAD_LINUX_H_

extern void InitCrashReporter();

#if defined(GOOGLE_CHROME_BUILD)
static const unsigned kMaxActiveURLSize = 1024;
static const unsigned kGuidSize = 32;  // 128 bits = 32 chars in hex.

extern int UploadCrashDump(const char* filename,
                           const char* process_type,
                           unsigned process_type_length,
                           const char* crash_url,
                           unsigned crash_url_length,
                           const char* guid,
                           unsigned guid_length);
#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // CHROME_APP_BREAKPAD_LINUX_H_
