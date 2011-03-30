// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_handler_browsertest.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"

bool WebUIHandlerBrowserTest::RunJavascript(const std::string& js_test,
                                            bool is_test) {
  web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(), UTF8ToUTF16(js_test));

  if (is_test)
    return WaitForResult();
  else
    return true;
}

void WebUIHandlerBrowserTest::HandlePass(const ListValue* args) {
  test_succeeded_ = true;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void WebUIHandlerBrowserTest::HandleFail(const ListValue* args) {
  test_succeeded_ = false;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  std::string message;
  ASSERT_TRUE(args->GetString(0, &message));
  LOG(INFO) << message;
}

void WebUIHandlerBrowserTest::RegisterMessages() {
  web_ui_->RegisterMessageCallback("Pass",
      NewCallback(this, &WebUIHandlerBrowserTest::HandlePass));
  web_ui_->RegisterMessageCallback("Fail",
      NewCallback(this, &WebUIHandlerBrowserTest::HandleFail));
}

bool WebUIHandlerBrowserTest::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
