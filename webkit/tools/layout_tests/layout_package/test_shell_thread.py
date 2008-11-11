# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Thread object for running the test shell and processing URLs from a
shared queue.

Each thread runs a separate instance of the test_shell binary and validates
the output.  When there are no more URLs to process in the shared queue, the
thread exits.
"""

import copy
import logging
import os
import Queue
import subprocess
import thread
import threading

import path_utils
import platform_utils
import test_failures

# The per-test timeout in milliseconds, if no --time-out-ms option was given to
# run_webkit_tests. This should correspond to the default timeout in
# test_shell.exe.
DEFAULT_TEST_TIMEOUT_MS = 10 * 1000

def ProcessOutput(proc, filename, test_uri, test_types, test_args):
  """Receives the output from a test_shell process, subjects it to a number
  of tests, and returns a list of failure types the test produced.

  Args:
    proc: an active test_shell process
    filename: path of the test file being run
    test_types: list of test types to subject the output to
    test_args: arguments to be passed to each test

  Returns: a list of failure objects for the test being processed
  """
  outlines = []
  failures = []
  crash_or_timeout = False

  # Some test args, such as the image hash, may be added or changed on a
  # test-by-test basis.
  local_test_args = copy.copy(test_args)

  line = proc.stdout.readline()
  while line.rstrip() != "#EOF":
    # Make sure we haven't crashed.
    if line == '' and proc.poll() is not None:
      failures.append(test_failures.FailureCrash())

      # This is hex code 0xc000001d, which is used for abrupt termination.
      # This happens if we hit ctrl+c from the prompt and we happen to
      # be waiting on the test_shell.
      if -1073741510 == proc.returncode:
        raise KeyboardInterrupt
      crash_or_timeout = True
      break

    # Don't include #URL lines in our output
    if line.startswith("#URL:"):
      url = line.rstrip()[5:]
      if url != test_uri:
        logging.fatal("Test got out of sync:\n|%s|\n|%s|" %
                      (url, test_uri))
        raise AssertionError("test out of sync")
    elif line.startswith("#MD5:"):
      local_test_args.hash = line.rstrip()[5:]
    elif line.startswith("#TEST_TIMED_OUT"):
      # Test timed out, but we still need to read until #EOF.
      crash_or_timeout = True
      failures.append(test_failures.FailureTimeout())
    else:
      outlines.append(line)
    line = proc.stdout.readline()

  # If we had any stderr output, append that. This is not ideal, but at least
  # it'll catch errors.
#  line = proc.stderr.readline()
#  while line.rstrip() != "#EOF":
#    # TODO(pamg): We suppress this stderr message temporarily so we can run
#    # the tests deterministically until someone has a chance to fix the
#    # underlying problem.
#    # See http://code.google.com/p/chromium/issues/detail?id=4285
#    if (line != '' and
#        not line.endswith('alias ISO-8859-8-I maps to ISO-8859-8-I already, '
#                          'but someone is trying to make it map to '
#                          'ISO-8859-8')):
#      outlines.append(line)
#    line = proc.stderr.readline()

  # Check the output and save the results.
  for test_type in test_types:
    new_failures = test_type.CompareOutput(filename, proc,
                                           ''.join(outlines),
                                           local_test_args)
    # Don't add any more failures if we already have a crash or timeout, so
    # we don't double-report those tests.
    if not crash_or_timeout:
      failures.extend(new_failures)

  return failures


def StartTestShell(binary, args):
  """Returns the process for a new test_shell started in layout-tests mode."""
  cmd = [binary, '--layout-tests'] + args
  # We'd really like to combine stderr into stdout here by setting stderr to
  # subprocess.STDOUT, but on Windows that's just dropping stderr output on
  # the floor, at least in Python 2.4.1.
  return subprocess.Popen(cmd,
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)


class SingleTestThread(threading.Thread):
  """Thread wrapper for running a single test file."""
  def __init__(self, test_shell_binary, shell_args, test_uri, filename,
               test_types, test_args):
    """
    Args:
      test_uri: full file:// or http:// URI of the test file to be run
      filename: absolute local path to the test file
      See TestShellThread for documentation of the remaining arguments.
    """

    threading.Thread.__init__(self)
    self._binary = test_shell_binary
    self._shell_args = shell_args
    self._test_uri = test_uri
    self._filename = filename
    self._test_types = test_types
    self._test_args = test_args
    self._single_test_failures = []

  def run(self):
    proc = StartTestShell(self._binary, self._shell_args + [self._test_uri])
    self._single_test_failures = ProcessOutput(proc,
                                               self._filename,
                                               self._test_uri,
                                               self._test_types,
                                               self._test_args)

  def GetFailures(self):
    return self._single_test_failures


class TestShellThread(threading.Thread):

  def __init__(self, filename_queue, test_shell_binary, test_types,
               test_args, shell_args, options):
    """Initialize all the local state for this test shell thread.

    Args:
      filename_queue: A thread safe Queue class that contains tuples of
                      (filename, uri) pairs.
      test_shell_binary: The path to test_shell.exe
      test_types: A list of TestType objects to run the test output against.
      test_args: A TestArguments object to pass to each TestType.
      shell_args: Any extra arguments to be passed to test_shell.exe.
      options: A property dictionary as produced by optparse. The command-line
               options should match those expected by run_webkit_tests; they
               are typically passed via the run_webkit_tests.TestRunner class.
    """
    threading.Thread.__init__(self)
    self._filename_queue = filename_queue
    self._test_shell_binary = test_shell_binary
    self._test_types = test_types
    self._test_args = test_args
    self._test_shell_proc = None
    self._shell_args = shell_args
    self._options = options
    self._failures = {}

    if self._options.run_singly:
      # When we're running one test per test_shell process, we can enforce
      # a hard timeout. test_shell uses a default of 10 seconds if no
      # time-out-ms is given, and the test_shell watchdog uses 2.5x the
      # test_shell's value.  We want to be larger than that.
      if not self._options.time_out_ms:
        self._options.time_out_ms = DEFAULT_TEST_TIMEOUT_MS
      self._time_out_sec = int(self._options.time_out_ms) * 3.0 / 1000.0
      logging.info("Setting Python per-test timeout to %s ms (%s sec)" %
                   (1000 * self._time_out_sec, self._time_out_sec))


  def GetFailures(self):
    """Returns a dictionary mapping test filename to a list of
    TestFailures."""
    return self._failures

  def run(self):
    """Main work entry point of the thread.  Basically we pull urls from the
    filename queue and run the tests until we run out of urls."""
    while True:
      try:
        filename, test_uri = self._filename_queue.get_nowait()
      except Queue.Empty:
        self._KillTestShell()
        logging.debug("queue empty, quitting test shell thread")
        return

      # we have a url, run tests
      if self._options.run_singly:
        failures = self._RunTestSingly(filename, test_uri)
      else:
        failures = self._RunTest(filename, test_uri)
      if failures:
        # Check and kill test shell if we need to
        if len([1 for f in failures if f.ShouldKillTestShell()]):
          self._KillTestShell()
        # print the error message(s)
        error_str = '\n'.join(['  ' + f.Message() for f in failures])
        logging.error("%s failed:\n%s" %
                      (path_utils.RelativeTestFilename(filename), error_str))
        # Group the errors for reporting
        self._failures[filename] = failures
      else:
        logging.debug(path_utils.RelativeTestFilename(filename) + " passed")


  def _RunTestSingly(self, filename, test_uri):
    """Run a test in a separate thread, enforcing a hard time limit.

    Since we can only detect the termination of a thread, not any internal
    state or progress, we can only run per-test timeouts when running test
    files singly.
    """
    worker = SingleTestThread(self._test_shell_binary,
                              self._shell_args,
                              test_uri,
                              filename,
                              self._test_types,
                              self._test_args)
    worker.start()
    worker.join(self._time_out_sec)
    if worker.isAlive():
      # If join() returned with the thread still running, the test_shell.exe is
      # completely hung and there's nothing more we can do with it.  We have
      # to kill all the test_shells to free it up. If we're running more than
      # one test_shell thread, we'll end up killing the other test_shells too,
      # introducing spurious crashes. We accept that tradeoff in order to
      # avoid losing the rest of this thread's results.
      logging.error('Test thread hung: killing all test_shells')
      # PlatformUtility() wants a base_dir, but it doesn't matter here.
      platform_util = platform_utils.PlatformUtility('')
      platform_util.KillAllTestShells()

    return worker.GetFailures()


  def _RunTest(self, filename, test_uri):
    """Run a single test file using a shared test_shell process.

    Args:
      filename: The absolute filename of the test
      test_uri: The URI version of the filename

    Return:
      A list of TestFailure objects describing the error.
    """
    self._EnsureTestShellIsRunning()

    # Ok, load the test URL...
    self._test_shell_proc.stdin.write(test_uri + "\n")
    self._test_shell_proc.stdin.flush()

    # ...and read the response
    return ProcessOutput(self._test_shell_proc, filename, test_uri,
                         self._test_types, self._test_args)


  def _EnsureTestShellIsRunning(self):
    """Start the shared test shell, if it's not running.  Not for use when
    running tests singly, since those each start a separate test shell in
    their own thread.
    """
    if (not self._test_shell_proc or
        self._test_shell_proc.poll() is not None):
      self._test_shell_proc = StartTestShell(self._test_shell_binary,
                                             self._shell_args)

  def _KillTestShell(self):
    """Kill the test shell process if it's running."""
    if self._test_shell_proc:
      self._test_shell_proc.stdin.close()
      self._test_shell_proc.stdout.close()
      if self._test_shell_proc.stderr:
        self._test_shell_proc.stderr.close()
      self._test_shell_proc = None

