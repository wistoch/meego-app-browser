# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is the Linux implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import path_utils
import subprocess
import sys

def PlatformName():
  """Returns the name of the platform we're currently running on."""
  # We're not ready for version-specific results yet. When we uncomment
  # this, we also need to add it to the BaselineSearchPath()
  return 'chromium-win' + PlatformVersion()

def PlatformVersion():
  """Returns the version string for the platform, e.g. '-vista' or
  '-snowleopard'. If the platform does not distinguish between
  minor versions, it returns ''."""
  winver = sys.getwindowsversion()
  if winver[0] == 6 and (winver[1] == 0):
    return '-vista'
  elif winver[0] == 5 and (winver[1] == 1 or winver[1] == 2):
    return '-xp'
  return ''

def BaselineSearchPath(all_versions=False):
  """Returns the list of directories to search for baselines/results, in
  order of preference. Paths are relative to the top of the source tree.

  If all_versions is True, returns the full list of search directories
  for all versions instead of the current version that the script
  is running on. This is for case that the platform or version of
  the search paths is different from the platform or version that the
  script is running on. For example, the rebaseline tool may run on Mac,
  Linux or Windows Vista to rebaseline for Windows XP, in which case,
  it gets a full list, finds the rebaselining dir (XP) and compares
  the XP baseline with fallback baselines.
  """

  dirs = []
  if all_versions or PlatformVersion() in ("-vista", "-xp"):
    dirs.append(path_utils.ChromiumBaselinePath('chromium-win-vista'))
  if all_versions or PlatformVersion() == "-xp":
    dirs.append(path_utils.ChromiumBaselinePath('chromium-win-xp'))
  dirs.append(path_utils.ChromiumBaselinePath('chromium-win'))
  dirs.append(path_utils.WebKitBaselinePath('win'))
  dirs.append(path_utils.WebKitBaselinePath('mac'))
  return dirs

def WDiffPath():
  """Path to the WDiff executable, whose binary is checked in on Win"""
  return path_utils.PathFromBase('third_party', 'cygwin', 'bin', 'wdiff.exe')

def ImageDiffPath(target):
  """Return the platform-specific binary path for the image compare util.
       We use this if we can't find the binary in the default location
       in path_utils.

  Args:
    target: Build target mode (debug or release)
  """
  return _FindBinary(target, 'image_diff.exe')

def LayoutTestHelperPath(target):
  """Return the platform-specific binary path for the layout test helper.
  We use this if we can't find the binary in the default location
  in path_utils.

  Args:
    target: Build target mode (debug or release)
  """
  return _FindBinary(target, 'layout_test_helper.exe')

def TestShellPath(target):
  """Return the platform-specific binary path for our TestShell.
     We use this if we can't find the binary in the default location
     in path_utils.

  Args:
    target: Build target mode (debug or release)
  """
  return _FindBinary(target, 'test_shell.exe')

def LigHTTPdExecutablePath():
  """Returns the executable path to start LigHTTPd"""
  return path_utils.PathFromBase('third_party', 'lighttpd', 'win',
                                 'LightTPD.exe')

def LigHTTPdModulePath():
  """Returns the library module path for LigHTTPd"""
  return path_utils.PathFromBase('third_party', 'lighttpd', 'win', 'lib')

def LigHTTPdPHPPath():
  """Returns the PHP executable path for LigHTTPd"""
  return path_utils.PathFromBase('third_party', 'lighttpd', 'win', 'php5',
                                 'php-cgi.exe')

def ShutDownHTTPServer(server_process):
  """Shut down the lighttpd web server. Blocks until it's fully shut down.

  Args:
    server_process: The subprocess object representing the running server.
        Unused in this implementation of the method.
  """
  subprocess.Popen(('taskkill.exe', '/f', '/im', 'LightTPD.exe'),
                   stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE).wait()

#
# Private helper functions.
#

def _FindBinary(target, binary):
  """On Windows, we look for binaries that we compile in potentially
  two places: src/webkit/$target (preferably, which we get if we
  built using webkit.gyp), or src/chrome/$target (if compiled some other
  way)."""
  try:
    return path_utils.PathFromBase('webkit', target, binary)
  except path_utils.PathNotFound:
    return path_utils.PathFromBase('chrome', target, binary)
