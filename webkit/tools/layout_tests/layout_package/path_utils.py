# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Some utility methods for getting paths used by run_webkit_tests.py.
"""

import errno
import os
import platform_utils
import subprocess
import sys

import google.path_utils


class PathNotFound(Exception): pass

# Save some paths here so we don't keep re-evaling.
_webkit_root = None
_layout_data_dir = None
_layout_tests_dir = None
# A map from platform description to directory list.
_platform_results_dirs = {}

# An instance of the PlatformUtility for use by methods mapped from there.
_platform_util = None

# TODO this should probably be moved into path_utils as ToUnixPath().
def WinPathToUnix(path):
  """Convert a windows path to use unix-style path separators (a/b/c)."""
  return path.replace('\\', '/')

def WebKitRoot():
  """Returns the full path to the directory containing webkit.gyp.  Raises
  PathNotFound if we're unable to find webkit.gyp."""
  global _webkit_root
  if _webkit_root:
    return _webkit_root
  webkit_gyp_path = google.path_utils.FindUpward(google.path_utils.ScriptDir(),
                                                 'webkit.gyp')
  _webkit_root = os.path.dirname(webkit_gyp_path)
  return _webkit_root

def LayoutDataDir():
  """Gets the full path to the tests directory.  Raises PathNotFound if
  we're unable to find it."""
  global _layout_data_dir
  if _layout_data_dir:
    return _layout_data_dir
  _layout_data_dir = google.path_utils.FindUpward(WebKitRoot(), 'webkit',
                                                  'data', 'layout_tests')
  return _layout_data_dir

def LayoutTestsDir(path = None):
  """Returns the full path to the directory containing layout tests, based on
  the supplied relative or absolute path to a layout tests. If the path contains
  "LayoutTests" directory, locates this directory, assuming it's either in
  in webkit/data/layout_tests or in third_party/WebKit."""

  if path != None and path.find('LayoutTests') == -1:
    return LayoutDataDir()

  global _layout_tests_dir
  if _layout_tests_dir:
    return _layout_tests_dir

  webkit_dir = google.path_utils.FindUpward(
      google.path_utils.ScriptDir(), 'third_party', 'WebKit')

  if os.path.exists(os.path.join(webkit_dir, 'LayoutTests')):
    _layout_tests_dir = webkit_dir
  else:
    _layout_tests_dir = LayoutDataDir()

  return _layout_tests_dir

def ChromiumPlatformResultsEnclosingDir():
  """Returns the full path to the directory containing Chromium platform
  result directories.
  """
  # TODO(pamg): Once we move platform/chromium-* into LayoutTests/platform/,
  # remove this and use PlatformResultsEnclosingDir() for everything.
  return os.path.join(LayoutDataDir(), 'platform')

def WebKitPlatformResultsEnclosingDir():
  """Gets the full path to just above the platform results directory."""
  return os.path.join(LayoutTestsDir(), 'LayoutTests', 'platform')

def PlatformResultsEnclosingDir(platform):
  """Gets the path to just above the results directory for this platform."""
  if platform.startswith('chromium'):
    return ChromiumPlatformResultsEnclosingDir()
  return WebKitPlatformResultsEnclosingDir()

