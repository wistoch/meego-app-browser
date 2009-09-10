#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script used to scan for server DLLs at build time and build a header
   included by setup.exe. This header contains an array of the names of
   the DLLs that need registering at install time.

"""

import ConfigParser
import glob
import optparse
import os
import sys

CHROME_DIR = "Chrome-bin"
SERVERS_DIR = "servers"
GENERATED_DLL_INCLUDE_FILE_NAME = "registered_dlls.h"
GENERATED_DLL_INCLUDE_FILE_CONTENTS = """
// This file is automatically generated by scan_server_dlls.py.
// It contains the list of COM server dlls that need registering at
// install time.
#include "base/basictypes.h"

namespace {
const wchar_t* kDllsToRegister[] = { %s };
const int kNumDllsToRegister = %d;
}
"""


def Readconfig(output_dir, input_file):
  """Reads config information from input file after setting default value of
  global variabes.
  """
  variables = {}
  variables['ChromeDir'] = CHROME_DIR
  # Use a bogus version number, we don't really care what it is, we just
  # want to find the files that would get picked up from chrome.release,
  # and don't care where the installer archive task ends up putting them.
  variables['VersionDir'] = os.path.join(variables['ChromeDir'],
                                         '0.0.0.0')
  config = ConfigParser.SafeConfigParser(variables)

  print "Reading input_file: " + input_file

  config.read(input_file)
  return config


def CreateRegisteredDllIncludeFile(registered_dll_list, header_output_dir):
  """ Outputs the header file included by the setup project that
  contains the names of the DLLs to be registered at installation
  time.
  """
  output_file = os.path.join(header_output_dir, GENERATED_DLL_INCLUDE_FILE_NAME)

  dll_array_string = ""
  for dll in registered_dll_list:
    dll.replace("\\", "\\\\")
    if dll_array_string:
      dll_array_string += ', '
    dll_array_string += "L\"%s\"" % dll

  if len(registered_dll_list) == 0:
    contents = GENERATED_DLL_INCLUDE_FILE_CONTENTS % ("L\"\"", 0)
  else:
    contents = GENERATED_DLL_INCLUDE_FILE_CONTENTS % (dll_array_string,
                                                      len(registered_dll_list))

  # Don't rewrite the header file if we don't need to.
  try:
    old_file = open(output_file, 'r')
  except EnvironmentError:
    old_contents = None
  else:
    old_contents = old_file.read()
    old_file.close()

  if contents != old_contents:
    print 'Updating server dll header: ' + str(output_file)
    open(output_file, 'w').write(contents)


def ScanServerDlls(config, distribution, output_dir):
  """Scans for DLLs in the specified section of config that are in the
  subdirectory of output_dir named SERVERS_DIR. Returns a list of only the
  filename components of the paths to all matching DLLs.
  """

  print "Scanning for server DLLs in " + output_dir

  registered_dll_list = []
  ScanDllsInSection(config, 'GENERAL', output_dir, registered_dll_list)
  if distribution:
    if len(distribution) > 1 and distribution[0] == '_':
      distribution = distribution[1:]
    ScanDllsInSection(config, distribution.upper(), output_dir,
                      registered_dll_list)

  return registered_dll_list


def ScanDllsInSection(config, section, output_dir, registered_dll_list):
  """Scans for DLLs in the specified section of config that are in the
  subdirectory of output_dir named SERVERS_DIR. Appends the file name of all
  matching dlls to registered_dll_list.
  """
  for option in config.options(section):
    if option.endswith('dir'):
      continue

    dst = config.get(section, option)
    (x, src_folder) = os.path.split(dst)

    for file in glob.glob(os.path.join(output_dir, option)):
      if option.startswith(SERVERS_DIR):
        (x, file_name) = os.path.split(file)
        if file_name.lower().endswith('.dll'):
          print "Found server DLL file: " + file_name
          registered_dll_list.append(file_name)


def RunSystemCommand(cmd):
  if (os.system(cmd) != 0):
    raise "Error while running cmd: %s" % cmd


def main(options):
  """Main method that reads input file, scans <build_output>\servers for
     matches to files described in the input file. A header file for the
     setup project is then generated.
  """
  config = Readconfig(options.output_dir, options.input_file)
  registered_dll_list = ScanServerDlls(config, options.distribution,
                                       options.output_dir)
  CreateRegisteredDllIncludeFile(registered_dll_list,
                                 options.header_output_dir)


if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('-o', '--output_dir', help='Build Output directory')
  option_parser.add_option('-x', '--header_output_dir',
      help='Location where the generated header file will be placed.')
  option_parser.add_option('-i', '--input_file', help='Input file')
  option_parser.add_option('-d', '--distribution',
      help='Name of Chromium Distribution. Optional.')

  options, args = option_parser.parse_args()
  sys.exit(main(options))
