// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_ui_test.h"

// TODO(jvoung) see what includes we really need.
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

namespace {

  // base url specified in nacl_test

const FilePath::CharType kSrpcHwHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_hw.html");

const FilePath::CharType kSrpcBasicHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_basic.html");

const FilePath::CharType kSrpcSockAddrHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_sockaddr.html");

const FilePath::CharType kSrpcShmHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_shm.html");

const FilePath::CharType kSrpcPluginHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_plugin.html");

const FilePath::CharType kSrpcNrdXferHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_nrd_xfer.html");

const FilePath::CharType kServerHtmlFileName[] =
    FILE_PATH_LITERAL("server_test.html");

const FilePath::CharType kNpapiHwHtmlFileName[] =
    FILE_PATH_LITERAL("npapi_hw.html");
}  // anonymous namespace

NaClUITest::NaClUITest() : NaClTest() {
  // NaClTest has all we need.
}

NaClUITest::~NaClUITest() {}

TEST_F(NaClUITest, ServerTest) {
  FilePath test_file(kServerHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, SrpcHelloWorld) {
  FilePath test_file(kSrpcHwHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, SrpcBasicTest) {
  FilePath test_file(kSrpcBasicHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, SrpcSockAddrTest) {
  FilePath test_file(kSrpcSockAddrHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, SrpcShmTest) {
  FilePath test_file(kSrpcShmHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, FLAKY_SrpcPluginTest) {
  FilePath test_file(kSrpcPluginHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, SrpcNrdXferTest) {
  FilePath test_file(kSrpcNrdXferHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}

TEST_F(NaClUITest, NpapiHwTest) {
  FilePath test_file(kNpapiHwHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}
