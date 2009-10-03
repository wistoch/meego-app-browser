#!/usr/bin/env python
#
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Tests the text output of Google C++ Mocking Framework.

SYNOPSIS
       gmock_output_test.py --gmock_build_dir=BUILD/DIR --gengolden
         # where BUILD/DIR contains the built gmock_output_test_ file.
       gmock_output_test.py --gengolden
       gmock_output_test.py
"""

__author__ = 'wan@google.com (Zhanyong Wan)'

import gmock_test_utils
import os
import re
import string
import sys
import unittest


# The flag for generating the golden file
GENGOLDEN_FLAG = '--gengolden'

IS_WINDOWS = os.name == 'nt'

if IS_WINDOWS:
  PROGRAM = r'..\build.dbg\gmock_output_test_.exe'
else:
  PROGRAM = 'gmock_output_test_'

PROGRAM_PATH = os.path.join(gmock_test_utils.GetBuildDir(), PROGRAM)
COMMAND = PROGRAM_PATH + ' --gtest_stack_trace_depth=0 --gtest_print_time=0'
GOLDEN_NAME = 'gmock_output_test_golden.txt'
GOLDEN_PATH = os.path.join(gmock_test_utils.GetSourceDir(),
                           GOLDEN_NAME)


def ToUnixLineEnding(s):
  """Changes all Windows/Mac line endings in s to UNIX line endings."""

  return s.replace('\r\n', '\n').replace('\r', '\n')


def RemoveReportHeaderAndFooter(output):
  """Removes Google Test result report's header and footer from the output."""

  output = re.sub(r'.*gtest_main.*\n', '', output)
  output = re.sub(r'\[.*\d+ tests.*\n', '', output)
  output = re.sub(r'\[.* test environment .*\n', '', output)
  output = re.sub(r'\[=+\] \d+ tests .* ran.*', '', output)
  output = re.sub(r'.* FAILED TESTS\n', '', output)
  return output


def RemoveLocations(output):
  """Removes all file location info from a Google Test program's output.

  Args:
       output:  the output of a Google Test program.

  Returns:
       output with all file location info (in the form of
       'DIRECTORY/FILE_NAME:LINE_NUMBER: 'or
       'DIRECTORY\\FILE_NAME(LINE_NUMBER): ') replaced by
       'FILE:#: '.
  """

  return re.sub(r'.*[/\\](.+)(\:\d+|\(\d+\))\:', 'FILE:#:', output)


def NormalizeErrorMarker(output):
  """Normalizes the error marker, which is different on Windows vs on Linux."""

  return re.sub(r' error: ', ' Failure\n', output)


def RemoveMemoryAddresses(output):
  """Removes memory addresses from the test output."""

  return re.sub(r'@\w+', '@0x#', output)


def RemoveTestNamesOfLeakedMocks(output):
  """Removes the test names of leaked mock objects from the test output."""

  return re.sub(r'\(used in test .+\) ', '', output)


def GetLeakyTests(output):
  """Returns a list of test names that leak mock objects."""

  # findall() returns a list of all matches of the regex in output.
  # For example, if '(used in test FooTest.Bar)' is in output, the
  # list will contain 'FooTest.Bar'.
  return re.findall(r'\(used in test (.+)\)', output)


def GetNormalizedOutputAndLeakyTests(output):
  """Normalizes the output of gmock_output_test_.

  Args:
    output: The test output.

  Returns:
    A tuple (the normalized test output, the list of test names that have
    leaked mocks).
  """

  output = ToUnixLineEnding(output)
  output = RemoveReportHeaderAndFooter(output)
  output = NormalizeErrorMarker(output)
  output = RemoveLocations(output)
  output = RemoveMemoryAddresses(output)
  return (RemoveTestNamesOfLeakedMocks(output), GetLeakyTests(output))


def IterShellCommandOutput(cmd, stdin_string=None):
  """Runs a command in a sub-process, and iterates the lines in its STDOUT.

  Args:

    cmd:           The shell command.
    stdin_string:  The string to be fed to the STDIN of the sub-process;
                   If None, the sub-process will inherit the STDIN
                   from the parent process.
  """

  # Spawns cmd in a sub-process, and gets its standard I/O file objects.
  stdin_file, stdout_file = os.popen2(cmd, 'b')

  # If the caller didn't specify a string for STDIN, gets it from the
  # parent process.
  if stdin_string is None:
    stdin_string = sys.stdin.read()

  # Feeds the STDIN string to the sub-process.
  stdin_file.write(stdin_string)
  stdin_file.close()

  while True:
    line = stdout_file.readline()
    if not line:  # EOF
      stdout_file.close()
      break

    yield line


def GetShellCommandOutput(cmd, stdin_string=None):
  """Runs a command in a sub-process, and returns its STDOUT in a string.

  Args:

    cmd:           The shell command.
    stdin_string:  The string to be fed to the STDIN of the sub-process;
                   If None, the sub-process will inherit the STDIN
                   from the parent process.
  """

  lines = list(IterShellCommandOutput(cmd, stdin_string))
  return string.join(lines, '')


def GetNormalizedCommandOutputAndLeakyTests(cmd):
  """Runs a command and returns its normalized output and a list of leaky tests.

  Args:
    cmd:  the shell command.
  """

  # Disables exception pop-ups on Windows.
  os.environ['GTEST_CATCH_EXCEPTIONS'] = '1'
  return GetNormalizedOutputAndLeakyTests(GetShellCommandOutput(cmd, ''))


class GMockOutputTest(unittest.TestCase):
  def testOutput(self):
    (output, leaky_tests) = GetNormalizedCommandOutputAndLeakyTests(COMMAND)
    golden_file = open(GOLDEN_PATH, 'rb')
    golden = golden_file.read()
    golden_file.close()

    # The normalized output should match the golden file.
    self.assertEquals(golden, output)

    # The raw output should contain 2 leaked mock object errors for
    # test GMockOutputTest.CatchesLeakedMocks.
    self.assertEquals(['GMockOutputTest.CatchesLeakedMocks',
                       'GMockOutputTest.CatchesLeakedMocks'],
                      leaky_tests)


if __name__ == '__main__':
  if sys.argv[1:] == [GENGOLDEN_FLAG]:
    (output, _) = GetNormalizedCommandOutputAndLeakyTests(COMMAND)
    golden_file = open(GOLDEN_PATH, 'wb')
    golden_file.write(output)
    golden_file.close()
  else:
    gmock_test_utils.Main()
