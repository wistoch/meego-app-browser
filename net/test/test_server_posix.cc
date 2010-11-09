// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <poll.h>

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"

namespace {

// Helper class used to detect and kill orphaned python test server processes.
// Checks if the command line of a process contains |path_string| (the path
// from which the test server was launched) and |port_string| (the port used by
// the test server), and if the parent pid of the process is 1 (indicating that
// it is an orphaned process).
class OrphanedTestServerFilter : public base::ProcessFilter {
 public:
  OrphanedTestServerFilter(
      const std::string& path_string, const std::string& port_string)
      : path_string_(path_string),
        port_string_(port_string) {}

  virtual bool Includes(const base::ProcessEntry& entry) const {
    if (entry.parent_pid() != 1)
      return false;
    bool found_path_string = false;
    bool found_port_string = false;
    for (std::vector<std::string>::const_iterator it =
         entry.cmd_line_args().begin();
         it != entry.cmd_line_args().end();
         ++it) {
      if (it->find(path_string_) != std::string::npos)
        found_path_string = true;
      if (it->find(port_string_) != std::string::npos)
        found_port_string = true;
    }
    return found_path_string && found_port_string;
  }

 private:
  std::string path_string_;
  std::string port_string_;
  DISALLOW_COPY_AND_ASSIGN(OrphanedTestServerFilter);
};

}  // namespace

namespace net {

bool TestServer::LaunchPython(const FilePath& testserver_path) {
  CommandLine python_command(FilePath(FILE_PATH_LITERAL("python")));
  python_command.AppendArgPath(testserver_path);
  if (!AddCommandLineArguments(&python_command))
    return false;

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    PLOG(ERROR) << "Could not create pipe.";
    return false;
  }

  // Save the read half. The write half is sent to the child.
  child_fd_ = pipefd[0];
  child_fd_closer_.reset(&child_fd_);
  file_util::ScopedFD write_closer(&pipefd[1]);
  base::file_handle_mapping_vector map_write_fd;
  map_write_fd.push_back(std::make_pair(pipefd[1], pipefd[1]));

  python_command.AppendSwitchASCII("startup-pipe",
                                   base::IntToString(pipefd[1]));

  // Try to kill any orphaned testserver processes that may be running.
  OrphanedTestServerFilter filter(testserver_path.value(),
                                  base::IntToString(host_port_pair_.port()));
  if (!base::KillProcesses(L"python", -1, &filter)) {
    LOG(WARNING) << "Failed to clean up older orphaned testserver instances.";
  }

  // Launch a new testserver process.
  if (!base::LaunchApp(python_command.argv(), map_write_fd, false,
                       &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << python_command.command_line_string()
               << " ...";
    return false;
  }

  return true;
}

bool TestServer::WaitToStart() {
  struct pollfd poll_fds[1];

  poll_fds[0].fd = child_fd_;
  poll_fds[0].events = POLLIN | POLLPRI;
  poll_fds[0].revents = 0;

  int rv = HANDLE_EINTR(poll(poll_fds, 1,
                             TestTimeouts::action_max_timeout_ms()));
  if (rv != 1) {
    LOG(ERROR) << "Failed to poll for the child file descriptor.";
    return false;
  }

  char buf[8];
  ssize_t n = HANDLE_EINTR(read(child_fd_, buf, sizeof(buf)));
  // We don't need the FD anymore.
  child_fd_closer_.reset(NULL);
  return n > 0;
}

bool TestServer::CheckCATrusted() {
  return true;
}

}  // namespace net
