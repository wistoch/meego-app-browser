// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "net/test/test_server.h"

static void PrintUsage() {
  printf("run_testserver --doc-root=relpath [--http|--https|--ftp]\n");
  printf("(NOTE: relpath should be relative to the 'src' directory)\n");
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  MessageLoopForIO message_loop;

  // Process command line
  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->GetSwitchCount() == 0 ||
      command_line->HasSwitch("help")) {
    PrintUsage();
    return -1;
  }

  std::string protocol;
  int port;
  if (command_line->HasSwitch("https")) {
    protocol = "https";
    port = net::TestServerLauncher::kOKHTTPSPort;
  } else if (command_line->HasSwitch("ftp")) {
    protocol = "ftp";
    port = net::kFTPDefaultPort;
  } else {
    protocol = "http";
    port = net::kHTTPDefaultPort;
  }
  // TODO(phajdan.jr): refactor this.
  std::wstring doc_root =
      command_line->GetSwitchValuePath("doc-root").ToWStringHack();
  if (doc_root.empty()) {
    printf("Error: --doc-root must be specified\n");
    PrintUsage();
    return -1;
  }

  // Launch testserver
  scoped_refptr<net::BaseTestServer> test_server;
  if (protocol == "https") {
    test_server = net::HTTPSTestServer::CreateGoodServer(doc_root);
  } else if (protocol == "ftp") {
    test_server = net::FTPTestServer::CreateServer(doc_root);
  } else if (protocol == "http") {
    test_server = net::HTTPTestServer::CreateServer(doc_root);
  } else {
    NOTREACHED();
  }

  printf("testserver running at %s://%s:%d (type ctrl+c to exit)\n",
         protocol.c_str(),
         net::TestServerLauncher::kHostName,
         port);

  message_loop.Run();
  return 0;
}
