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

#ifndef CHROME_COMMON_CHROME_PROCESS_FILTER_H__
#define CHROME_COMMON_CHROME_PROCESS_FILTER_H__

#include "base/process_util.h"

// Filter all chrome browser processes that run with the same user data
// directory.
class BrowserProcessFilter : public process_util::ProcessFilter {
 public:
  // Create the filter for the given user_data_dir.
  // If user_data_dir is an empty string, will use the PathService
  // user_data_dir (e.g. chrome::DIR_USER_DATA).
  explicit BrowserProcessFilter(const std::wstring user_data_dir);

  uint32 browser_process_id() const { return browser_process_id_; }

  virtual bool Includes(uint32 pid, uint32 parent_pid) const;

 private:
  std::wstring user_data_dir_;
  DWORD browser_process_id_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserProcessFilter);
};

#endif  // CHROME_COMMON_CHROME_PROCESS_FILTER_H__
