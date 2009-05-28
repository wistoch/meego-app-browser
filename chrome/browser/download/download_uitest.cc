// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "build/build_config.h"
#if defined(OS_WIN)
#include <shlwapi.h>
#endif

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/automation/url_request_slow_download_job.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

#if defined(OS_WIN)
// Checks if the volume supports Alternate Data Streams. This is required for
// the Zone Identifier implementation.
bool VolumeSupportsADS(const std::wstring path) {
  wchar_t drive[MAX_PATH] = {0};
  wcscpy_s(drive, MAX_PATH, path.c_str());

  EXPECT_TRUE(PathStripToRootW(drive));

  DWORD fs_flags = 0;
  EXPECT_TRUE(GetVolumeInformationW(drive, NULL, 0, 0, NULL, &fs_flags, NULL,
                                    0));

  if (fs_flags & FILE_NAMED_STREAMS)
    return true;

  return false;
}

// Checks if the ZoneIdentifier is correctly set to "Internet" (3)
void CheckZoneIdentifier(const std::wstring full_path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

  std::wstring path = full_path + L":Zone.Identifier";
  HANDLE file = CreateFile(path.c_str(), GENERIC_READ, kShare, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);

  char buffer[100] = {0};
  DWORD read = 0;
  ASSERT_TRUE(ReadFile(file, buffer, 100, &read, NULL));
  CloseHandle(file);

  const char kIdentifier[] = "[ZoneTransfer]\nZoneId=3";
  ASSERT_EQ(arraysize(kIdentifier), read);

  ASSERT_EQ(0, strcmp(kIdentifier, buffer));
}
#endif  // defined(OS_WIN)

class DownloadTest : public UITest {
 protected:
  DownloadTest() : UITest() {}

  void CleanUpDownload(const FilePath& client_filename,
                       const FilePath& server_filename) {
    // Find the path on the client.
    FilePath file_on_client = download_prefix_.Append(client_filename);
    EXPECT_TRUE(file_util::PathExists(file_on_client));

    // Find the path on the server.
    FilePath file_on_server;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                                 &file_on_server));
    file_on_server = file_on_server.Append(server_filename);
    ASSERT_TRUE(file_util::PathExists(file_on_server));

    // Check that we downloaded the file correctly.
    EXPECT_TRUE(file_util::ContentsEqual(file_on_server,
                                         file_on_client));

#if defined(OS_WIN)
    // Check if the Zone Identifier is correctly set.
    if (VolumeSupportsADS(file_on_client.value()))
      CheckZoneIdentifier(file_on_client.value());
#endif

    // Delete the client copy of the file.
    EXPECT_TRUE(file_util::Delete(file_on_client, false));
  }

  void CleanUpDownload(const FilePath& file) {
    CleanUpDownload(file, file);
  }

  virtual void SetUp() {
    UITest::SetUp();
    download_prefix_ = FilePath::FromWStringHack(GetDownloadDirectory());
  }

 protected:
  void RunSizeTest(const GURL& url,
                   const std::wstring& expected_title_in_progress,
                   const std::wstring& expected_title_finished) {
    {
      EXPECT_EQ(1, GetTabCount());

      NavigateToURL(url);
      // Downloads appear in the shelf
      WaitUntilTabCount(1);
      // TODO(tc): check download status text

      // Complete sending the request.  We do this by loading a second URL in a
      // separate tab.
      scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
      EXPECT_TRUE(window->AppendTab(GURL(
          URLRequestSlowDownloadJob::kFinishDownloadUrl)));
      EXPECT_EQ(2, GetTabCount());
      // TODO(tc): check download status text

      // Make sure the download shelf is showing.
      scoped_refptr<TabProxy> dl_tab(window->GetTab(0));
      ASSERT_TRUE(dl_tab.get());
      EXPECT_TRUE(WaitForDownloadShelfVisible(dl_tab.get()));
    }

    FilePath filename;
    net::FileURLToFilePath(url, &filename);
    filename = filename.BaseName();
    FilePath download_path = download_prefix_.Append(filename);
    EXPECT_TRUE(file_util::PathExists(download_path));

    // Delete the file we just downloaded.
    for (int i = 0; i < 10; ++i) {
      if (file_util::Delete(download_path, false))
        break;
      PlatformThread::Sleep(action_max_timeout_ms() / 10);
    }
    EXPECT_FALSE(file_util::PathExists(download_path));
  }

  FilePath download_prefix_;
};

// Download a file with non-viewable content, verify that the
// download tab opened and the file exists.
TEST_F(DownloadTest, DownloadMimeType) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file.ToWStringHack()));
  // No new tabs created, downloads appear in the current tab's download shelf.
  WaitUntilTabCount(1);

  // Wait until the file is downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  CleanUpDownload(file);

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
TEST_F(DownloadTest, NoDownload) {
  FilePath file(FILE_PATH_LITERAL("download-test2.html"));
  FilePath file_path = download_prefix_.Append(file);

  if (file_util::PathExists(file_path))
    ASSERT_TRUE(file_util::Delete(file_path, false));

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file.ToWStringHack()));
  WaitUntilTabCount(1);

  // Wait to see if the file will be downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  EXPECT_FALSE(file_util::PathExists(file_path));
  if (file_util::PathExists(file_path))
    ASSERT_TRUE(file_util::Delete(file_path, false));

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_FALSE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
TEST_F(DownloadTest, ContentDisposition) {
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file.ToWStringHack()));
  WaitUntilTabCount(1);

  // Wait until the file is downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  CleanUpDownload(download_file, file);

  // Ensure the download shelf is visible on the current tab.
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// UnknownSize and KnownSize are tests which depend on
// URLRequestSlowDownloadJob to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish.  At that time, the download will finish.
TEST_F(DownloadTest, UnknownSize) {
  GURL url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  RunSizeTest(url, L"32.0 KB - " + filename.ToWStringHack(),
              L"100% - " + filename.ToWStringHack());
}

// http://b/1158253
TEST_F(DownloadTest, DISABLED_KnownSize) {
  GURL url(URLRequestSlowDownloadJob::kKnownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  RunSizeTest(url, L"71% - " + filename.ToWStringHack(),
              L"100% - " + filename.ToWStringHack());
}

}  // namespace
