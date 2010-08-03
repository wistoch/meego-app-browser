// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_
#define CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/ie_event_sink.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test/test_with_web_server.h"
#include "chrome_frame/test/win_event_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_frame_test {

// Convenience enum for specifying whether a load occurred in IE or CF.
enum LoadedInRenderer {
  IN_IE = 0,
  IN_CF
};

// This mocks an IEEventListener, providing methods for expecting certain
// sequences of events.
class MockIEEventSink : public IEEventListener {
 public:
  MockIEEventSink() {
    CComObject<IEEventSink>::CreateInstance(&event_sink_);
    event_sink_->AddRef();
  }

  ~MockIEEventSink() {
    Detach();
    int reference_count = event_sink_->reference_count();
    DLOG_IF(ERROR, reference_count != 1)
        << "Event sink is still referenced externally: ref count = "
        << reference_count;
    event_sink_->Release();
  }

  // Override IEEventListener methods.
  MOCK_METHOD7(OnBeforeNavigate2, void (IDispatch* dispatch,  // NOLINT
                                        VARIANT* url,
                                        VARIANT* flags,
                                        VARIANT* target_frame_name,
                                        VARIANT* post_data,
                                        VARIANT* headers,
                                        VARIANT_BOOL* cancel));
  MOCK_METHOD2(OnNavigateComplete2, void (IDispatch* dispatch,  // NOLINT
                                          VARIANT* url));
  MOCK_METHOD5(OnNewWindow3, void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel,
                                   DWORD flags,
                                   BSTR url_context,
                                   BSTR url));
  MOCK_METHOD2(OnNewWindow2, void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel));
  MOCK_METHOD5(OnNavigateError, void (IDispatch* dispatch,  // NOLINT
                                      VARIANT* url,
                                      VARIANT* frame_name,
                                      VARIANT* status_code,
                                      VARIANT* cancel));
  MOCK_METHOD2(OnFileDownload, void (VARIANT_BOOL active_doc,  // NOLINT
                                     VARIANT_BOOL* cancel));
  MOCK_METHOD0(OnQuit, void ());  // NOLINT
  MOCK_METHOD1(OnLoadError, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD3(OnMessage, void (const wchar_t* message,  // NOLINT
                                const wchar_t* origin,
                                const wchar_t* source));
  MOCK_METHOD2(OnNewBrowserWindow, void (IDispatch* dispatch,  // NOLINT
                                         const wchar_t* url));

  // Convenience OnLoad method which is called once when a page is loaded with
  // |is_cf| set to whether the renderer is CF or not.
  MOCK_METHOD2(OnLoad, void (bool is_cf, const wchar_t* url));  // NOLINT

  // Attach |dispatch| to the event sink and begin listening to the source's
  // events.
  void Attach(IDispatch* dispatch) {
    event_sink_->set_listener(this);
    event_sink_->Attach(dispatch);
  }

  void Detach() {
    event_sink_->set_listener(NULL);
    event_sink_->Uninitialize();
  }

  // Expect a normal navigation to |url| to occur in CF or IE.
  void ExpectNavigation(bool is_cf, const std::wstring& url);

  // Same as above, but used when the new navigation is to a diffrent fragment
  // in the same page.
  void ExpectInPageNavigation(bool is_cf, const std::wstring& url);

  // Expect a navigation in a new window created by a window.open call to |url|.
  // |parent_cf| signifies whether the parent frame was loaded in CF, while
  // |new_window_cf| signifies whether to expect the new page to be loaded in
  // CF.
  void ExpectJavascriptWindowOpenNavigation(bool parent_cf, bool new_window_cf,
                                            const std::wstring& url);

  // Expect a new window to open. The new event sink will be attached to
  // |new_window_mock|.
  void ExpectNewWindow(MockIEEventSink* new_window_mock);

  // Expects any and all navigations.
  void ExpectAnyNavigations();

  IEEventSink* event_sink() { return event_sink_; }

 private:
  // Override IE's OnDocumentComplete to call our OnLoad, iff it is IE actually
  // rendering the page.
  virtual void OnDocumentComplete(IDispatch* dispatch, VARIANT* url) {
    if (!event_sink_->IsCFRendering())
      OnLoad(IN_IE, V_BSTR(url));
  }

  // Override CF's OnLoad to call our OnLoad.
  virtual void OnLoad(const wchar_t* url) {
    OnLoad(IN_CF, url);
  }

  // Helper method for expecting navigations. |before_cardinality| specifies
  // the cardinality for the BeforeNavigate expectation and
  // |complete_cardinality| specifies the cardinality for the NavigateComplete
  // expectation. Returns the set of expectations added.
  // Note: Prefer making a new Expect... method before making this public.
  testing::ExpectationSet ExpectNavigationCardinality(const std::wstring& url,
      testing::Cardinality before_cardinality,
      testing::Cardinality complete_cardinality);

  // It may be necessary to create this on the heap. Otherwise, if the
  // reference count is greater than zero, a debug assert will be triggered
  // in the destructor. This happens at least when IE crashes. In that case,
  // DispEventUnadvise and CoDisconnectObject are not sufficient to decrement
  // the reference count.
  // TODO(kkania): Investigate if the above is true.
  CComObject<IEEventSink>* event_sink_;
};

