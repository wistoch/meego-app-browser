#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
appid.py -- Chromium appid header file generation utility.
"""

import optparse
import sys

GENERATED_APPID_INCLUDE_FILE_CONTENTS = """
// This file is automatically generated by appid.py.
// It contains the Google Update Appid used for this build. Note that
// the Appid will be empty for non Google Chrome builds.
namespace google_update {
const wchar_t kChromeGuid[] = L"%s";
}
"""

def GenerateAppIdHeader(opts):
  contents = GENERATED_APPID_INCLUDE_FILE_CONTENTS % opts.appid

  output_file = open(opts.output_file, 'w')
  try:
    output_file.write(contents)
  finally:
    output_file.close()

def main():
  parser = optparse.OptionParser()
  parser.add_option('-a', '--appid',
                    help='The Google Update App Id of the Chrome being built.')
  parser.add_option('-o', '--output_file',
                    help='The path to the generated output header file')

  (opts, args) = parser.parse_args()

  if opts.appid is None or not opts.output_file:
    parser.print_help()
    return 1

  # Log a trace in the build output when we run.
  print "Generating appid header... ",
  GenerateAppIdHeader(opts)

  print "Done."


if __name__ == '__main__':
  sys.exit(main())
