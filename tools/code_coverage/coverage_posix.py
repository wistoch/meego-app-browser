#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate and process code coverage.

TODO(jrg): rename this from coverage_posix.py to coverage_all.py!

Written for and tested on Mac, Linux, and Windows.  To use this script
to generate coverage numbers, please run from within a gyp-generated
project.

All platforms, to set up coverage:
  cd ...../chromium ; src/tools/gyp/gyp_dogfood -Dcoverage=1 src/build/all.gyp

Run coverage on...
Mac:
  ( cd src/chrome ; xcodebuild -configuration Debug -target coverage )
Linux:
  ( cd src/chrome ; hammer coverage )
  # In particular, don't try and run 'coverage' from src/build


--directory=DIR: specify directory that contains gcda files, and where
  a "coverage" directory will be created containing the output html.
  Example name:   ..../chromium/src/xcodebuild/Debug

--genhtml: generate html output.  If not specified only lcov is generated.

--all_unittests: if present, run all files named *_unittests that we
  can find.

--fast_test: make the tests run real fast (just for testing)

--strict: if a test fails, we continue happily.  --strict will cause
  us to die immediately.

--trim=False: by default we trim away tests known to be problematic on
  specific platforms.  If set to false we do NOT trim out tests.

--xvfb=True: By default we use Xvfb to make sure DISPLAY is valid
  (Linux only).  if set to False, do not use Xvfb.  TODO(jrg): convert
  this script from the compile stage of a builder to a
  RunPythonCommandInBuildDir() command to avoid the need for this
  step.

