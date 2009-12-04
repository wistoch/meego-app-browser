#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run layout tests using the test_shell.

This is a port of the existing webkit test script run-webkit-tests.

The TestRunner class runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list TestFailure
objects to the TestRunner.  The TestRunner then aggregates the TestFailures to
create a final report.

This script reads several files, if they exist in the test_lists subdirectory
next to this script itself.  Each should contain a list of paths to individual
tests or entire subdirectories of tests, relative to the outermost test
directory.  Entire lines starting with '//' (comments) will be ignored.

For details of the files' contents and purposes, see test_lists/README.
"""

import errno
import glob
import logging
import math
import optparse
import os
import Queue
import random
import re
import shutil
import subprocess
import sys
import time
import traceback

from layout_package import apache_http_server
from layout_package import test_expectations
from layout_package import http_server
from layout_package import json_results_generator
from layout_package import path_utils
from layout_package import test_failures
from layout_package import test_shell_thread
from layout_package import test_files
from layout_package import websocket_server
from test_types import fuzzy_image_diff
from test_types import image_diff
from test_types import test_type_base
from test_types import text_diff

TestExpectationsFile = test_expectations.TestExpectationsFile

class TestInfo:
  """Groups information about a test for easy passing of data."""
  def __init__(self, filename, timeout):
    """Generates the URI and stores the filename and timeout for this test.
    Args:
      filename: Full path to the test.
      timeout: Timeout for running the test in TestShell.
      """
    self.filename = filename
    self.uri = path_utils.FilenameToUri(filename)
    self.timeout = timeout
    expected_hash_file = path_utils.ExpectedFilename(filename, '.checksum')
    try:
      self.image_hash = open(expected_hash_file, "r").read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      self.image_hash = None


class ResultSummary(object):
  """A class for partitioning the test results we get into buckets.

  This class is basically a glorified struct and it's private to this file
  so we don't bother with any information hiding."""
  def __init__(self, expectations, test_files):
    self.total = len(test_files)
    self.expectations = expectations
    self.tests_by_expectation = {}
    self.tests_by_timeline = {}
    self.results = {}
    self.unexpected_results = {}
    self.failures = {}
    self.tests_by_expectation[test_expectations.SKIP] = set()
    for expectation in TestExpectationsFile.EXPECTATIONS.values():
      self.tests_by_expectation[expectation] = set()
    for timeline in TestExpectationsFile.TIMELINES.values():
      self.tests_by_timeline[timeline] = expectations.GetTestsWithTimeline(
          timeline)

  def Add(self, test, failures, result):
    """Add a result into the appropriate bin.

    Args:
      test: test file name
      failures: list of failure objects from test execution
      result: result of test (PASS, IMAGE, etc.)."""

    self.tests_by_expectation[result].add(test)
    self.results[test] = result
    if len(failures):
      self.failures[test] = failures
    if not self.expectations.MatchesAnExpectedResult(test, result):
      self.unexpected_results[test] = result


