// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SHELL_DEVTOOLS_CALLARGS_H_
#define TEST_SHELL_DEVTOOLS_CALLARGS_H_

#include "base/logging.h"

#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

class TestShellDevToolsCallArgs {
 public:
  TestShellDevToolsCallArgs(const WebKit::WebString& data)
      : data_(data) {
    ++calls_count_;
  }

  TestShellDevToolsCallArgs(const TestShellDevToolsCallArgs& args)
      : data_(args.data_) {
    ++calls_count_;
  }

  ~TestShellDevToolsCallArgs() {
    --calls_count_;
    DCHECK(calls_count_ >= 0);
  }

  static int calls_count() { return calls_count_; }

  WebKit::WebString data_;

 private:
  static int calls_count_;
};

#endif  // TEST_SHELL_DEVTOOLS_CALLARGS_H_
