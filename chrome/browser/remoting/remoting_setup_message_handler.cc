// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_setup_message_handler.h"

#include "base/scoped_ptr.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/remoting/remoting_setup_flow.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"

static const wchar_t kLoginIFrameXPath[] = L"//iframe[@id='login']";
static const wchar_t kDoneIframeXPath[] = L"//iframe[@id='done']";

void RemotingSetupMessageHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &RemotingSetupMessageHandler::HandleSubmitAuth));
}

void RemotingSetupMessageHandler::HandleSubmitAuth(const Value* value) {
  std::string json(dom_ui_util::GetJsonResponseFromFirstArgumentInList(value));
  std::string username, password, captcha;
  if (json.empty())
    return;

  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString("user", &username) ||
      !result->GetString("pass", &password) ||
      !result->GetString("captcha", &captcha)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  // Pass the information to the flow.
  if (flow_)
    flow_->OnUserSubmittedAuth(username, password, captcha);
}

void RemotingSetupMessageHandler::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(kLoginIFrameXPath,
                            L"showGaiaSuccessAndSettingUp();");
}

void RemotingSetupMessageHandler::ShowGaiaFailed() {
  // TODO(hclam): Implement this.
}

void RemotingSetupMessageHandler::ShowSetupDone() {
   std::wstring javascript = L"setMessage('You are all set!');";
   ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"showSetupDone");

   ExecuteJavascriptInIFrame(kDoneIframeXPath, L"onPageShown();");
}

void RemotingSetupMessageHandler::ExecuteJavascriptInIFrame(
    const std::wstring& iframe_xpath,
    const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}