class TestRunner:
  """A class for managing running a series of tests on a series of test
  files."""

  HTTP_SUBDIR = os.sep.join(['', 'http', ''])
  WEBSOCKET_SUBDIR = os.sep.join(['', 'websocket', ''])

  # The per-test timeout in milliseconds, if no --time-out-ms option was given
  # to run_webkit_tests. This should correspond to the default timeout in
  # test_shell.exe.
  DEFAULT_TEST_TIMEOUT_MS = 6 * 1000

  NUM_RETRY_ON_UNEXPECTED_FAILURE = 1

  def __init__(self, options):
    """Initialize test runner data structures.

    Args:
      options: a dictionary of command line options
    """
    self._options = options

    if options.use_apache:
      self._http_server = apache_http_server.LayoutTestApacheHttpd(
          options.results_directory)
    else:
      self._http_server = http_server.Lighttpd(options.results_directory)

    self._shardable_directories = ['chrome', 'LayoutTests', 'pending', 'fast',
        'svg']

    self._websocket_server = websocket_server.PyWebSocket(
        options.results_directory)
    # disable wss server. need to install pyOpenSSL on buildbots.
    # self._websocket_secure_server = websocket_server.PyWebSocket(
    #        options.results_directory, use_tls=True, port=9323)

    # a list of TestType objects
    self._test_types = []

    # a set of test files, and the same tests as a list
    self._test_files = set()
    self._test_files_list = None
    self._file_dir = path_utils.GetAbsolutePath(os.path.dirname(sys.argv[0]))
    self._result_queue = Queue.Queue()

  def __del__(self):
    logging.info("flushing stdout")
    sys.stdout.flush()
    logging.info("flushing stderr")
    sys.stderr.flush()
    logging.info("stopping http server")
    # Stop the http server.
    self._http_server.Stop()
    # Stop the Web Socket / Web Socket Secure servers.
    self._websocket_server.Stop()
    # self._websocket_secure_server.Stop()

  def GatherFilePaths(self, paths):
    """Find all the files to test.

    Args:
      paths: a list of globs to use instead of the defaults."""
    self._test_files = test_files.GatherTestFiles(paths)
    logging.info('Found: %d tests' % len(self._test_files))

  def ParseExpectations(self, platform, is_debug_mode):
    """Parse the expectations from the test_list files and return a data
    structure holding them. Throws an error if the test_list files have invalid
    syntax.
    """
    if self._options.lint_test_files:
      test_files = None
    else:
      test_files = self._test_files

    try:
      self._expectations = test_expectations.TestExpectations(test_files,
          self._file_dir, platform, is_debug_mode,
          self._options.lint_test_files)
      return self._expectations
    except Exception, err:
      if self._options.lint_test_files:
        print str(err)
      else:
        raise err

  def PrepareListsAndPrintOutput(self):
    """Create appropriate subsets of test lists and returns a ResultSummary
    object. Also prints expected test counts."""

    # Remove skipped - both fixable and ignored - files from the
    # top-level list of files to test.
    num_all_test_files = len(self._test_files)
    skipped = ()
    if num_all_test_files > 1 and not self._options.force:
      skipped = self._expectations.GetTestsWithResultType(
                     test_expectations.SKIP)
      self._test_files -= skipped

    # Create a sorted list of test files so the subset chunk, if used, contains
    # alphabetically consecutive tests.
    self._test_files_list = list(self._test_files)
    if self._options.randomize_order:
      random.shuffle(self._test_files_list)
    else:
      self._test_files_list.sort(self.TestFilesSort)

    # If the user specifies they just want to run a subset of the tests,
    # just grab a subset of the non-skipped tests.
    if self._options.run_chunk or self._options.run_part:
      chunk_value = self._options.run_chunk or self._options.run_part
      test_files = self._test_files_list
      try:
        (chunk_num, chunk_len) = chunk_value.split(":")
        chunk_num = int(chunk_num)
        assert(chunk_num >= 0)
        test_size = int(chunk_len)
        assert(test_size > 0)
      except:
        logging.critical("invalid chunk '%s'" % chunk_value)
        sys.exit(1)

      # Get the number of tests
      num_tests = len(test_files)

      # Get the start offset of the slice.
      if self._options.run_chunk:
        chunk_len = test_size
        # In this case chunk_num can be really large. We need to make the
        # slave fit in the current number of tests.
        slice_start = (chunk_num * chunk_len) % num_tests
      else:
        # Validate the data.
        assert(test_size <= num_tests)
        assert(chunk_num <= test_size)

        # To count the chunk_len, and make sure we don't skip some tests, we
        # round to the next value that fits exacly all the parts.
        rounded_tests = num_tests
        if rounded_tests % test_size != 0:
          rounded_tests = num_tests + test_size - (num_tests % test_size)

        chunk_len = rounded_tests / test_size
        slice_start = chunk_len * (chunk_num - 1)
        # It does not mind if we go over test_size.

      # Get the end offset of the slice.
      slice_end = min(num_tests, slice_start + chunk_len)

      files = test_files[slice_start:slice_end]

      tests_run_msg = 'Run: %d tests (chunk slice [%d:%d] of %d)' % (
          (slice_end - slice_start), slice_start, slice_end, num_tests)
      logging.info(tests_run_msg)

      # If we reached the end and we don't have enough tests, we run some
      # from the beginning.
      if self._options.run_chunk and (slice_end - slice_start < chunk_len):
        extra = 1 + chunk_len - (slice_end - slice_start)
        extra_msg = '   last chunk is partial, appending [0:%d]' % extra
        logging.info(extra_msg)
        tests_run_msg += "\n" + extra_msg
        files.extend(test_files[0:extra])
      tests_run_filename = os.path.join(self._options.results_directory,
                                        "tests_run.txt")
      tests_run_file = open(tests_run_filename, "w")
      tests_run_file.write(tests_run_msg + "\n")
      tests_run_file.close()

      len_skip_chunk = int(len(files) * len(skipped) /
                           float(len(self._test_files)))
      skip_chunk_list = list(skipped)[0:len_skip_chunk]
      skip_chunk = set(skip_chunk_list)

      # Update expectations so that the stats are calculated correctly.
      # We need to pass a list that includes the right # of skipped files
      # to ParseExpectations so that ResultSummary() will get the correct
      # stats. So, we add in the subset of skipped files, and then subtract
      # them back out.
      self._test_files_list = files + skip_chunk_list
      self._test_files = set(self._test_files_list)

      self._expectations = self.ParseExpectations(
          path_utils.PlatformName(), options.target == 'Debug')

      self._test_files = set(files)
      self._test_files_list = files
    else:
      skip_chunk = skipped
      logging.info('Run: %d tests' % len(self._test_files))

    result_summary = ResultSummary(self._expectations,
                                   self._test_files | skip_chunk)
    self._PrintExpectedResultsOfType(result_summary, test_expectations.PASS,
        "passes")
    self._PrintExpectedResultsOfType(result_summary, test_expectations.FAIL,
        "failures")
    self._PrintExpectedResultsOfType(result_summary, test_expectations.FLAKY,
        "flaky")
    self._PrintExpectedResultsOfType(result_summary, test_expectations.SKIP,
        "skipped")

    if self._options.force:
      logging.info('Running all tests, including skips (--force)')
    else:
      # Note that we don't actually run the skipped tests (they were
      # subtracted out of self._test_files, above), but we stub out the
      # results here so the statistics can remain accurate.
      for test in skip_chunk:
        result_summary.Add(test, [], test_expectations.SKIP)

    return result_summary

  def AddTestType(self, test_type):
    """Add a TestType to the TestRunner."""
    self._test_types.append(test_type)

  # We sort the tests so that tests using the http server will run first.  We
  # are seeing some flakiness, maybe related to apache getting swapped out,
  # slow, or stuck after not serving requests for a while.
  def TestFilesSort(self, x, y):
    """Sort with http tests always first."""
    x_is_http = x.find(self.HTTP_SUBDIR) >= 0
    y_is_http = y.find(self.HTTP_SUBDIR) >= 0
    if x_is_http != y_is_http:
      return cmp(y_is_http, x_is_http)
    return cmp(x, y)

  def _GetDirForTestFile(self, test_file):
    """Returns the highest-level directory by which to shard the given test
    file."""
    # TODO(ojan): See if we can grab the lowest level directory. That will
    # provide better parallelization. We should at least be able to do so
    # for some directories (e.g. LayoutTests/dom).
    index = test_file.rfind(os.sep + 'LayoutTests' + os.sep)
    if index is -1:
      index = test_file.rfind(os.sep + 'chrome' + os.sep)

    test_file = test_file[index + 1:]
    test_file_parts = test_file.split(os.sep, 1)
    directory = test_file_parts[0]
    test_file = test_file_parts[1]

    return_value = directory
    while directory in self._shardable_directories:
      test_file_parts = test_file.split(os.sep, 1)
      directory = test_file_parts[0]
      return_value = os.path.join(return_value, directory)
      test_file = test_file_parts[1]

    return return_value

  def _GetTestInfoForFile(self, test_file):
    """Returns the appropriate TestInfo object for the file. Mostly this is used
    for looking up the timeout value (in ms) to use for the given test."""
    if self._expectations.HasModifier(test_file, test_expectations.SLOW):
      return TestInfo(test_file, self._options.slow_time_out_ms)
    return TestInfo(test_file, self._options.time_out_ms)

  def _GetTestFileQueue(self, test_files):
    """Create the thread safe queue of lists of (test filenames, test URIs)
    tuples. Each TestShellThread pulls a list from this queue and runs those
    tests in order before grabbing the next available list.

    Shard the lists by directory. This helps ensure that tests that depend
    on each other (aka bad tests!) continue to run together as most
    cross-tests dependencies tend to occur within the same directory.

    Return:
      The Queue of lists of TestInfo objects.
    """

    if self._options.experimental_fully_parallel:
      filename_queue = Queue.Queue()
      for test_file in test_files:
       filename_queue.put(('.', [self._GetTestInfoForFile(test_file)]))
      return filename_queue

    tests_by_dir = {}
    for test_file in test_files:
      directory = self._GetDirForTestFile(test_file)
      if directory not in tests_by_dir:
        tests_by_dir[directory] = []
      tests_by_dir[directory].append(self._GetTestInfoForFile(test_file))

    # Sort by the number of tests in the dir so that the ones with the most
    # tests get run first in order to maximize parallelization. Number of tests
    # is a good enough, but not perfect, approximation of how long that set of
    # tests will take to run. We can't just use a PriorityQueue until we move
    # to Python 2.6.
    test_lists = []
    http_tests = None
    for directory in tests_by_dir:
      test_list = tests_by_dir[directory]
      # Keep the tests in alphabetical order.
      # TODO: Remove once tests are fixed so they can be run in any order.
      test_list.reverse()
      test_list_tuple = (directory, test_list)
      if directory == 'LayoutTests' + os.sep + 'http':
        http_tests = test_list_tuple
      else:
        test_lists.append(test_list_tuple)
    test_lists.sort(lambda a, b: cmp(len(b[1]), len(a[1])))

    # Put the http tests first. There are only a couple hundred of them, but
    # each http test takes a very long time to run, so sorting by the number
    # of tests doesn't accurately capture how long they take to run.
    if http_tests:
      test_lists.insert(0, http_tests)

    filename_queue = Queue.Queue()
    for item in test_lists:
      filename_queue.put(item)
    return filename_queue

  def _GetTestShellArgs(self, index):
    """Returns the tuple of arguments for tests and for test_shell."""
    shell_args = []
    test_args = test_type_base.TestArguments()
    if not self._options.no_pixel_tests:
      png_path = os.path.join(self._options.results_directory,
                              "png_result%s.png" % index)
      shell_args.append("--pixel-tests=" + png_path)
      test_args.png_path = png_path

    test_args.new_baseline = self._options.new_baseline
    test_args.show_sources = self._options.sources

    if self._options.startup_dialog:
      shell_args.append('--testshell-startup-dialog')

    if self._options.gp_fault_error_box:
      shell_args.append('--gp-fault-error-box')

    return (test_args, shell_args)

  def _ContainWebSocketTest(self, test_files):
    if not test_files:
      return False
    for test_file in test_files:
      if test_file.find(self.WEBSOCKET_SUBDIR) >= 0:
        return True
    return False

  def _InstantiateTestShellThreads(self, test_shell_binary, test_files):
    """Instantitates and starts the TestShellThread(s).

    Return:
      The list of threads.
    """
    test_shell_command = [test_shell_binary]

    if self._options.wrapper:
      # This split() isn't really what we want -- it incorrectly will
      # split quoted strings within the wrapper argument -- but in
      # practice it shouldn't come up and the --help output warns
      # about it anyway.
      test_shell_command = self._options.wrapper.split() + test_shell_command

    filename_queue = self._GetTestFileQueue(test_files)

    # Instantiate TestShellThreads and start them.
    threads = []
    for i in xrange(int(self._options.num_test_shells)):
      # Create separate TestTypes instances for each thread.
      test_types = []
      for t in self._test_types:
        test_types.append(t(self._options.platform,
                            self._options.results_directory))

      test_args, shell_args = self._GetTestShellArgs(i)
      thread = test_shell_thread.TestShellThread(filename_queue,
                                                 self._result_queue,
                                                 test_shell_command,
                                                 test_types,
                                                 test_args,
                                                 shell_args,
                                                 self._options)
      thread.start()
      threads.append(thread)

    return threads

  def _StopLayoutTestHelper(self, proc):
    """Stop the layout test helper and closes it down."""
    if proc:
      logging.info("Stopping layout test helper")
      proc.stdin.write("x\n")
      proc.stdin.close()
      proc.wait()

  def _RunTests(self, test_shell_binary, file_list, result_summary):
    """Runs the tests in the file_list.

    Return: A tuple (failures, thread_timings, test_timings,
        individual_test_timings)
        failures is a map from test to list of failure types
        thread_timings is a list of dicts with the total runtime of each thread
          with 'name', 'num_tests', 'total_time' properties
        test_timings is a list of timings for each sharded subdirectory of the
          form [time, directory_name, num_tests]
        individual_test_timings is a list of run times for each test in the form
          {filename:filename, test_run_time:test_run_time}
        result_summary: summary object to populate with the results
    """
    threads = self._InstantiateTestShellThreads(test_shell_binary, file_list)

    # Wait for the threads to finish and collect test failures.
    failures = {}
    test_timings = {}
    individual_test_timings = []
    thread_timings = []
    try:
      for thread in threads:
        while thread.isAlive():
          # Let it timeout occasionally so it can notice a KeyboardInterrupt
          # Actually, the timeout doesn't really matter: apparently it
          # suffices to not use an indefinite blocking join for it to
          # be interruptible by KeyboardInterrupt.
          thread.join(1.0)
          self._UpdateSummary(result_summary)
        thread_timings.append({ 'name': thread.getName(),
                                'num_tests': thread.GetNumTests(),
                                'total_time': thread.GetTotalTime()});
        test_timings.update(thread.GetDirectoryTimingStats())
        individual_test_timings.extend(thread.GetIndividualTestStats())
    except KeyboardInterrupt:
      for thread in threads:
        thread.Cancel()
      self._StopLayoutTestHelper(layout_test_helper_proc)
      raise
    for thread in threads:
      # Check whether a TestShellThread died before normal completion.
      exception_info = thread.GetExceptionInfo()
      if exception_info is not None:
        # Re-raise the thread's exception here to make it clear that
        # testing was aborted. Otherwise, the tests that did not run
        # would be assumed to have passed.
        raise exception_info[0], exception_info[1], exception_info[2]

    # Make sure we pick up any remaining tests.
    self._UpdateSummary(result_summary)
    return (thread_timings, test_timings, individual_test_timings)

  def Run(self, result_summary):
    """Run all our tests on all our test files.

    For each test file, we run each test type.  If there are any failures, we
    collect them for reporting.

    Args:
      result_summary: a summary object tracking the test results.

    Return:
      We return nonzero if there are regressions compared to the last run.
    """
    if not self._test_files:
      return 0
    start_time = time.time()
    test_shell_binary = path_utils.TestShellPath(self._options.target)

    # Start up any helper needed
    layout_test_helper_proc = None
    if not options.no_pixel_tests:
      helper_path = path_utils.LayoutTestHelperPath(self._options.target)
      if len(helper_path):
        logging.info("Starting layout helper %s" % helper_path)
        layout_test_helper_proc = subprocess.Popen([helper_path],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   stderr=None)
        is_ready = layout_test_helper_proc.stdout.readline()
        if not is_ready.startswith('ready'):
          logging.error("layout_test_helper failed to be ready")

    # Check that the system dependencies (themes, fonts, ...) are correct.
    if not self._options.nocheck_sys_deps:
      proc = subprocess.Popen([test_shell_binary,
                              "--check-layout-test-sys-deps"])
      if proc.wait() != 0:
        logging.info("Aborting because system dependencies check failed.\n"
                     "To override, invoke with --nocheck-sys-deps")
        sys.exit(1)

    # If we have http tests, the first one will be an http test.
    if ((self._test_files_list and
         self._test_files_list[0].find(self.HTTP_SUBDIR) >= 0)
        or self._options.randomize_order):
      self._http_server.Start()

    # Start Web Socket server.
    if (self._ContainWebSocketTest(self._test_files_list)):
      self._websocket_server.Start()
      # self._websocket_secure_server.Start()

    thread_timings, test_timings, individual_test_timings = (
        self._RunTests(test_shell_binary, self._test_files_list,
                       result_summary))
    original_results_by_test, original_results_by_type = self._CompareResults(
        result_summary)
    final_results_by_test = original_results_by_test
    final_results_by_type = original_results_by_type
    failed_results_by_test, failed_results_by_type = self._OnlyFailures(
        original_results_by_test)
    num_passes = len(original_results_by_test) - len(failed_results_by_test)

    retries = 0
    while (retries < self.NUM_RETRY_ON_UNEXPECTED_FAILURE and
        len(failed_results_by_test)):
      logging.info("Retrying %d unexpected failure(s)" %
                   len(failed_results_by_test))
      retries += 1
      retry_summary = ResultSummary(self._expectations,
                                    failed_results_by_test.keys())
      self._RunTests(test_shell_binary, failed_results_by_test.keys(),
                     retry_summary)
      final_results_by_test, final_results_by_type = self._CompareResults(
          retry_summary)
      failed_results_by_test, failed_results_by_type = self._OnlyFailures(
          final_results_by_test)

    self._StopLayoutTestHelper(layout_test_helper_proc)

    print
    end_time = time.time()
    logging.info("%6.2f total testing time" % (end_time - start_time))
    cuml_time = 0
    logging.debug("Thread timing:")
    for t in thread_timings:
      logging.debug("    %10s: %5d tests, %6.2f secs" %
                    (t['name'], t['num_tests'], t['total_time']))
      cuml_time += t['total_time']
    logging.debug("")

    logging.debug("   %6.2f cumulative, %6.2f optimal" %
                  (cuml_time, cuml_time / int(self._options.num_test_shells)))

    print
    self._PrintTimingStatistics(test_timings, individual_test_timings,
                                result_summary)

    self._PrintUnexpectedResults(original_results_by_test,
                                 original_results_by_type,
                                 final_results_by_test,
                                 final_results_by_type)

    # Write summaries to stdout.
    # The summaries should include flaky tests, so use the original summary,
    # not the final one.
    self._PrintResultSummary(result_summary, sys.stdout)

    if self._options.verbose:
      self._WriteJSONFiles(result_summary, individual_test_timings);

    # Write the same data to a log file.
    out_filename = os.path.join(self._options.results_directory, "score.txt")
    output_file = open(out_filename, "w")
    self._PrintResultSummary(result_summary, output_file)
    output_file.close()

    # Write the summary to disk (results.html) and maybe open the test_shell
    # to this file.
    wrote_results = self._WriteResultsHtmlFile(result_summary)
    if not self._options.noshow_results and wrote_results:
      self._ShowResultsHtmlFile()

    sys.stdout.flush()
    sys.stderr.flush()
    # Ignore flaky failures and unexpected passes so we don't turn the
    # bot red for those.
    return len(failed_results_by_test)

  def _UpdateSummary(self, result_summary):
    """Update the summary while running tests."""
    while True:
      try:
        (test, fail_list) = self._result_queue.get_nowait()
        result = test_failures.DetermineResultType(fail_list)
        result_summary.Add(test, fail_list, result)
      except Queue.Empty:
        return

  def _CompareResults(self, result_summary):
    """Determine if the results in this test run are unexpected.

    Returns:
      A dict of files -> results and a dict of results -> sets of files
    """
    results_by_test = {}
    results_by_type = {}
    for result in TestExpectationsFile.EXPECTATIONS.values():
      results_by_type[result] = set()
    for test, result in result_summary.unexpected_results.iteritems():
      results_by_test[test] = result
      results_by_type[result].add(test)
    return results_by_test, results_by_type

  def _OnlyFailures(self, results_by_test):
    """Filters a dict of results and returns only the failures.

    Args:
      results_by_test: a dict of files -> results

    Returns:
      a dict of files -> results and results -> sets of files.
    """
    failed_results_by_test = {}
    failed_results_by_type = {}
    for test, result in results_by_test.iteritems():
      if result != test_expectations.PASS:
        failed_results_by_test[test] = result
        if result in failed_results_by_type:
          failed_results_by_type.add(test)
        else:
          failed_results_by_type = set([test])
    return failed_results_by_test, failed_results_by_type

  def _WriteJSONFiles(self, result_summary, individual_test_timings):
    logging.debug("Writing JSON files in %s." % self._options.results_directory)
    # Write a json file of the test_expectations.txt file for the layout tests
    # dashboard.
    expectations_file = open(os.path.join(self._options.results_directory,
        "expectations.json"), "w")
    expectations_json = self._expectations.GetExpectationsJsonForAllPlatforms()
    expectations_file.write(("ADD_EXPECTATIONS(" + expectations_json + ");"))
    expectations_file.close()

    json_results_generator.JSONResultsGenerator(self._options,
        result_summary, individual_test_timings,
        self._options.results_directory, self._test_files_list)

    logging.debug("Finished writing JSON files.")

  def _PrintExpectedResultsOfType(self, result_summary, result_type,
      result_type_str):
    """Print the number of the tests in a given result class.

    Args:
      result_summary - the object containg all the results to report on
      result_type - the particular result type to report in the summary.
      result_type_str - a string description of the result_type.
    """
    tests = self._expectations.GetTestsWithResultType(result_type)
    now = result_summary.tests_by_timeline[test_expectations.NOW]
    wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]
    defer = result_summary.tests_by_timeline[test_expectations.DEFER]

    # We use a fancy format string in order to print the data out in a
    # nicely-aligned table.
    fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd defer, %%%dd wontfix)" %
              (self._NumDigits(now), self._NumDigits(defer),
               self._NumDigits(wontfix)))
    logging.info(fmtstr %
        (len(tests), result_type_str, len(tests & now), len(tests & defer),
         len(tests & wontfix)))

  def _NumDigits(self, num):
    """Returns the number of digits needed to represent the length of a
    sequence."""
    ndigits = 1
    if len(num):
      ndigits = int(math.log10(len(num))) + 1
    return ndigits

  def _PrintTimingStatistics(self, directory_test_timings,
      individual_test_timings, result_summary):
    self._PrintAggregateTestStatistics(individual_test_timings)
    self._PrintIndividualTestTimes(individual_test_timings, result_summary)
    self._PrintDirectoryTimings(directory_test_timings)

  def _PrintAggregateTestStatistics(self, individual_test_timings):
    """Prints aggregate statistics (e.g. median, mean, etc.) for all tests.
    Args:
      individual_test_timings: List of test_shell_thread.TestStats for all
          tests.
    """
    test_types = individual_test_timings[0].time_for_diffs.keys()
    times_for_test_shell = []
    times_for_diff_processing = []
    times_per_test_type = {}
    for test_type in test_types:
      times_per_test_type[test_type] = []

    for test_stats in individual_test_timings:
      times_for_test_shell.append(test_stats.test_run_time)
      times_for_diff_processing.append(test_stats.total_time_for_all_diffs)
      time_for_diffs = test_stats.time_for_diffs
      for test_type in test_types:
        times_per_test_type[test_type].append(time_for_diffs[test_type])

    self._PrintStatisticsForTestTimings(
        "PER TEST TIME IN TESTSHELL (seconds):",
        times_for_test_shell)
    self._PrintStatisticsForTestTimings(
        "PER TEST DIFF PROCESSING TIMES (seconds):",
        times_for_diff_processing)
    for test_type in test_types:
      self._PrintStatisticsForTestTimings(
          "PER TEST TIMES BY TEST TYPE: %s" % test_type,
          times_per_test_type[test_type])

  def _PrintIndividualTestTimes(self, individual_test_timings, result_summary):
    """Prints the run times for slow, timeout and crash tests.
    Args:
      individual_test_timings: List of test_shell_thread.TestStats for all
          tests.
      result_summary: Object containing the results of all the tests.
    """
    # Reverse-sort by the time spent in test_shell.
    individual_test_timings.sort(lambda a, b:
        cmp(b.test_run_time, a.test_run_time))

    num_printed = 0
    slow_tests = []
    timeout_or_crash_tests = []
    unexpected_slow_tests = []
    for test_tuple in individual_test_timings:
      filename = test_tuple.filename
      is_timeout_crash_or_slow = False
      if self._expectations.HasModifier(filename, test_expectations.SLOW):
        is_timeout_crash_or_slow = True
        slow_tests.append(test_tuple)

      if (result_summary.results[filename] == test_expectations.TIMEOUT or
          result_summary.results[filename] == test_expectations.CRASH):
        is_timeout_crash_or_slow = True
        timeout_or_crash_tests.append(test_tuple)

      if (not is_timeout_crash_or_slow and
          num_printed < self._options.num_slow_tests_to_log):
        num_printed = num_printed + 1
        unexpected_slow_tests.append(test_tuple)

    print
    self._PrintTestListTiming("%s slowest tests that are not marked as SLOW "
        "and did not timeout/crash:" % self._options.num_slow_tests_to_log,
        unexpected_slow_tests)
    print
    self._PrintTestListTiming("Tests marked as SLOW:", slow_tests)
    print
    self._PrintTestListTiming("Tests that timed out or crashed:",
        timeout_or_crash_tests)
    print

  def _PrintTestListTiming(self, title, test_list):
    logging.debug(title)
    for test_tuple in test_list:
      filename = test_tuple.filename[len(path_utils.LayoutTestsDir()) + 1:]
      filename = filename.replace('\\', '/')
      test_run_time = round(test_tuple.test_run_time, 1)
      logging.debug("%s took %s seconds" % (filename, test_run_time))

  def _PrintDirectoryTimings(self, directory_test_timings):
    timings = []
    for directory in directory_test_timings:
      num_tests, time_for_directory = directory_test_timings[directory]
      timings.append((round(time_for_directory, 1), directory, num_tests))
    timings.sort()

    logging.debug("Time to process each subdirectory:")
    for timing in timings:
      logging.debug("%s took %s seconds to run %s tests." % \
                    (timing[1], timing[0], timing[2]))

  def _PrintStatisticsForTestTimings(self, title, timings):
    """Prints the median, mean and standard deviation of the values in timings.
    Args:
      title: Title for these timings.
      timings: A list of floats representing times.
    """
    logging.debug(title)
    timings.sort()

    num_tests = len(timings)
    percentile90 = timings[int(.9 * num_tests)]
    percentile99 = timings[int(.99 * num_tests)]

    if num_tests % 2 == 1:
      median = timings[((num_tests - 1) / 2) - 1]
    else:
      lower = timings[num_tests / 2 - 1]
      upper = timings[num_tests / 2]
      median = (float(lower + upper)) / 2

    mean = sum(timings) / num_tests

    for time in timings:
      sum_of_deviations = math.pow(time - mean, 2)

    std_deviation = math.sqrt(sum_of_deviations / num_tests)
    logging.debug(("Median: %s, Mean: %s, 90th percentile: %s, "
        "99th percentile: %s, Standard deviation: %s\n" % (
        median, mean, percentile90, percentile99, std_deviation)))

  def _PrintResultSummary(self, result_summary, output):
    """Print a short summary to the output file about how many tests passed.

    Args:
      output: a file or stream to write to
    """
    failed = len(result_summary.failures)
    skipped = len(result_summary.tests_by_expectation[test_expectations.SKIP])
    total = result_summary.total
    passed = total - failed - skipped
    pct_passed = 0.0
    if total > 0:
      pct_passed = float(passed) * 100 / total

    output.write("\n");
    output.write("=> Total: %d/%d tests passed (%.1f%%)\n" %
                 (passed, total, pct_passed))

    output.write("\n");
    self._PrintResultSummaryEntry(result_summary, test_expectations.NOW,
       "Tests to be fixed for the current release", output)

    output.write("\n");
    self._PrintResultSummaryEntry(result_summary, test_expectations.DEFER,
       "Tests we'll fix in the future if they fail (DEFER)", output)

    output.write("\n");
    self._PrintResultSummaryEntry(result_summary, test_expectations.WONTFIX,
       "Tests that won't be fixed if they fail (WONTFIX)", output)

  def _PrintResultSummaryEntry(self, result_summary, timeline, heading, output):
    """Print a summary block of results for a particular timeline of test.

    Args:
      result_summary: summary to print results for
      timeline: the timeline to print results for (NOT, WONTFIX, etc.)
      heading: a textual description of the timeline
      output: a stream to write to
    """
    total = len(result_summary.tests_by_timeline[timeline])
    not_passing = (total -
       len(result_summary.tests_by_expectation[test_expectations.PASS] &
           result_summary.tests_by_timeline[timeline]))
    output.write("=> %s (%d):\n" % (heading, not_passing))
    for result in TestExpectationsFile.EXPECTATION_ORDER:
      if result == test_expectations.PASS:
        continue
      results = (result_summary.tests_by_expectation[result] &
                 result_summary.tests_by_timeline[timeline])
      desc = TestExpectationsFile.EXPECTATION_DESCRIPTIONS[result]
      plural = ["", "s"]
      if not_passing and len(results):
        pct = len(results) * 100.0 / not_passing
        output.write("%d test case%s (%4.1f%%) %s\n" %
                     (len(results), plural[len(results) != 1], pct,
                      desc[len(results) != 1]))

  def _PrintUnexpectedResults(self, original_results_by_test,
                              original_results_by_type,
                              final_results_by_test,
                              final_results_by_type):
    """Print unexpected results (regressions) to stdout and a file.

    Args:
      original_results_by_test: dict mapping tests -> results for the first
        (original) test run
      original_results_by_type: dict mapping results -> sets of tests for the
        first test run
      final_results_by_test: dict of tests->results after the retries
        eliminated any flakiness
      final_results_by_type: dict of results->tests after the retries
        eliminated any flakiness
    """
    print "-" * 78
    print
    flaky_results_by_type = {}
    non_flaky_results_by_type = {}

    for test, result in original_results_by_test.iteritems():
      if (result == test_expectations.PASS or
          (test in final_results_by_test and
           result == final_results_by_test[test])):
        if result in non_flaky_results_by_type:
          non_flaky_results_by_type[result].add(test)
        else:
          non_flaky_results_by_type[result] = set([test])
      else:
        if result in flaky_results_by_type:
          flaky_results_by_type[result].add(test)
        else:
          flaky_results_by_type[result] = set([test])

    self._PrintUnexpectedResultsByType(non_flaky_results_by_type, False,
                                       sys.stdout)
    self._PrintUnexpectedResultsByType(flaky_results_by_type, True, sys.stdout)

    out_filename = os.path.join(self._options.results_directory,
                                "regressions.txt")
    output_file = open(out_filename, "w")
    self._PrintUnexpectedResultsByType(non_flaky_results_by_type, False,
                                       output_file)
    output_file.close()

    print "-" * 78

  def _PrintUnexpectedResultsByType(self, results_by_type, is_flaky, output):
    """Helper method to print a set of unexpected results to an output stream
    sorted by the result type.

    Args:
      results_by_type: dict(result_type -> list of files)
      is_flaky: where these results flaky or not (changes the output)
      output: stream to write output to
    """
    descriptions = TestExpectationsFile.EXPECTATION_DESCRIPTIONS
    keywords = {}
    for k, v in TestExpectationsFile.EXPECTATIONS.iteritems():
      keywords[v] = k.upper()

    for result in TestExpectationsFile.EXPECTATION_ORDER:
      if result in results_by_type:
        self._PrintUnexpectedResultSet(output, results_by_type[result],
                                       is_flaky, descriptions[result][1],
                                       keywords[result])

  def _PrintUnexpectedResultSet(self, output, filenames, is_flaky,
                                header_text, keyword):
    """A helper method to print one set of results (all of the same type)
    to a stream.

    Args:
      output: a stream or file object to write() to
      filenames: a list of absolute filenames
      header_text: a string to display before the list of filenames
      keyword: expectation keyword
    """
    filenames = list(filenames)
    filenames.sort()
    if not is_flaky and keyword == 'PASS':
      self._PrintUnexpectedPasses(output, filenames)
      return

    if is_flaky:
      prefix = "Unexpected flakiness:"
    else:
      prefix = "Regressions: Unexpected"
    output.write("%s %s (%d):\n" % (prefix, header_text, len(filenames)))
    for filename in filenames:
      flaky_text = ""
      if is_flaky:
        flaky_text = " " + self._expectations.GetExpectationsString(filename)

      filename = path_utils.RelativeTestFilename(filename)
      output.write("  %s = %s%s\n" % (filename, keyword, flaky_text))
    output.write("\n")

  def _PrintUnexpectedPasses(self, output, filenames):
    """Prints the list of files that passed unexpectedly.

    TODO(dpranke): This routine is a bit of a hack, since it's not clear
    what the best way to output this is. Each unexpected pass might have
    multiple expected results, and printing all the combinations would
    be pretty clunky. For now we sort them into three buckets, crashes,
    timeouts, and everything else.

    Args:
      output: a stream to write to
      filenames: list of files that passed
    """
    crashes = []
    timeouts = []
    failures = []
    for filename in filenames:
      expectations = self._expectations.GetExpectations(filename)
      if test_expectations.CRASH in expectations:
        crashes.append(filename)
      elif test_expectations.TIMEOUT in expectations:
        timeouts.append(filename)
      else:
        failures.append(filename)

    self._PrintPassSet(output, "crash", crashes)
    self._PrintPassSet(output, "timeout", timeouts)
    self._PrintPassSet(output, "fail", failures)

  def _PrintPassSet(self, output, expected_str, filenames):
    """Print a set of unexpected passes.

    Args:
      output: stream to write to
      expected_str: worst expectation for the given file
      filenames: list of files in that set
    """
    if len(filenames):
      filenames.sort()
      output.write("Expected to %s, but passed: (%d)\n" %
                   (expected_str, len(filenames)))
      for filename in filenames:
        filename = path_utils.RelativeTestFilename(filename)
        output.write("  %s\n" % filename)
      output.write("\n")

  def _WriteResultsHtmlFile(self, result_summary):
    """Write results.html which is a summary of tests that failed.

    Args:
      result_summary: a summary of the results :)

    Returns:
      True if any results were written (since expected failures may be omitted)
    """
    # test failures
    failures = result_summary.failures
    failed_results_by_test, failed_results_by_type = self._OnlyFailures(
        result_summary.unexpected_results)
    if self._options.full_results_html:
      test_files = failures.keys()
    else:
      test_files = failed_results_by_test.keys()
    if not len(test_files):
      return False

    out_filename = os.path.join(self._options.results_directory,
                                "results.html")
    out_file = open(out_filename, 'w')
    # header
    if self._options.full_results_html:
      h2 = "Test Failures"
    else:
      h2 = "Unexpected Test Failures"
    out_file.write("<html><head><title>Layout Test Results (%(time)s)</title>"
                   "</head><body><h2>%(h2)s (%(time)s)</h2>\n"
                   % {'h2': h2, 'time': time.asctime()})

    test_files.sort()
    for test_file in test_files:
      if test_file in failures: test_failures = failures[test_file]
      else: test_failures = []  # unexpected passes
      out_file.write("<p><a href='%s'>%s</a><br />\n"
                     % (path_utils.FilenameToUri(test_file),
                        path_utils.RelativeTestFilename(test_file)))
      for failure in test_failures:
        out_file.write("&nbsp;&nbsp;%s<br/>"
                       % failure.ResultHtmlOutput(
                         path_utils.RelativeTestFilename(test_file)))
      out_file.write("</p>\n")

    # footer
    out_file.write("</body></html>\n")
    return True

  def _ShowResultsHtmlFile(self):
    """Launches the test shell open to the results.html page."""
    results_filename = os.path.join(self._options.results_directory,
                                    "results.html")
    subprocess.Popen([path_utils.TestShellPath(self._options.target),
                      path_utils.FilenameToUri(results_filename)])


