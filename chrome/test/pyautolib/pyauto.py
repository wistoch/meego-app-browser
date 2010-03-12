#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto: Python Interface to Chromium's Automation Proxy.

PyAuto uses swig to expose Automation Proxy interfaces to Python.
For complete documentation on the functionality available,
run pydoc on this file.

Ref: http://dev.chromium.org/developers/pyauto
"""

import os
import sys
import unittest

import bookmark_model


def _LocateBinDirs():
  script_dir = os.path.dirname(__file__)
  chrome_src = os.path.join(script_dir, os.pardir, os.pardir, os.pardir)

  bin_dirs = {
      'linux2': [ os.path.join(chrome_src, 'out', 'Debug'),
                  os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                  os.path.join(chrome_src, 'out', 'Release'),
                  os.path.join(chrome_src, 'sconsbuild', 'Release')],
      'darwin': [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
                  os.path.join(chrome_src, 'xcodebuild', 'Release')],
      'win32':  [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release')],
      'cygwin': [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release')],
  }
  sys.path += bin_dirs.get(sys.platform, [])

_LocateBinDirs()

try:
  import pyautolib
  # Needed so that all additional classes (like: FilePath, GURL) exposed by
  # swig interface get available in this module.
  from pyautolib import *
except ImportError:
  print >>sys.stderr, "Could not locate built libraries. Did you build?"
  raise


class PyUITest(pyautolib.PyUITestSuite, unittest.TestCase):
  """Base class for UI Test Cases in Python.

  A browser is created before executing each test, and is destroyed after
  each test irrespective of whether the test passed or failed.

  You should derive from this class and create methods with 'test' prefix,
  and use methods inherited from PyUITestSuite (the C++ side).

  Example:

    class MyTest(PyUITest):

      def testNavigation(self):
        self.NavigateToURL("http://www.google.com")
        self.assertTrue("Google" == self.GetActiveTabTitle())
  """

  def __init__(self, methodName='runTest', extra_chrome_flags=None):
    """Initialize PyUITest.

    When redefining __init__ in a derived class, make sure that:
      o you make a call this __init__
      o __init__ takes methodName as a arg. this is mandated by unittest module

    Args:
      methodName: the default method name. Internal use by unittest module
      extra_chrome_flags: additional flags to pass when launching chrome
    """
    args = sys.argv
    if extra_chrome_flags is not None:
       args.append('--extra-chrome-flags=%s' % extra_chrome_flags)
    pyautolib.PyUITestSuite.__init__(self, args)
    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    os.environ['PATH'] = browser_dir + os.pathsep + os.environ['PATH']
    self.Initialize(pyautolib.FilePath(browser_dir))
    unittest.TestCase.__init__(self, methodName)

  def __del__(self):
    pyautolib.PyUITestSuite.__del__(self)

  def run(self, result=None):
    """The main run method.

    We override this method to make calls to the setup steps in PyUITestSuite.
    """
    self.SetUp()     # Open a browser window
    unittest.TestCase.run(self, result)
    self.TearDown()  # Destroy the browser window

  def GetBookmarkModel(self):
    """Return the bookmark model as a BookmarkModel object.

    This is a snapshot of the bookmark model; it is not a proxy and
    does not get updated as the bookmark model changes.
    """
    return bookmark_model.BookmarkModel(self._GetBookmarksAsJSON())