def GetPlatformDirs(platform):
  """Returns a list of directories to look in for test results.

  The result will be sought in the chromium-specific platform directories,
  in the corresponding WebKit platform directories, in the WebKit platform/mac/
  directory, and finally next to the test file.

  Suppose that the |platform| is 'chromium-win-xp'.  In that case, the
  following directories are searched in order, if they exist, and the first
  match is returned:
    LayoutTests/platform/chromium-win-xp/
    LayoutTests/platform/chromium-win/
    LayoutTests/platform/chromium/
    LayoutTests/platform/win-xp/
    LayoutTests/platform/win/
    LayoutTests/platform/mac/
    the directory in which the test itself is located

  If the |platform| is 'chromium-mac-leopard', the sequence will be as follows:
    LayoutTests/platform/chromium-mac-leopard/
    LayoutTests/platform/chromium-mac/
    LayoutTests/platform/chromium/
    LayoutTests/platform/mac-leopard/
    LayoutTests/platform/mac/
    the directory in which the test itself is located

  A platform may optionally fall back to the Windows results if its own
  results are not found, by returning True from its platform-specific
  platform_utils.IsNonWindowsPlatformTargettingWindowsResults(). Supposing
  that Linux does so, the search sequence for the |platform| 'chromium-linux'
  will be
    LayoutTests/platform/chromium-linux/
    LayoutTests/platform/chromium/
    LayoutTests/platform/linux/
    LayoutTests/platform/chromium-win/
    LayoutTests/platform/win/
    LayoutTests/platform/mac/
    the directory in which the test itself is located

  Args:
    platform: a hyphen-separated list of platform descriptors from least to
        most specific, matching the WebKit format, that will be used to find
        the platform/ directories to look in. For example, 'chromium-win' or
        'chromium-mac-leopard'.
  Returns:
    a list of directories to search
  """
  # Use the cached directory list if we have one.
  global _platform_results_dirs
  platform_dirs = _platform_results_dirs.get(platform, [])
  if len(platform_dirs) == 0:
    # Build the list of platform directories: chromium-foo-bar, chromium-foo,
    # chromium.
    segments = platform.split('-')
    for length in range(len(segments), 0, -1):
      platform_dirs.append('-'.join(segments[:length]))

    # Append corresponding WebKit platform directories too.
    if platform.startswith('chromium-'):
      for length in range(len(segments), 1, -1):
        platform_dirs.append('-'.join(segments[1:length]))

    if platform_utils.IsNonWindowsPlatformTargettingWindowsResults():
      if platform.startswith('chromium'):
        platform_dirs.append('chromium-win')
      platform_dirs.append('win')

    # Finally, append platform/mac/ to all searches.
    if 'mac' not in platform_dirs:
      platform_dirs.append('mac')

    platform_dirs = [os.path.join(PlatformResultsEnclosingDir(x), x)
                     for x in platform_dirs]
    _platform_results_dirs[platform] = platform_dirs

  return platform_dirs

def ExpectedBaseline(filename, suffix, platform):
  """Given a test name, finds where the baseline result is located. The
  result is returned as a pair of values, the absolute path to top of the test
  results directory, and the relative path from there to the results file.

  Both return values will be in the format appropriate for the
  current platform (e.g., "\\" for path separators on Windows).

  If the results file is not found, then None will be returned for the
  directory, but the expected relative pathname will still be returned.

  Args:
     filename: absolute filename to test file
     suffix: file suffix of the expected results, including dot; e.g. '.txt'
         or '.png'.  This should not be None, but may be an empty string.
     platform: the most-specific directory name to use to build the
         search list of directories, e.g., 'chromium-win', or
         'chromium-mac-leopard' (we follow the WebKit format)
  Returns
     platform_dir - abs path to the top of the results tree (or test tree),
         or None if the file doesn't exist.
     results_filename - relative path from top of tree to the results file
         (os.path.join of the two gives you the full path to the file, unless
          None was returned.)
  """
  testname = os.path.splitext(RelativeTestFilename(filename))[0]

  # While we still have tests in LayoutTests/, chrome/, and pending/, we need
  # to strip that outer directory.
  # TODO(pamg): Once we upstream all of chrome/ and pending/, clean this up.
  platform_filename = testname + '-expected' + suffix
  testdir, base_filename = platform_filename.split('/', 1)

  platform_dirs = GetPlatformDirs(platform)
  for platform_dir in platform_dirs:
    # TODO(pamg): Clean this up once we upstream everything in chrome/ and
    # pending/.
    if os.path.basename(platform_dir).startswith('chromium'):
      if os.path.exists(os.path.join(platform_dir, platform_filename)):
        return platform_dir, platform_filename
    else:
      if os.path.exists(os.path.join(platform_dir, base_filename)):
        return platform_dir, base_filename

  # If it wasn't found in a platform directory, return the expected result
  # in the test directory, even if no such file actually exists.
  platform_dir = LayoutTestsDir(filename)
  if os.path.exists(os.path.join(platform_dir, platform_filename)):
    return platform_dir, platform_filename
  return None, platform_filename

