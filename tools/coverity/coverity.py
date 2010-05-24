#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Runs Coverity Prevent on a build of Chromium.

This script should be run in a Visual Studio Command Prompt, so that the
INCLUDE, LIB, and PATH environment variables are set properly for Visual
Studio.

Usage examples:
  coverity.py
  coverity.py --dry-run
  coverity.py --target=debug
  %comspec% /c ""C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat"
      x86 && C:\Python24\python.exe C:\coverity.py"

For a full list of options, pass the '--help' switch.

See http://support.microsoft.com/kb/308569 for running this script as a
Scheduled Task on Windows XP.

"""

import optparse
import os
import subprocess
import sys
import time

# TODO(wtc): Change these constants to command-line flags, particularly the
# ones that are paths.  Set default values for the flags.

CHROMIUM_SOURCE_DIR = 'C:\\chromium.latest'

# Relative to CHROMIUM_SOURCE_DIR.
CHROMIUM_SOLUTION_FILE = 'src\\chrome\\chrome.sln'

# Relative to CHROMIUM_SOURCE_DIR.
CHROMIUM_SOLUTION_DIR = 'src\\chrome'

COVERITY_BIN_DIR = 'C:\\coverity\\prevent-win32-4.5.1\\bin'

COVERITY_INTERMEDIATE_DIR = 'C:\\coverity\\cvbuild\\cr_int'

COVERITY_ANALYZE_OPTIONS = ('--cxx --security --concurrency '
                            '--enable ATOMICITY '
                            '--enable MISSING_LOCK '
                            '--enable DELETE_VOID '
                            '--checker-option PASS_BY_VALUE:size_threshold:16 '
                            '--checker-option '
                            'USE_AFTER_FREE:allow_simple_use:false '
                            '--enable-constraint-fpp '
                            '--enable-callgraph-metrics')

# Might need to be changed to FQDN
COVERITY_REMOTE = 'chromecoverity-linux1'

COVERITY_PORT = '5467'

COVERITY_PRODUCT = 'Chromium'

COVERITY_TARGET = 'Windows'

COVERITY_USER = 'admin'

CHROMIUM_PASSWORD = 'xxxxxxxx'

# Relative to CHROMIUM_SOURCE_DIR.  Contains the pid of this script.
LOCK_FILE = 'coverity.lock'

def _RunCommand(cmd, dry_run, shell=False):
  """Runs the command if dry_run is false, otherwise just prints the command."""
  print cmd
  # TODO(wtc): Check the return value of subprocess.call, which is the return
  # value of the command.
  if not dry_run:
    subprocess.call(cmd, shell=shell)

def main(options, args):
  """Runs all the selected tests for the given build type and target."""
  # Create the lock file to prevent another instance of this script from
  # running.
  lock_filename = '%s\\%s' % (CHROMIUM_SOURCE_DIR, LOCK_FILE)
  try:
    lock_file = os.open(lock_filename,
                        os.O_CREAT | os.O_EXCL | os.O_TRUNC | os.O_RDWR)
  except OSError, err:
    print 'Failed to open lock file:\n  ' + str(err)
    return 1

  # Write the pid of this script (the python.exe process) to the lock file.
  os.write(lock_file, str(os.getpid()))

  options.target = options.target.title()

  start_time = time.time()

  print 'Change directory to ' + CHROMIUM_SOURCE_DIR
  os.chdir(CHROMIUM_SOURCE_DIR)

  cmd = 'gclient sync'
  _RunCommand(cmd, options.dry_run, shell=True)
  print 'Elapsed time: %ds' % (time.time() - start_time)

  # Do a clean build.  Remove the build output directory first.
  # TODO(wtc): Consider using Python's rmtree function in the shutil module,
  # or the RemoveDirectory function in
  # trunk/tools/buildbot/scripts/common/chromium_utils.py.
  cmd = 'rmdir /s /q %s\\%s\\%s' % (CHROMIUM_SOURCE_DIR,
                                    CHROMIUM_SOLUTION_DIR, options.target)
  _RunCommand(cmd, options.dry_run, shell=True)
  print 'Elapsed time: %ds' % (time.time() - start_time)

  cmd = '%s\\cov-build.exe --dir %s devenv.com %s\\%s /build %s' % (
      COVERITY_BIN_DIR, COVERITY_INTERMEDIATE_DIR, CHROMIUM_SOURCE_DIR,
      CHROMIUM_SOLUTION_FILE, options.target)
  _RunCommand(cmd, options.dry_run)
  print 'Elapsed time: %ds' % (time.time() - start_time)

  cmd = '%s\\cov-analyze.exe --dir %s %s' % (COVERITY_BIN_DIR,
                                             COVERITY_INTERMEDIATE_DIR,
                                             COVERITY_ANALYZE_OPTIONS)
  _RunCommand(cmd, options.dry_run)
  print 'Elapsed time: %ds' % (time.time() - start_time)

  cmd = ('%s\\cov-commit-defects.exe --dir %s --remote %s --port %s'
         '--product %s '
         '--target %s '
         '--user %s '
         '--password %s') % (COVERITY_BIN_DIR,
                                   COVERITY_INTERMEDIATE_DIR,
                                   COVERITY_REMOTE, COVERITY_PORT,
                                   COVERITY_PRODUCT,
                                   COVERITY_TARGET, COVERITY_USER,
                                   CHROMIUM_PASSWORD)
  _RunCommand(cmd, options.dry_run)

  print 'Total time: %ds' % (time.time() - start_time)

  os.close(lock_file)
  os.remove(lock_filename)

  return 0

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--dry-run', action='store_true', default=False,
                           help='print but don\'t run the commands')
  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  options, args = option_parser.parse_args()

  result = main(options, args)
  sys.exit(result)
