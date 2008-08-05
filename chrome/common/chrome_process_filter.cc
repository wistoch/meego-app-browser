// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>

#include "chrome/common/chrome_process_filter.h"

#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

BrowserProcessFilter::BrowserProcessFilter(const std::wstring user_data_dir)
    : browser_process_id_(0),
      user_data_dir_(user_data_dir) {
  // Find the message window (if any) for the current user data directory,
  // and get its process ID.  We'll only count browser processes that either
  // have the same process ID or have that process ID as their parent.

  if (user_data_dir_.length() == 0)
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_);


  HWND message_window = FindWindowEx(HWND_MESSAGE, NULL,
                                     chrome::kMessageWindowClass,
                                     user_data_dir_.c_str());
  if (message_window)
    GetWindowThreadProcessId(message_window, &browser_process_id_);
}

bool BrowserProcessFilter::Includes(uint32 pid,
                                    uint32 parent_pid) const {
  return browser_process_id_ && (browser_process_id_ == pid ||
                                 browser_process_id_ == parent_pid);
}
