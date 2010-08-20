// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/http_server.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";

ChromeFrameHTTPServer::ChromeFrameHTTPServer()
  : test_server_(net::TestServer::TYPE_HTTP, FilePath(kDocRoot)) {
}

void ChromeFrameHTTPServer::SetUp() {
  ASSERT_TRUE(test_server_.Start());

  // copy CFInstance.js into the test directory
  FilePath cf_source_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &cf_source_path);
  cf_source_path = cf_source_path.Append(FILE_PATH_LITERAL("chrome_frame"));

  file_util::CopyFile(cf_source_path.Append(FILE_PATH_LITERAL("CFInstance.js")),
      cf_source_path.Append(
          FILE_PATH_LITERAL("test")).Append(
              FILE_PATH_LITERAL("data")).Append(
                  FILE_PATH_LITERAL("CFInstance.js")));  // NOLINT

  file_util::CopyFile(cf_source_path.Append(FILE_PATH_LITERAL("CFInstall.js")),
      cf_source_path.Append(
          FILE_PATH_LITERAL("test")).Append(
              FILE_PATH_LITERAL("data")).Append(
                  FILE_PATH_LITERAL("CFInstall.js")));  // NOLINT
}

void ChromeFrameHTTPServer::TearDown() {
  test_server_.Stop();

  // clobber CFInstance.js
  FilePath cfi_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &cfi_path);
  cfi_path = cfi_path
      .Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("CFInstance.js"));

  file_util::Delete(cfi_path, false);

  cfi_path.empty();
  PathService::Get(base::DIR_SOURCE_ROOT, &cfi_path);
  cfi_path = cfi_path
      .Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("CFInstall.js"));

  file_util::Delete(cfi_path, false);
}

bool ChromeFrameHTTPServer::WaitToFinish(int milliseconds) {
  bool ret = test_server_.WaitToFinish(milliseconds);
  if (!ret) {
    LOG(ERROR) << "WaitToFinish failed with error:" << ::GetLastError();
  } else {
    ret = test_server_.Stop();
  }
  return ret;
}

// TODO(phajdan.jr): Change wchar_t* to std::string& and fix callers.
GURL ChromeFrameHTTPServer::Resolve(const wchar_t* relative_url) {
  return test_server_.GetURL(WideToUTF8(relative_url));
}

FilePath ChromeFrameHTTPServer::GetDataDir() {
  return test_server_.document_root();
}