Strings after all options are considered tests to run.  Test names
have all text before a ':' stripped to help with gyp compatibility.
For example, ../base/base.gyp:base_unittests is interpreted as a test
named "base_unittests".
"""

import glob
import logging
import optparse
import os
import shutil
import subprocess
import sys
import time
import traceback

class Coverage(object):
  """Doitall class for code coverage."""

  def __init__(self, directory, options, args):
    super(Coverage, self).__init__()
    logging.basicConfig(level=logging.DEBUG)
    self.directory = directory
    self.options = options
    self.args = args
    self.directory_parent = os.path.dirname(self.directory)
    self.output_directory = os.path.join(self.directory, 'coverage')
    if not os.path.exists(self.output_directory):
      os.mkdir(self.output_directory)
    # The "final" lcov-format file
    self.coverage_info_file = os.path.join(self.directory, 'coverage.info')
    # If needed, an intermediate VSTS-format file
    self.vsts_output = os.path.join(self.directory, 'coverage.vsts')
    # Needed for Windows.
    self.src_root = options.src_root
    self.FindPrograms()
    self.ConfirmPlatformAndPaths()
    self.tests = []
    self.xvfb_pid = 0

  def FindInPath(self, program):
    """Find program in our path.  Return abs path to it, or None."""
    if not 'PATH' in os.environ:
      logging.fatal('No PATH environment variable?')
      sys.exit(1)
    paths = os.environ['PATH'].split(os.pathsep)
    for path in paths:
      fullpath = os.path.join(path, program)
      if os.path.exists(fullpath):
        return fullpath
    return None

  def FindPrograms(self):
    """Find programs we may want to run."""
    if self.IsPosix():
      self.lcov_directory = os.path.join(sys.path[0],
                                         '../../third_party/lcov/bin')
      self.lcov = os.path.join(self.lcov_directory, 'lcov')
      self.mcov = os.path.join(self.lcov_directory, 'mcov')
      self.genhtml = os.path.join(self.lcov_directory, 'genhtml')
      self.programs = [self.lcov, self.mcov, self.genhtml]
    else:
      # Hack to get the buildbot working.
      os.environ['PATH'] += r';c:\coverage\coverage_analyzer'
      os.environ['PATH'] += r';c:\coverage\performance_tools'
      # (end hack)
      commands = ['vsperfcmd.exe', 'vsinstr.exe', 'coverage_analyzer.exe']
      self.perf = self.FindInPath('vsperfcmd.exe')
      self.instrument = self.FindInPath('vsinstr.exe')
      self.analyzer = self.FindInPath('coverage_analyzer.exe')
      if not self.perf or not self.instrument or not self.analyzer:
        logging.fatal('Could not find Win performance commands.')
        logging.fatal('Commands needed in PATH: ' + str(commands))
        sys.exit(1)
      self.programs = [self.perf, self.instrument, self.analyzer]

  def FindTests(self):
    """Find unit tests to run; set self.tests to this list.

    Assume all non-option items in the arg list are tests to be run.
    """
    # Small tests: can be run in the "chromium" directory.
    # If asked, run all we can find.
    if self.options.all_unittests:
      self.tests += glob.glob(os.path.join(self.directory, '*_unittests'))

    # If told explicit tests, run those (after stripping the name as
    # appropriate)
    for testname in self.args:
      if ':' in testname:
        self.tests += [os.path.join(self.directory, testname.split(':')[1])]
      else:
        self.tests += [os.path.join(self.directory, testname)]
    # Medium tests?
    # Not sure all of these work yet (e.g. page_cycler_tests)
    # self.tests += glob.glob(os.path.join(self.directory, '*_tests'))

    # If needed, append .exe to tests since vsinstr.exe likes it that
    # way.
    if self.IsWindows():
      for ind in range(len(self.tests)):
        test = self.tests[ind]
        test_exe = test + '.exe'
        if not test.endswith('.exe') and os.path.exists(test_exe):
          self.tests[ind] = test_exe

  def TrimTests(self):
    """Trim specific tests for each platform."""
    if self.IsWindows():
      return
      # TODO(jrg): remove when not needed
      inclusion = ['unit_tests']
      keep = []
      for test in self.tests:
        for i in inclusion:
          if i in test:
            keep.append(test)
      self.tests = keep
      logging.info('After trimming tests we have ' + ' '.join(self.tests))
      return
    if self.IsLinux():
      return
    if self.IsMac():
      exclusion = ['automated_ui_tests']
      punted = []
      for test in self.tests:
        for e in exclusion:
          if test.endswith(e):
            punted.append(test)
      self.tests = filter(lambda t: t not in punted, self.tests)
      if punted:
        logging.info('Tests trimmed out: ' + str(punted))

  def ConfirmPlatformAndPaths(self):
    """Confirm OS and paths (e.g. lcov)."""
    for program in self.programs:
      if not os.path.exists(program):
        logging.fatal('Program missing: ' + program)
        sys.exit(1)

  def Run(self, cmdlist, ignore_error=False, ignore_retcode=None,
          explanation=None):
    """Run the command list; exit fatally on error."""
    logging.info('Running ' + str(cmdlist))
    retcode = subprocess.call(cmdlist)
    if retcode:
      if ignore_error or retcode == ignore_retcode:
        logging.warning('COVERAGE: %s unhappy but errors ignored  %s' %
                        (str(cmdlist), explanation or ''))
      else:
        logging.fatal('COVERAGE:  %s failed; return code: %d' %
                      (str(cmdlist), retcode))
        sys.exit(retcode)


  def IsPosix(self):
    """Return True if we are POSIX."""
    return self.IsMac() or self.IsLinux()

  def IsMac(self):
    return sys.platform == 'darwin'

  def IsLinux(self):
    return sys.platform == 'linux2'

  def IsWindows(self):
    """Return True if we are Windows."""
    return sys.platform in ('win32', 'cygwin')

  def ClearData(self):
    """Clear old gcda files and old coverage info files."""
    if os.path.exists(self.coverage_info_file):
      os.remove(self.coverage_info_file)
    if self.IsPosix():
      subprocess.call([self.lcov,
                       '--directory', self.directory_parent,
                       '--zerocounters'])
      shutil.rmtree(os.path.join(self.directory, 'coverage'))

  def BeforeRunOneTest(self, testname):
    """Do things before running each test."""
    if not self.IsWindows():
      return
    # Stop old counters if needed
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist, ignore_error=True)
    # Instrument binaries
    for fulltest in self.tests:
      if os.path.exists(fulltest):
        cmdlist = [self.instrument, '/COVERAGE', fulltest]
        self.Run(cmdlist, ignore_retcode=4,
                 explanation='OK with a multiple-instrument')
    # Start new counters
    cmdlist = [self.perf, '-start:coverage', '-output:' + self.vsts_output]
    self.Run(cmdlist)

  def BeforeRunAllTests(self):
    """Called right before we run all tests."""
    if self.IsLinux() and self.options.xvfb:
      self.StartXvfb()

  def RunTests(self):
    """Run all unit tests and generate appropriate lcov files."""
    self.BeforeRunAllTests()
    for fulltest in self.tests:
      if not os.path.exists(fulltest):
        logging.info(fulltest + ' does not exist')
        if self.options.strict:
          sys.exit(2)
      else:
        logging.info('%s path exists' % fulltest)
      cmdlist = [fulltest, '--gtest_print_time']

      # If asked, make this REAL fast for testing.
      if self.options.fast_test:
        # cmdlist.append('--gtest_filter=RenderWidgetHost*')
        cmdlist.append('--gtest_filter=CommandLine*')

      self.BeforeRunOneTest(fulltest)
      logging.info('Running test ' + str(cmdlist))
      try:
        retcode = subprocess.call(cmdlist)
      except:  # can't "except WindowsError" since script runs on non-Windows
        logging.info('EXCEPTION while running a unit test')
        logging.info(traceback.format_exc())
        retcode = 999
      self.AfterRunOneTest(fulltest)

      if retcode:
        logging.info('COVERAGE: test %s failed; return code: %d.' %
                      (fulltest, retcode))
        if self.options.strict:
          logging.fatal('Test failure is fatal.')
          sys.exit(retcode)
    self.AfterRunAllTests()

  def AfterRunOneTest(self, testname):
    """Do things right after running each test."""
    if not self.IsWindows():
      return
    # Stop counters
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist)
    full_output = self.vsts_output + '.coverage'
    shutil.move(full_output, self.vsts_output)
    # generate lcov!
    self.GenerateLcovWindows(testname)

  def AfterRunAllTests(self):
    """Do things right after running ALL tests."""
    if self.IsPosix():
      # On POSIX we can do it all at once without running out of memory.
      self.GenerateLcovPosix()
    if self.IsLinux() and self.options.xvfb:
      self.StopXvfb()

  def StartXvfb(self):
    """Start Xvfb and set an appropriate DISPLAY environment.  Linux only.

    Copied from http://src.chromium.org/viewvc/chrome/trunk/tools/buildbot/
      scripts/slave/slave_utils.py?view=markup
    with some simplifications (e.g. no need to use xdisplaycheck, save
    pid in var not file, etc)
    """
    logging.info('Xvfb: starting')
    proc = subprocess.Popen(["Xvfb", ":9", "-screen", "0", "1024x768x24",
                             "-ac"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    self.xvfb_pid = proc.pid
    if not self.xvfb_pid:
      logging.info('Could not start Xvfb')
      return
    os.environ['DISPLAY'] = ":9"
    # Now confirm, giving a chance for it to start if needed.
    logging.info('Xvfb: confirming')
    for test in range(10):
      proc = subprocess.Popen('xdpyinfo >/dev/null', shell=True)
      pid, retcode = os.waitpid(proc.pid, 0)
      if retcode == 0:
        break
      time.sleep(0.5)
    if retcode != 0:
      logging.info('Warning: could not confirm Xvfb happiness')
    else:
      logging.info('Xvfb: OK')

  def StopXvfb(self):
    """Stop Xvfb if needed.  Linux only."""
    if self.xvfb_pid:
      logging.info('Xvfb: killing')
      try:
        os.kill(self.xvfb_pid, signal.SIGKILL)
      except:
        pass
      del os.environ['DISPLAY']
      self.xvfb_pid = 0


  def GenerateLcovPosix(self):
    """Convert profile data to lcov."""
    command = [self.mcov,
               '--directory', self.directory_parent,
               '--output', self.coverage_info_file]
    print >>sys.stderr, 'Assembly command: ' + ' '.join(command)
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)

  def GenerateLcovWindows(self, testname=None):
    """Convert VSTS format to lcov.  Appends coverage data to sum file."""
    lcov_file = self.vsts_output + '.lcov'
    if os.path.exists(lcov_file):
      os.remove(lcov_file)
    # generates the file (self.vsts_output + ".lcov")

    cmdlist = [self.analyzer,
               '-sym_path=' + self.directory,
               '-src_root=' + self.src_root,
               self.vsts_output]
    self.Run(cmdlist)
    if not os.path.exists(lcov_file):
      logging.fatal('Output file %s not created' % lcov_file)
      sys.exit(1)
    logging.info('Appending lcov for test %s to %s' %
                 (testname, self.coverage_info_file))
    size_before = 0
    if os.path.exists(self.coverage_info_file):
      size_before = os.stat(self.coverage_info_file).st_size
    src = open(lcov_file, 'r')
    dst = open(self.coverage_info_file, 'a')
    dst.write(src.read())
    src.close()
    dst.close()
    size_after = os.stat(self.coverage_info_file).st_size
    logging.info('Lcov file growth for %s: %d --> %d' %
                 (self.coverage_info_file, size_before, size_after))

  def GenerateHtml(self):
    """Convert lcov to html."""
    # TODO(jrg): This isn't happy when run with unit_tests since V8 has a
    # different "base" so V8 includes can't be found in ".".  Fix.
    command = [self.genhtml,
               self.coverage_info_file,
               '--output-directory',
               self.output_directory]
    print >>sys.stderr, 'html generation command: ' + ' '.join(command)
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)

def main():
  # Print out the args to help someone do it by hand if needed
  print >>sys.stderr, sys.argv

  parser = optparse.OptionParser()
  parser.add_option('-d',
                    '--directory',
                    dest='directory',
                    default=None,
                    help='Directory of unit test files')
  parser.add_option('-a',
                    '--all_unittests',
                    dest='all_unittests',
                    default=False,
                    help='Run all tests we can find (*_unittests)')
  parser.add_option('-g',
                    '--genhtml',
                    dest='genhtml',
                    default=False,
                    help='Generate html from lcov output')
  parser.add_option('-f',
                    '--fast_test',
                    dest='fast_test',
                    default=False,
                    help='Make the tests run REAL fast by doing little.')
  parser.add_option('-s',
                    '--strict',
                    dest='strict',
                    default=False,
                    help='Be strict and die on test failure.')
  parser.add_option('-S',
                    '--src_root',
                    dest='src_root',
                    default='.',
                    help='Source root (only used on Windows)')
  parser.add_option('-t',
                    '--trim',
                    dest='trim',
                    default=True,
                    help='Trim out tests?  Default True.')
  parser.add_option('-x',
                    '--xvfb',
                    dest='xvfb',
                    default=True,
                    help='Use Xvfb for tests?  Default True.')
  (options, args) = parser.parse_args()
  if not options.directory:
    parser.error('Directory not specified')
  coverage = Coverage(options.directory, options, args)
  coverage.ClearData()
  coverage.FindTests()
  if options.trim:
    coverage.TrimTests()
  coverage.RunTests()
  if options.genhtml:
    coverage.GenerateHtml()
  return 0


if __name__ == '__main__':
  sys.exit(main())
