#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to archive layout test results generated by buildbots.

Actual result files (*-actual-*.txt), but not results from simplified diff
tests (*-simp-actual-*.txt) or JS-filtered diff tests (*-jsfilt-*.txt), will
be included in the archive.

When this is run, the current directory (cwd) should be the outer build
directory (e.g., chrome-release/build/).

For a list of command-line options, call this script with '--help'.
"""

import optparse
import os
import socket
import sys

import chromium_config as config
import chromium_utils
import slave_utils

# Directory name, above the build directory, in which test results can be
# found if no --results-dir option is given.
RESULT_DIR = config.Archive.layout_test_result_dir

# The result file is stored in a subdirectory of this one named for the build
# configuration (e.g., 'chrome-release') and version.
DEST_DIR_BASE = config.Archive.layout_test_result_archive


def _CollectArchiveFiles(output_dir):
  """Returns a list of file paths to archive, relative to the output_dir.

  Files in the output_dir or one of its subdirectories, whose names end with
  '-actual-*.txt' but not '-simp-actual-*.txt' or '-jsfilt-actual-*.txt',
  will be included in the list.
  """
  file_list = []
  for (path, dirs, files) in os.walk(output_dir):
    rel_path = path[len(output_dir + '\\'):]
    for file in files:
      if ('-actual-' in file and file.endswith('.txt') and
          '-simp-actual-' not in file and '-jsfilt-actual-' not in file):
        file_list.append(os.path.join(rel_path, file))
  if os.path.exists(os.path.join(output_dir, 'results.html')):
    file_list.append('results.html')
  return file_list


def main(options, args):
  chrome_dir = os.path.abspath(options.build_dir)
  if options.results_dir is not None:
    options.results_dir = os.path.abspath(os.path.join(options.build_dir,
                                                       options.results_dir))
  else:
    options.results_dir = chromium_utils.FindUpward(chrome_dir, RESULT_DIR)
  print 'Archiving results from %s' % options.results_dir
  staging_dir = slave_utils.GetStagingDir(chrome_dir)
  print 'Staging in %s' % staging_dir

  file_list = _CollectArchiveFiles(options.results_dir)
  (zip_dir, zip_file) = chromium_utils.MakeZip(staging_dir,
                                             'layout-test-results',
                                             file_list,
                                             options.results_dir)

  # Extract the build name of this slave (e.g., 'chrome-release') from its
  # configuration file.
  build_name = slave_utils.SlaveBuildName(chrome_dir)

  last_change = str(slave_utils.SubversionRevision(chrome_dir))
  print 'last change: %s' % last_change
  print 'build name: %s' % build_name
  print 'host name: %s' % socket.gethostname()

  dest_dir = os.path.join(DEST_DIR_BASE, build_name, last_change)
  chromium_utils.MaybeMakeDirectory(dest_dir)
  print 'saving results to %s' % dest_dir
  chromium_utils.CopyFileToDir(zip_file, dest_dir)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--build-dir', default='webkit',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')
  option_parser.add_option('', '--results-dir',
                           help='path to layout test results, relative to '
                                'the build_dir')
  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
