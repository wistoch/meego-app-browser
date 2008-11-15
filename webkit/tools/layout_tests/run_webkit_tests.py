#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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

import glob
import logging
import optparse
import os
import Queue
import shutil
import subprocess
import sys
import time

import google.path_utils

from layout_package import compare_failures
from layout_package import test_expectations
from layout_package import http_server
from layout_package import path_utils
from layout_package import test_failures
from layout_package import test_shell_thread
from test_types import image_diff
from test_types import test_type_base
from test_types import text_diff
from test_types import simplified_text_diff


# The test list files are found in this subdirectory, which must be a sibling
# to this script itself.
TEST_FILE_DIR = 'test_lists'


class TestRunner:
  """A class for managing running a series of tests on a series of test
  files."""

  # When collecting test cases, we include any file with these extensions.
  _supported_file_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl',
                                    '.php', '.svg'])
  # When collecting test cases, skip these directories
  _skipped_directories = set(['.svn', '_svn', 'resources'])

  HTTP_SUBDIR = os.sep.join(['', 'http', ''])

  def __init__(self, options, paths):
    """Collect a list of files to test.

    Args:
      options: a dictionary of command line options
      paths: a list of paths to crawl looking for test files
    """
    self._options = options

    self._http_server = http_server.Lighttpd(options.results_directory)
    # a list of TestType objects
    self._test_types = []
    # a set of test files
    self._test_files = set()

    self._GatherTestFiles(paths)

  def __del__(self):
    sys.stdout.flush()
    sys.stderr.flush()
    # Stop the http server.
    self._http_server.Stop()

  def _GatherTestFiles(self, paths):
    """Generate a set of test files and place them in self._test_files, with
    appropriate subsets in self._ignored_failures, self._fixable_failures,
    and self._fixable_crashes.
    
    Args:
      paths: a list of command line paths relative to the webkit/tests
             directory.  glob patterns are ok.
    """
    paths_to_walk = set()
    for path in paths:
      # If there's an * in the name, assume it's a glob pattern.
      path = os.path.join(path_utils.LayoutDataDir(), path)
      if path.find('*') > -1:
        filenames = glob.glob(path)
        paths_to_walk.update(filenames)
      else:
        paths_to_walk.add(path)

    # Now walk all the paths passed in on the command line and get filenames
    for path in paths_to_walk:
      if os.path.isfile(path) and self._HasSupportedExtension(path):
        self._test_files.add(os.path.normpath(path))
        continue

      for root, dirs, files in os.walk(path):
        # don't walk skipped directories and sub directories
        if os.path.basename(root) in TestRunner._skipped_directories:
          del dirs[:]
          continue

        for filename in files:
          if self._HasSupportedExtension(filename):
            filename = os.path.join(root, filename)
            filename = os.path.normpath(filename)
            self._test_files.add(filename)

    # Filter and sort out files from the skipped, ignored, and fixable file
    # lists.
    saved_test_files = set()
    if len(self._test_files) == 1:
      # If there's only one test file, we don't want to skip it, but we do want
      # to sort it.  So we save it to add back to the list later.
      saved_test_files = self._test_files

    file_dir = os.path.join(os.path.dirname(sys.argv[0]), TEST_FILE_DIR)
    file_dir = os.path.join(file_dir, path_utils.TestListPlatformDir())
    file_dir = path_utils.GetAbsolutePath(file_dir)

    is_debug_mode = self._options.target == 'Debug'
    expectations = test_expectations.TestExpectations(self._test_files,
                                                      file_dir,
                                                      is_debug_mode)

    # Remove skipped - both fixable and ignored - files from the
    # top-level list of files to test.
    skipped = (expectations.GetFixableSkipped() |
               expectations.GetIgnoredSkipped())

    self._test_files -= skipped

    # If there was only one test file, run the test even if it was skipped.
    if len(saved_test_files):
      self._test_files = saved_test_files

    logging.info('Run: %d tests' % len(self._test_files))
    logging.info('Skipped: %d tests' % len(skipped))
    logging.info('Skipped tests do not appear in any of the below numbers\n')
    logging.info('Expected passes: %d tests' % 
                 len(self._test_files - 
                     expectations.GetFixable() - 
                     expectations.GetIgnored())) 
    logging.info(('Expected failures: %d fixable, %d ignored') % 
                 (len(expectations.GetFixableFailures()),
                  len(expectations.GetIgnoredFailures())))
    logging.info(('Expected timeouts: %d fixable, %d ignored') % 
                 (len(expectations.GetFixableTimeouts()),
                  len(expectations.GetIgnoredTimeouts())))   
    logging.info('Expected crashes: %d fixable tests' % 
                 len(expectations.GetFixableCrashes()))

    # Store the expectations in this object to allow it to be used to 
    # track regressions and print results.
    self._expectations = expectations

  def _HasSupportedExtension(self, filename):
    """Return true if filename is one of the file extensions we want to run a
    test on."""
    extension = os.path.splitext(filename)[1]
    return extension in TestRunner._supported_file_extensions

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

  def Run(self):
    """Run all our tests on all our test files.

    For each test file, we run each test type.  If there are any failures, we
    collect them for reporting.
    
    Return:
      We return nonzero if there are regressions compared to the last run.
    """
    if not self._test_files:
      return 0
    start_time = time.time()
    logging.info("Starting tests")

    # Create the output directory if it doesn't already exist.
    google.path_utils.MaybeMakeDirectory(self._options.results_directory)

    test_files = list(self._test_files)
    test_files.sort(self.TestFilesSort)
    # Create the thread safe queue of (test filenames, test URIs) tuples. Each
    # TestShellThread pulls values from this queue.
    filename_queue = Queue.Queue()
    for test_file in test_files:
      filename_queue.put((test_file, path_utils.FilenameToUri(test_file)))
    
    # If we have http tests, the first one will be an http test.
    if test_files and test_files[0].find(self.HTTP_SUBDIR) >= 0:
      self._http_server.Start()

    # Instantiate TestShellThreads and start them.
    threads = []
    test_shell_binary = path_utils.TestShellBinaryPath(self._options.target)
    for i in xrange(int(self._options.num_test_shells)):
      shell_args = []
      test_args = test_type_base.TestArguments()
      if not self._options.no_pixel_tests:
        png_path = os.path.join(self._options.results_directory,
                                "png_result%s.png" % i)
        shell_args.append("--pixel-tests=" + png_path)
        test_args.png_path = png_path

      test_args.wdiff = self._options.wdiff
      test_args.new_baseline = self._options.new_baseline
      test_args.show_sources = self._options.sources

      # Create separate TestTypes instances for each thread.
      test_types = []
      for t in self._test_types:
        test_types.append(t(self._options.platform,
                            self._options.results_directory))

      if self._options.startup_dialog:
        shell_args.append('--testshell-startup-dialog')
        
      if self._options.winhttp:
        shell_args.append('--winhttp')

      # larger timeout if page heap is enabled.
      if self._options.time_out_ms:
        shell_args.append('--time-out-ms=' + self._options.time_out_ms)

      thread = test_shell_thread.TestShellThread(filename_queue,
                                                 test_shell_binary,
                                                 test_types,
                                                 test_args,
                                                 shell_args,
                                                 self._options)
      thread.start()
      threads.append(thread)

    # Wait for the threads to finish and collect test failures.
    test_failures = {}
    for thread in threads:
      thread.join()
      test_failures.update(thread.GetFailures())

    print
    end_time = time.time()
    logging.info("%f total testing time" % (end_time - start_time))
    
    # Tests are done running. Compare failures with expected failures.
    regressions = self._CompareFailures(test_failures)

    # Write summaries to stdout.
    self._PrintResults(test_failures)

    # Write the summary to disk (results.html) and maybe open the test_shell
    # to this file.
    wrote_results = self._WriteResultsHtmlFile(test_failures, regressions)
    if not self._options.noshow_results and wrote_results:
      self._ShowResultsHtmlFile()

    sys.stdout.flush()
    sys.stderr.flush()
    return len(regressions)

  def _PrintResults(self, test_failures):
    """Print a short summary to stdout about how many tests passed.
    
    Args:
      test_failures is a dictionary mapping the test filename to a list of
      TestFailure objects if the test failed
    """

    failure_counts = {}
    fixable_counts = {}
    non_ignored_counts = {}
    fixable_failures = set()
    non_ignored_failures = set()

    # Aggregate failures in a dictionary (TestFailure -> frequency),
    # with known (fixable and ignored) failures separated out.
    def AddFailure(dictionary, key):
      if key in dictionary:
        dictionary[key] += 1
      else:
        dictionary[key] = 1

    for test, failures in test_failures.iteritems():
      for failure in failures:
        AddFailure(failure_counts, failure.__class__)
        if self._expectations.IsFixable(test):
          AddFailure(fixable_counts, failure.__class__)
          fixable_failures.add(test)
        if not self._expectations.IsIgnored(test):
          AddFailure(non_ignored_counts, failure.__class__)
          non_ignored_failures.add(test)                
                             
    # Print summaries.
    print "-" * 78

    # Print breakdown of tests we need to fix and want to pass. 
    # Include skipped fixable tests in the statistics.
    skipped = self._expectations.GetFixableSkipped()

    self._PrintResultSummary("=> Tests to be fixed",
                             self._expectations.GetFixable(),
                             fixable_failures,
                             fixable_counts,
                             skipped)

    self._PrintResultSummary("=> Tests we want to pass",
                             (self._test_files - 
                              self._expectations.GetIgnored()),
                             non_ignored_failures,
                             non_ignored_counts,
                             skipped)

    # Print breakdown of all tests including all skipped tests.
    skipped |= self._expectations.GetIgnoredSkipped()
    self._PrintResultSummary("=> All tests",
                             self._test_files,
                             test_failures,
                             failure_counts,
                             skipped)
    print

  def _PrintResultSummary(self, heading, all, failed, failure_counts, skipped):
    """Print a summary block of results for a particular category of test.

    Args:
      heading: text to print before the block, followed by the total count
      all: list of all tests in this category
      failed: list of failing tests in this category
      failure_counts: dictionary of (TestFailure -> frequency)
    """
    total = len(all | skipped)
    print "\n%s (%d):" % (heading, total)
    skip_count = len(skipped)
    pass_count = total - skip_count - len(failed)
    self._PrintResultLine(pass_count, total, "Passed")
    self._PrintResultLine(skip_count, total, "Skipped")
    # Sort the failure counts and print them one by one.
    sorted_keys = sorted(failure_counts.keys(),
                         key=test_failures.FailureSort.SortOrder)
    for failure in sorted_keys:
      self._PrintResultLine(failure_counts[failure], total, failure.Message())

  def _PrintResultLine(self, count, total, message):
    if count == 0: return
    print ("%(count)d test case%(plural)s (%(percent).1f%%) %(message)s" % 
           { 'count'   : count,
             'plural'  : ('s', '')[count == 1],
             'percent' : float(count) * 100 / total,
             'message' : message })

  def _CompareFailures(self, test_failures):
    """Determine how the test failures from this test run differ from the
    previous test run and print results to stdout and a file.

    Args:
      test_failures is a dictionary mapping the test filename to a list of
      TestFailure objects if the test failed
    
    Return:
      A set of regressions (unexpected failures, hangs, or crashes)
    """
    cf = compare_failures.CompareFailures(self._test_files,
                                          test_failures,
                                          self._expectations)

    if not self._options.nocompare_failures: cf.PrintRegressions()
    return cf.GetRegressions()    

  def _WriteResultsHtmlFile(self, test_failures, regressions):
    """Write results.html which is a summary of tests that failed.

    Args:
      test_failures: a dictionary mapping the test filename to a list of
          TestFailure objects if the test failed
      regressions: a set of test filenames that regressed

    Returns:
      True if any results were written (since expected failures may be omitted)
    """
    # test failures
    if self._options.full_results_html:
      test_files = test_failures.keys()
    else:
      test_files = list(regressions)
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
      if test_file in test_failures: failures = test_failures[test_file]
      else: failures = []  # unexpected passes
      out_file.write("<p><a href='%s'>%s</a><br />\n"
                     % (path_utils.FilenameToUri(test_file),
                        path_utils.RelativeTestFilename(test_file)))
      for failure in failures:
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
    subprocess.Popen([path_utils.TestShellBinaryPath(self._options.target),
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

  if options.platform is None:
    options.platform = path_utils.PlatformDir()

  if options.results_directory.startswith("/"):
    # Assume it's an absolute path and normalize
    options.results_directory = path_utils.GetAbsolutePath(
        options.results_directory)
  else:
    # If it's a relative path, make the output directory relative to Debug or
    # Release.
    basedir = path_utils.WebKitRoot()
    basedir = os.path.join(basedir, options.target)

    options.results_directory = path_utils.GetAbsolutePath(
        os.path.join(basedir, options.results_directory))

  try:
    test_shell_binary_path = path_utils.TestShellBinaryPath(options.target)
  except:
    print "\nERROR: test_shell is not found. Be sure that you have built it"
    print "and that you are using the correct build. This script will run the"
    print "Release one by default. Use --debug to use the Debug build.\n"
    sys.exit(1)

  logging.info("Using platform '%s'" % options.platform)
  logging.info("Placing test results in %s" % options.results_directory)
  if options.new_baseline:
    logging.info("Placing new baselines in %s" %
                 os.path.join(path_utils.PlatformResultsDir(options.platform),
                              options.platform))
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

  # This was an experimental feature where we would run more than one
  # test_shell in parallel.  For some reason, this would result in different
  # layout test results, so just use 1 test_shell for now.
  options.num_test_shells = 1

  # Include all tests if none are specified.
  paths = []
  if args:
    paths += args
  if options.test_list:
    paths += ReadTestFiles(options.test_list)
  if not paths:
    paths = ['.']
  test_runner = TestRunner(options, paths)
  test_runner.AddTestType(text_diff.TestTextDiff)
  test_runner.AddTestType(simplified_text_diff.SimplifiedTextDiff)
  if not options.no_pixel_tests:
    test_runner.AddTestType(image_diff.ImageDiff)
  has_new_failures = test_runner.Run()
  logging.info("Exit status: %d" % has_new_failures)
  sys.exit(has_new_failures)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--no-pixel-tests", action="store_true",
                           default=False,
                           help="disable pixel-to-pixel PNG comparisons")
  option_parser.add_option("", "--wdiff", action="store_true",
                           default=False,
                           help="enable word-by-word diffing")
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
                           default=False, help="show all failures in"
                           "results.html, rather than only regressions")
  option_parser.add_option("", "--nocompare-failures", action="store_true",
                           default=False,
                           help="Disable comparison to the last test run. "
                                "When enabled, show stats on how many tests "
                                "newly pass or fail.")
  option_parser.add_option("", "--time-out-ms",
                           default=None,
                           help="Set the timeout for each test")
  option_parser.add_option("", "--run-singly", action="store_true",
                           default=False,
                           help="run a separate test_shell for each test")
  option_parser.add_option("", "--debug", action="store_true", default=False,
                           help="use the debug binary instead of the release "
                                "binary")
  option_parser.add_option("", "--platform",
                           help="Override the platform for expected results")
  option_parser.add_option("", "--target", default="",
                           help="Set the build target configuration (overrides"
                                 "--debug)")
  # TODO(pamg): Support multiple levels of verbosity, and remove --sources.
  option_parser.add_option("-v", "--verbose", action="store_true",
                           default=False, help="include debug-level logging")
  option_parser.add_option("", "--sources", action="store_true",
                           help="show expected result file path for each test "
                                "(implies --verbose)")
  option_parser.add_option("", "--startup-dialog", action="store_true",
                           default=False, 
                           help="create a dialog on test_shell.exe startup")
  option_parser.add_option("", "--winhttp", action="store_true",
                           default=False, 
                           help="Use WinHTTP stack")
  option_parser.add_option("", "--test-list", action="append",
                           help="read list of tests to run from file",
                           metavar="FILE")
  options, args = option_parser.parse_args()
  main(options, args)
