// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/test_automation_provider.h"

#include "base/command_line.h"
#include "chrome/test/automation/automation_messages.h"

#include "chrome_frame/test/net/test_automation_resource_message_filter.h"

namespace {

// A special command line switch to just run the unit tests without CF in
// the picture.  Can be useful when the harness itself needs to be debugged.
const wchar_t kNoCfTestRun[] = L"no-cf-test-run";

bool CFTestsDisabled() {
  static bool switch_present = CommandLine::ForCurrentProcess()->
      HasSwitch(kNoCfTestRun);
  return switch_present;
}

}  // end namespace

TestAutomationProvider::TestAutomationProvider(
    Profile* profile,
    TestAutomationProviderDelegate* delegate)
    : AutomationProvider(profile), tab_handle_(-1), delegate_(delegate) {
  filter_ = new TestAutomationResourceMessageFilter(this);
  URLRequest::RegisterRequestInterceptor(this);
}

TestAutomationProvider::~TestAutomationProvider() {
  URLRequest::UnregisterRequestInterceptor(this);
}

void TestAutomationProvider::OnMessageReceived(const IPC::Message& msg) {
  if (filter_->OnMessageReceived(msg))
    return;  // Message handled by the filter.

  __super::OnMessageReceived(msg);
}

// IPC override to grab the tab handle.
bool TestAutomationProvider::Send(IPC::Message* msg) {
  if (msg->type() == AutomationMsg_TabLoaded::ID) {
    DCHECK(tab_handle_ == -1) << "Currently only support one tab";
    void* iter = NULL;
    CHECK(msg->ReadInt(&iter, &tab_handle_));
    DLOG(INFO) << "Got tab handle: " << tab_handle_;
    DCHECK(tab_handle_ != -1 && tab_handle_ != 0);
    delegate_->OnInitialTabLoaded();
  }

  return AutomationProvider::Send(msg);
}

URLRequestJob* TestAutomationProvider::MaybeIntercept(URLRequest* request) {
  if (CFTestsDisabled())
    return NULL;

  if (request->url().SchemeIs("http") || request->url().SchemeIs("https")) {
    // Only look at requests that don't have any user data.
    // ResourceDispatcherHost uses the user data for requests that it manages.
    // We don't want to mess with those.

    // We could also check if the current thread is our TestUrlRequest thread
    // and only intercept requests that belong to that thread.
    if (request->GetUserData(NULL) == NULL) {
      DCHECK(tab_handle_ != -1);
      URLRequestAutomationJob* job = new URLRequestAutomationJob(request,
          tab_handle_, filter_);
      return job;
    }
  }

  return NULL;
}

// static
TestAutomationProvider* TestAutomationProvider::NewAutomationProvider(
    Profile* p, const std::string& channel,
    TestAutomationProviderDelegate* delegate) {
  TestAutomationProvider* automation = new TestAutomationProvider(p, delegate);
  automation->ConnectToChannel(channel);
  automation->SetExpectedTabCount(1);
  return automation;
}
