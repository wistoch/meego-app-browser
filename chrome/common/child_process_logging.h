// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
#define CHROME_COMMON_CHILD_PROCESS_LOGGING_H_

#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

// Sets the URL that is logged if the child process crashes. Use GURL() to clear
// the URL.
void SetActiveURL(const GURL& url);

// Sets the Client ID that is used as GUID if a Chrome process crashes.
void SetClientId(const std::string& client_id);

// Sets the list of "active" extensions in this process. We overload "active" to
// mean different things depending on the process type:
// - browser: all enabled extensions
// - renderer: the unique set of extension ids from all content scripts
// - extension: the id of each extension running in this process (there can be
//   multiple because of process collapsing).
void SetActiveExtensions(const std::vector<std::string>& extension_ids);

// Simple wrapper class that sets the active URL in it's constructor and clears
// the active URL in the destructor.
class ScopedActiveURLSetter {
 public:
  explicit ScopedActiveURLSetter(const GURL& url)  {
    SetActiveURL(url);
  }

  ~ScopedActiveURLSetter()  {
    SetActiveURL(GURL());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveURLSetter);
};

}  // namespace child_process_logging

#if defined(OS_MACOSX) && __OBJC__

@class NSString;

typedef void (*SetCrashKeyValueFuncPtr)(NSString*, NSString*);
typedef void (*ClearCrashKeyValueFuncPtr)(NSString*);

namespace child_process_logging {
void SetCrashKeyFunctions(SetCrashKeyValueFuncPtr set_key_func,
                          ClearCrashKeyValueFuncPtr clear_key_func);
void SetActiveURLImpl(const GURL& url,
                      SetCrashKeyValueFuncPtr set_key_func,
                      ClearCrashKeyValueFuncPtr clear_key_func);

extern const int kMaxNumCrashURLChunks;
extern const int kMaxNumURLChunkValueLength;
extern const char *kUrlChunkFormatStr;
}  // namespace child_process_logging

#endif  // defined(OS_MACOSX) && __OBJC__

#endif  // CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