def ReadTestFiles(files):
  tests = []
  for file in files:
    for line in open(file):
      line = test_expectations.StripComments(line)
      if line: tests.append(line)
  return tests

def main(options, args):
  """Run the tests.  Will call sys.exit when complete.

  Args:
    options: a dictionary of command line options
    args: a list of sub directories or files to test
  """

  if options.sources:
    options.verbose = True

  # Set up our logging format.
  log_level = logging.INFO
  if options.verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')

  if not options.target:
    if options.debug:
      options.target = "Debug"
    else:
      options.target = "Release"

  if not options.use_apache:
    options.use_apache = sys.platform == 'darwin'

  if options.results_directory.startswith("/"):
    # Assume it's an absolute path and normalize.
    options.results_directory = path_utils.GetAbsolutePath(
        options.results_directory)
  else:
    # If it's a relative path, make the output directory relative to Debug or
    # Release.
    basedir = path_utils.PathFromBase('webkit')
    options.results_directory = path_utils.GetAbsolutePath(
        os.path.join(basedir, options.target, options.results_directory))

  if options.clobber_old_results:
    # Just clobber the actual test results directories since the other files
    # in the results directory are explicitly used for cross-run tracking.
    test_dirs = ("LayoutTests", "chrome", "pending")
    for directory in test_dirs:
      path = os.path.join(options.results_directory, directory)
      if os.path.exists(path):
        shutil.rmtree(path)

  # Ensure platform is valid and force it to the form 'chromium-<platform>'.
  options.platform = path_utils.PlatformName(options.platform)

  if not options.num_test_shells:
    cpus = 1
    if sys.platform in ('win32', 'cygwin'):
      cpus = int(os.environ.get('NUMBER_OF_PROCESSORS', 1))
    elif (hasattr(os, "sysconf") and
          os.sysconf_names.has_key("SC_NPROCESSORS_ONLN")):
      # Linux & Unix:
      ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
      if isinstance(ncpus, int) and ncpus > 0:
        cpus = ncpus
    elif sys.platform in ('darwin'): # OSX:
      cpus = int(os.popen2("sysctl -n hw.ncpu")[1].read())

    # TODO(ojan): Use cpus+1 once we flesh out the flakiness.
    options.num_test_shells = cpus

  logging.info("Running %s test_shells in parallel" % options.num_test_shells)

  if not options.time_out_ms:
    if options.target == "Debug":
      options.time_out_ms = str(2 * TestRunner.DEFAULT_TEST_TIMEOUT_MS)
    else:
      options.time_out_ms = str(TestRunner.DEFAULT_TEST_TIMEOUT_MS)

  options.slow_time_out_ms = str(5 * int(options.time_out_ms))
  logging.info("Regular timeout: %s, slow test timeout: %s" %
      (options.time_out_ms, options.slow_time_out_ms))

  # Include all tests if none are specified.
  new_args = []
  for arg in args:
    if arg and arg != '':
      new_args.append(arg)

  paths = new_args
  if not paths:
    paths = []
  if options.test_list:
    paths += ReadTestFiles(options.test_list)

  # Create the output directory if it doesn't already exist.
  path_utils.MaybeMakeDirectory(options.results_directory)

  test_runner = TestRunner(options)
  test_runner.GatherFilePaths(paths)

  if options.lint_test_files:
    # Creating the expecations for each platform/target pair does all the
    # test list parsing and ensures it's correct syntax (e.g. no dupes).
    for platform in TestExpectationsFile.PLATFORMS:
      test_runner.ParseExpectations(platform, is_debug_mode=True)
      test_runner.ParseExpectations(platform, is_debug_mode=False)
    print ("If there are no fail messages, errors or exceptions, then the "
        "lint succeeded.")
    sys.exit(0)
  else:
    test_runner.ParseExpectations(options.platform, options.target == 'Debug')
    result_summary = test_runner.PrepareListsAndPrintOutput()

  if options.find_baselines:
    # Record where we found each baseline, then exit.
    print("html,txt,checksum,png");
    for t in test_runner._test_files:
      (expected_txt_dir, expected_txt_file) = path_utils.ExpectedBaseline(
          t, '.txt')[0]
      (expected_png_dir, expected_png_file) = path_utils.ExpectedBaseline(
          t, '.png')[0]
      (expected_checksum_dir,
          expected_checksum_file) = path_utils.ExpectedBaseline(
          t, '.checksum')[0]
      print("%s,%s,%s,%s" % (path_utils.RelativeTestFilename(t),
            expected_txt_dir, expected_checksum_dir, expected_png_dir))
    return

  try:
    test_shell_binary_path = path_utils.TestShellPath(options.target)
  except path_utils.PathNotFound:
    print "\nERROR: test_shell is not found. Be sure that you have built it"
    print "and that you are using the correct build. This script will run the"
    print "Release one by default. Use --debug to use the Debug build.\n"
    sys.exit(1)

  logging.info("Using platform '%s'" % options.platform)
  logging.info("Placing test results in %s" % options.results_directory)
  if options.new_baseline:
    logging.info("Placing new baselines in %s" %
                 path_utils.ChromiumBaselinePath(options.platform))
  logging.info("Using %s build at %s" %
               (options.target, test_shell_binary_path))
  if not options.no_pixel_tests:
    logging.info("Running pixel tests")

  if 'cygwin' == sys.platform:
    logging.warn("#" * 40)
    logging.warn("# UNEXPECTED PYTHON VERSION")
    logging.warn("# This script should be run using the version of python")
    logging.warn("# in third_party/python_24/")
    logging.warn("#" * 40)
    sys.exit(1)

  # Delete the disk cache if any to ensure a clean test run.
  cachedir = os.path.split(test_shell_binary_path)[0]
  cachedir = os.path.join(cachedir, "cache")
  if os.path.exists(cachedir):
    shutil.rmtree(cachedir)

  test_runner.AddTestType(text_diff.TestTextDiff)
  if not options.no_pixel_tests:
    test_runner.AddTestType(image_diff.ImageDiff)
    if options.fuzzy_pixel_tests:
      test_runner.AddTestType(fuzzy_image_diff.FuzzyImageDiff)
  has_new_failures = test_runner.Run(result_summary)
  logging.info("Exit status: %d" % has_new_failures)
  sys.exit(has_new_failures)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--no-pixel-tests", action="store_true",
                           default=False,
                           help="disable pixel-to-pixel PNG comparisons")
  option_parser.add_option("", "--fuzzy-pixel-tests", action="store_true",
                           default=False,
                           help="Also use fuzzy matching to compare pixel test"
                                " outputs.")
  option_parser.add_option("", "--results-directory",
                           default="layout-test-results",
                           help="Output results directory source dir,"
                                " relative to Debug or Release")
  option_parser.add_option("", "--new-baseline", action="store_true",
                           default=False,
                           help="save all generated results as new baselines "
                                "into the platform directory, overwriting "
                                "whatever's already there.")
  option_parser.add_option("", "--noshow-results", action="store_true",
                           default=False, help="don't launch the test_shell"
                           " with results after the tests are done")
  option_parser.add_option("", "--full-results-html", action="store_true",
                           default=False, help="show all failures in "
                           "results.html, rather than only regressions")
  option_parser.add_option("", "--clobber-old-results", action="store_true",
                           default=False, help="Clobbers test results from "
                           "previous runs.")
  option_parser.add_option("", "--lint-test-files", action="store_true",
                           default=False, help="Makes sure the test files "
                           "parse for all configurations. Does not run any "
                           "tests.")
  option_parser.add_option("", "--force", action="store_true",
                           default=False,
                           help="Run all tests, even those marked SKIP in the "
                                "test list")
  option_parser.add_option("", "--num-test-shells",
                           help="Number of testshells to run in parallel.")
  option_parser.add_option("", "--use-apache", action="store_true",
                           default=False,
                           help="Whether to use apache instead of lighttpd.")
  option_parser.add_option("", "--time-out-ms", default=None,
                           help="Set the timeout for each test")
  option_parser.add_option("", "--run-singly", action="store_true",
                           default=False,
                           help="run a separate test_shell for each test")
  option_parser.add_option("", "--debug", action="store_true", default=False,
                           help="use the debug binary instead of the release "
                                "binary")
  option_parser.add_option("", "--num-slow-tests-to-log", default=50,
                           help="Number of slow tests whose timings to print.")
  option_parser.add_option("", "--platform",
                           help="Override the platform for expected results")
  option_parser.add_option("", "--target", default="",
                           help="Set the build target configuration (overrides"
                                 " --debug)")
  # TODO(pamg): Support multiple levels of verbosity, and remove --sources.
  option_parser.add_option("-v", "--verbose", action="store_true",
                           default=False, help="include debug-level logging")
  option_parser.add_option("", "--sources", action="store_true",
                           help="show expected result file path for each test "
                                "(implies --verbose)")
  option_parser.add_option("", "--startup-dialog", action="store_true",
                           default=False,
                           help="create a dialog on test_shell.exe startup")
  option_parser.add_option("", "--gp-fault-error-box", action="store_true",
                           default=False,
                           help="enable Windows GP fault error box")
  option_parser.add_option("", "--wrapper",
                           help="wrapper command to insert before invocations "
                                "of test_shell; option is split on whitespace "
                                "before running.  (example: "
                                "--wrapper='valgrind --smc-check=all')")
  option_parser.add_option("", "--test-list", action="append",
                           help="read list of tests to run from file",
                           metavar="FILE")
  option_parser.add_option("", "--nocheck-sys-deps", action="store_true",
                           default=False,
                           help="Don't check the system dependencies (themes)")
  option_parser.add_option("", "--randomize-order", action="store_true",
                           default=False,
                           help=("Run tests in random order (useful for "
                                 "tracking down corruption)"))
  option_parser.add_option("", "--run-chunk",
                           default=None,
                           help=("Run a specified chunk (n:l), the nth of len "
                                 "l, of the layout tests"))
  option_parser.add_option("", "--run-part",
                           default=None,
                           help=("Run a specified part (n:m), the nth of m"
                                 " parts, of the layout tests"))
  option_parser.add_option("", "--batch-size",
                           default=None,
                           help=("Run a the tests in batches (n), after every "
                                 "n tests, the test shell is relaunched."))
  option_parser.add_option("", "--builder-name",
                           default="DUMMY_BUILDER_NAME",
                           help=("The name of the builder shown on the "
                                 "waterfall running this script e.g. WebKit."))
  option_parser.add_option("", "--build-name",
                           default="DUMMY_BUILD_NAME",
                           help=("The name of the builder used in it's path, "
                                 "e.g. webkit-rel."))
  option_parser.add_option("", "--build-number",
                           default="DUMMY_BUILD_NUMBER",
                           help=("The build number of the builder running"
                                 "this script."))
  option_parser.add_option("", "--find-baselines", action="store_true",
                           default=False,
                           help="Prints a table mapping tests to their "
                                "expected results")
  option_parser.add_option("", "--experimental-fully-parallel",
                           action="store_true", default=False,
                           help="run all tests in parallel")
  options, args = option_parser.parse_args()
  main(options, args)
