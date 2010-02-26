// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_AUTOMATION_CLIENT_MOCK_H_
#define CHROME_FRAME_AUTOMATION_CLIENT_MOCK_H_

#include <windows.h>
#include <string>

#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/proxy_factory_mock.h"
#include "gmock/gmock.h"

// ChromeFrameAutomationClient [CFAC] tests.
struct MockCFDelegate : public ChromeFrameDelegateImpl {
  MOCK_CONST_METHOD0(GetWindow, WindowType());
  MOCK_METHOD1(GetBounds, void(RECT* bounds));
  MOCK_METHOD0(GetDocumentUrl, std::string());
  MOCK_METHOD2(ExecuteScript, bool(const std::string& script,
                                   std::string* result));
  MOCK_METHOD0(OnAutomationServerReady, void());
  MOCK_METHOD2(OnAutomationServerLaunchFailed, void(
      AutomationLaunchResult reason, const std::string& server_version));
  // This remains in interface since we call it if Navigate()
  // returns immediate error.
  MOCK_METHOD2(OnLoadFailed, void(int error_code, const std::string& url));

  // Do not mock this method. :) Use it as message demuxer and dispatcher
  // to the following methods (which we mock)
  // MOCK_METHOD1(OnMessageReceived, void(const IPC::Message&));

  MOCK_METHOD2(OnNavigationStateChanged, void(int tab_handle, int flags));
  MOCK_METHOD2(OnUpdateTargetUrl, void(int tab_handle,
      const std::wstring& new_target_url));
  MOCK_METHOD2(OnAcceleratorPressed, void(int tab_handle,
      const MSG& accel_message));
  MOCK_METHOD2(OnTabbedOut, void(int tab_handle, bool reverse));
  MOCK_METHOD3(OnOpenURL, void(int tab_handle, const GURL& url,
      int open_disposition));
  MOCK_METHOD2(OnDidNavigate, void(int tab_handle,
      const IPC::NavigationInfo& navigation_info));
  MOCK_METHOD3(OnNavigationFailed, void(int tab_handle, int error_code,
      const GURL& gurl));
  MOCK_METHOD2(OnLoad, void(int tab_handle, const GURL& url));
  MOCK_METHOD4(OnMessageFromChromeFrame, void(int tab_handle,
      const std::string& message,
      const std::string& origin,
      const std::string& target));
  MOCK_METHOD4(OnHandleContextMenu, void(int tab_handle, HANDLE menu_handle,
      int align_flags, const IPC::ContextMenuParams& params));
  MOCK_METHOD3(OnRequestStart, void(int tab_handle, int request_id,
      const IPC::AutomationURLRequest& request));
  MOCK_METHOD3(OnRequestRead, void(int tab_handle, int request_id,
      int bytes_to_read));
  MOCK_METHOD3(OnRequestEnd, void(int tab_handle, int request_id,
      const URLRequestStatus& status));
  MOCK_METHOD3(OnSetCookieAsync, void(int tab_handle, const GURL& url,
      const std::string& cookie));

  // Use for sending network responses
  void SetRequestDelegate(PluginUrlRequestDelegate* request_delegate) {
    request_delegate_ = request_delegate;
  }

  void ReplyStarted(int request_id, const char* headers) {
    request_delegate_->OnResponseStarted(request_id, "text/html", headers,
        0, base::Time::Now(), EmptyString(), EmptyString(), 0);
  }

  void ReplyData(int request_id, const std::string* data) {
    request_delegate_->OnReadComplete(request_id, data->c_str(), data->size());
  }

  void Reply(const URLRequestStatus& status, int request_id) {
    request_delegate_->OnResponseEnd(request_id, status);
  }

  void Reply404(int request_id) {
    ReplyStarted(request_id, "HTTP/1.1 404\r\n\r\n");
    Reply(URLRequestStatus(), request_id);
  }

  PluginUrlRequestDelegate* request_delegate_;
};

class MockAutomationProxy : public ChromeFrameAutomationProxy {
 public:
  MOCK_METHOD1(Send, bool(IPC::Message*));
  MOCK_METHOD3(SendAsAsync, void(IPC::SyncMessage* msg, void* callback,
                                 void* key));
  MOCK_METHOD1(CancelAsync, void(void* key));
  MOCK_METHOD1(CreateTabProxy, scoped_refptr<TabProxy>(int handle));
  MOCK_METHOD0(server_version, std::string(void));
  MOCK_METHOD1(SendProxyConfig, void(const std::string&));
  MOCK_METHOD1(SetEnableExtensionAutomation, void(bool enable));

  ~MockAutomationProxy() {}
};

struct MockAutomationMessageSender : public AutomationMessageSender {
  MOCK_METHOD1(Send, bool(IPC::Message*));
  MOCK_METHOD3(SendWithTimeout, bool(IPC::Message* , int , bool*));

  void ForwardTo(MockAutomationProxy *p) {
    proxy_ = p;
    ON_CALL(*this, Send(testing::_))
        .WillByDefault(testing::Invoke(proxy_, &MockAutomationProxy::Send));
  }

  MockAutomationProxy* proxy_;
};

// [CFAC] -- uses a ProxyFactory for creation of ChromeFrameAutomationProxy
// -- uses ChromeFrameAutomationProxy
// -- uses TabProxy obtained from ChromeFrameAutomationProxy
// -- uses ChromeFrameDelegate as outgoing interface
//
// We mock ProxyFactory to return mock object (MockAutomationProxy) implementing
// ChromeFrameAutomationProxy interface.
// Since CFAC uses TabProxy for few calls and TabProxy is not easy mockable,
// we create 'real' TabProxy but with fake AutomationSender (the one responsible
// for sending messages over channel).
// Additionally we have mock implementation ChromeFrameDelagate interface -
// MockCFDelegate.

// Test fixture, saves typing all of it's members.
class CFACMockTest : public testing::Test {
 public:
  MockProxyFactory factory_;
  MockCFDelegate   cfd_;
  chrome_frame_test::TimedMsgLoop loop_;
  MockAutomationProxy proxy_;
  scoped_ptr<AutomationHandleTracker> tracker_;
  MockAutomationMessageSender dummy_sender_;
  scoped_refptr<TabProxy> tab_;
  // the victim of all tests
  scoped_refptr<ChromeFrameAutomationClient> client_;

  std::wstring profile_;
  int timeout_;
  void* id_;  // Automation server id we are going to return
  int tab_handle_;   // Tab handle. Any non-zero value is Ok.

  inline ChromeFrameAutomationProxy* get_proxy() {
    return static_cast<ChromeFrameAutomationProxy*>(&proxy_);
  }

  inline void CreateTab() {
    ASSERT_EQ(NULL, tab_.get());
    tab_ = new TabProxy(&dummy_sender_, tracker_.get(), tab_handle_);
  }

  // Easy methods to set expectations.
  void SetAutomationServerOk();
  void Set_CFD_LaunchFailed(AutomationLaunchResult result);

 protected:
  CFACMockTest() : tracker_(NULL), timeout_(500),
      profile_(L"Adam.N.Epilinter") {
    id_ = reinterpret_cast<void*>(5);
    tab_handle_ = 3;
  }

  virtual void SetUp() {
    dummy_sender_.ForwardTo(&proxy_);
    tracker_.reset(new AutomationHandleTracker());

    client_ = new ChromeFrameAutomationClient;
    client_->set_proxy_factory(&factory_);
  }
};


#endif  // CHROME_FRAME_AUTOMATION_CLIENT_MOCK_H_