def ExpectedFilename(filename, suffix, platform):
  """Given a test name, returns an absolute path to its expected results.

  If no expected results are found in any of the searched directories, the
  directory in which the test itself is located will be returned. The return
  value is in the format appropriate for the platform (e.g., "\\" for
  path separators on windows).

  Args:
     filename: absolute filename to test file
     suffix: file suffix of the expected results, including dot; e.g. '.txt'
         or '.png'.  This should not be None, but may be an empty string.
     platform: the most-specific directory name to use to build the
         search list of directories, e.g., 'chromium-win', or
         'chromium-mac-leopard' (we follow the WebKit format)
  """
  (platform_dir, platform_filename) = ExpectedBaseline(filename, suffix,
                                                      platform)
  if platform_dir:
    return os.path.join(platform_dir, platform_filename)
  return os.path.join(LayoutTestsDir(filename), platform_filename)

def ImageDiffBinaryPath(target):
  """Gets the full path to the image_diff binary for the target build
  configuration. Raises PathNotFound if the file doesn't exist"""
  platform_util = platform_utils.PlatformUtility('')
  full_path = os.path.join(WebKitRoot(), target,
                           platform_util.ImageDiffBinary())
  if not os.path.exists(full_path):
    # try output directory from either Xcode or chrome.sln
    full_path = platform_util.ImageDiffBinaryPath(target)
  if not os.path.exists(full_path):
    raise PathNotFound('unable to find image_diff at %s' % full_path)
  return full_path

def TestShellBinaryPath(target):
  """Gets the full path to the test_shell binary for the target build
  configuration. Raises PathNotFound if the file doesn't exist"""
  platform_util = platform_utils.PlatformUtility('')
  full_path = os.path.join(WebKitRoot(), target,
                           platform_util.TestShellBinary())
  if not os.path.exists(full_path):
    # try output directory from either Xcode or chrome.sln
    full_path = platform_util.TestShellBinaryPath(target)
  if not os.path.exists(full_path):
    raise PathNotFound('unable to find test_shell at %s' % full_path)
  return full_path

def LayoutTestHelperBinaryPath(target):
  """Gets the full path to the layout test helper binary for the target build
  configuration, if it is needed, or an empty string if it isn't.

  Raises PathNotFound if it is needed and the file doesn't exist"""
  platform_util = platform_utils.PlatformUtility('')
  if len(platform_util.LayoutTestHelperBinary()):
    full_path = os.path.join(WebKitRoot(), target,
                             platform_util.LayoutTestHelperBinary())
    if not os.path.exists(full_path):
      # try output directory from either Xcode or chrome.sln
      full_path = platform_util.LayoutTestHelperBinaryPath(target)
      if len(full_path) and not os.path.exists(full_path):
        raise PathNotFound('unable to find layout_test_helper at %s' %
                           full_path)
    return full_path
  else:
    return ''

def RelativeTestFilename(filename):
  """Provide the filename of the test relative to the layout data
  directory as a unix style path (a/b/c)."""
  return WinPathToUnix(filename[len(LayoutTestsDir(filename)) + 1:])

def GetPlatformUtil():
  """Returns a singleton instance of the PlatformUtility."""
  global _platform_util
  if not _platform_util:
    # Avoid circular import by delaying it.
    import layout_package.platform_utils
    _platform_util = (
        layout_package.platform_utils.PlatformUtility(WebKitRoot()))
  return _platform_util

# Map platform specific path utility functions.  We do this as a convenience
# so importing path_utils will get all path related functions even if they are
# platform specific.
def GetAbsolutePath(path):
  return GetPlatformUtil().GetAbsolutePath(path)

def FilenameToUri(path):
  return GetPlatformUtil().FilenameToUri(path)

def TestListPlatformDir():
  return GetPlatformUtil().TestListPlatformDir()

def PlatformDir():
  return GetPlatformUtil().PlatformDir()

def PlatformNewResultsDir():
  return GetPlatformUtil().PlatformNewResultsDir()