// Mocks a window observer so that tests can detect new windows.
class MockWindowObserver : public WindowObserver {
 public:
  // Override WindowObserver methods.
  MOCK_METHOD2(OnWindowDetected, void (HWND hwnd,  // NOLINT
                                       const std::string& caption));

  // Watch for all windows of the given class type.
  void WatchWindow(const wchar_t* window_class) {
    DCHECK(window_class);
    window_watcher_.AddObserver(this, WideToUTF8(window_class));
  }

 private:
  WindowWatchdog window_watcher_;
};

// This test fixture provides common methods needed for testing CF
// integration with IE. gMock is used to verify that IE is reporting correct
// navigational events and MockWebServer is used to verify that the correct
// requests are going out.
class MockIEEventSinkTest {
 public:
  MockIEEventSinkTest();

  ~MockIEEventSinkTest() {
    // Detach manually here so that it occurs before |last_resort_close_ie_|
    // is destroyed.
    ie_mock_.Detach();
  }

  // Launches IE as a COM server and sets |ie_mock_| as the event sink, then
  // navigates to the given url. Then the timed message loop is run until
  // |ie_mock_| receives OnQuit or the timeout is exceeded.
  void LaunchIEAndNavigate(const std::wstring& url);

  // Same as above but allows the timeout to be specified.
  void LaunchIENavigateAndLoop(const std::wstring& url, int timeout);

  // Returns the url for the test file given. |relative_path| should be
  // relative to the test data directory.
  std::wstring GetTestUrl(const std::wstring& relative_path);

  // Returns the absolute FilePath for the test file given. |relative_path|
  // should be relative to the test data directory.
  FilePath GetTestFilePath(const std::wstring& relative_path);

  // Returns the url for an html page just containing some text. Iff |use_cf|
  // is true, the chrome_frame meta tag will be included too.
  std::wstring GetSimplePageUrl() {
    return GetTestUrl(L"simple.html");
  }

  // Returns the url for an html page just containing one link to the simple
  // page mentioned above.
  std::wstring GetLinkPageUrl() {
    return GetTestUrl(L"link.html");
  }

  // Returns the url for an html page containing several anchors pointing
  // to different parts of the page. |index| specifies what fragment to
  // append to the url. If zero, no fragment is appended. The highest fragment
  // is #a4.
  std::wstring GetAnchorPageUrl(int index) {
    DCHECK_LT(index, 5);
    std::wstring base_name = L"anchor.html";
    if (index > 0)
      base_name += std::wstring(L"#a") + base::IntToString16(index);
    return GetTestUrl(base_name);
  }

 protected:
  CloseIeAtEndOfScope last_resort_close_ie_;
  chrome_frame_test::TimedMsgLoop loop_;
  testing::StrictMock<MockIEEventSink> ie_mock_;
  testing::StrictMock<MockWebServer> server_mock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIEEventSinkTest);
};

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_
