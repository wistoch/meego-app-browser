// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_
#define CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_

#include <windows.h>
#include <string>

#include "chrome_frame/test/http_server.h"
#include "chrome_frame/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// Class that:
// 1) Starts the local webserver,
// 2) Supports launching browsers - Internet Explorer and Firefox with local url
// 3) Wait the webserver to finish. It is supposed the test webpage to shutdown
//    the server by navigating to "kill" page
// 4) Supports read the posted results from the test webpage to the "dump"
//    webserver directory
class ChromeFrameTestWithWebServer: public testing::Test {
 protected:
  enum BrowserKind { INVALID, IE, FIREFOX, OPERA, SAFARI, CHROME };

  bool LaunchBrowser(BrowserKind browser, const wchar_t* url);
  bool WaitForTestToComplete(int milliseconds);
  // Waits for the page to notify us of the window.onload event firing.
  // Note that the milliseconds value is only approximate.
  bool WaitForOnLoad(int milliseconds);
  bool ReadResultFile(const std::wstring& file_name, std::string* data);

  // Launches the specified browser and waits for the test to complete
  // (see WaitForTestToComplete).  Then checks that the outcome is OK.
  // This function uses EXPECT_TRUE and ASSERT_TRUE for all steps performed
  // hence no return value.
  void SimpleBrowserTest(BrowserKind browser, const wchar_t* page,
                         const wchar_t* result_file_to_check);

  // Same as SimpleBrowserTest but if the browser isn't installed (LaunchBrowser
  // fails), the function will print out a warning but not treat the test
  // as failed.
  // Currently this is how we run Opera tests.
  void OptionalBrowserTest(BrowserKind browser, const wchar_t* page,
                           const wchar_t* result_file_to_check);

  // Test if chrome frame correctly reports its version.
  void VersionTest(BrowserKind browser, const wchar_t* page,
                   const wchar_t* result_file_to_check);

  // Closes all browsers in preparation for a test and during cleanup.
  void CloseAllBrowsers();

  void CloseBrowser();

  // Ensures (well, at least tries to ensure) that the browser window has focus.
  bool BringBrowserToTop();

  // Returns true iff the specified result file contains 'expected result'.
  bool CheckResultFile(const std::wstring& file_name,
                       const std::string& expected_result);

  const FilePath& GetCFTestFilePath() {
    return test_file_path_;
  }

  virtual void SetUp();
  virtual void TearDown();

  // Important: kind means "sheep" in Icelandic. ?:-o
  const char* ToString(BrowserKind kind) {
    switch (kind) {
      case IE:
        return "IE";
      case FIREFOX:
        return "Firefox";
      case OPERA:
        return "Opera";
      case CHROME:
        return "Chrome";
      case SAFARI:
        return "Safari";
      default:
        NOTREACHED();
        break;
    }
    return "";
  }

  BrowserKind browser_;
  FilePath results_dir_;
  ScopedHandle browser_handle_;
  ChromeFrameHTTPServer server_;
  // The on-disk path to our html test files.
  FilePath test_file_path_;
};

// A helper class for doing some bookkeeping when using the
// SimpleWebServer class.
class SimpleWebServerTest {
 public:
  explicit SimpleWebServerTest(int port) : server_(port), port_(port) {
  }

  ~SimpleWebServerTest() {
    server_.DeleteAllResponses();
  }

  void PopulateStaticFileList(const wchar_t* pages[], int count,
                              const FilePath& directory) {
    for (int i = 0; i < count; ++i) {
      server_.AddResponse(new test_server::FileResponse(
          StringPrintf("/%ls", pages[i]).c_str(), directory.Append(pages[i])));
    }
  }

  std::wstring FormatHttpPath(const wchar_t* document_path) {
    return StringPrintf(L"http://localhost:%i/%ls", port_, document_path);
  }

  // Returns the last client request object.
  // Under normal circumstances this will be the request for /quit.
  const test_server::Request& last_request() const {
    const test_server::ConnectionList& connections = server_.connections();
    DCHECK(connections.size());
    const test_server::Connection* c = connections.back();
    return c->request();
  }

  bool FindRequest(const std::string& path,
                   const test_server::Request** request) {
    test_server::ConnectionList::const_iterator index;
    for (index = server_.connections().begin();
         index != server_.connections().end(); index++) {
      const test_server::Connection* connection = *index;
      if (!lstrcmpiA(connection->request().path().c_str(), path.c_str())) {
        if (request)
          *request = &connection->request();
        return true;
      }
    }
    return false;
  }

  // Counts the number of times a page was requested.
  // Optionally checks if the request method for each is equal to
  // |expected_method|.  If expected_method is NULL no such check is made.
  int GetRequestCountForPage(const wchar_t* page, const char* expected_method) {
    // Check how many requests we got for the cf page.
    test_server::ConnectionList::const_iterator it;
    int requests = 0;
    const test_server::ConnectionList& connections = server_.connections();
    for (it = connections.begin(); it != connections.end(); ++it) {
      const test_server::Connection* c = (*it);
      const test_server::Request& r = c->request();
      if (!r.path().empty() &&
          ASCIIToWide(r.path().substr(1)).compare(page) == 0) {
        if (expected_method) {
          EXPECT_EQ(expected_method, r.method());
        }
        requests++;
      }
    }
    return requests;
  }

  test_server::SimpleWebServer* web_server() {
    return &server_;
  }

 protected:
  test_server::SimpleWebServer server_;
  int port_;
};

#endif  // CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_
