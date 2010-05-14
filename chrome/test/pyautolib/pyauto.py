#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto: Python Interface to Chromium's Automation Proxy.

PyAuto uses swig to expose Automation Proxy interfaces to Python.
For complete documentation on the functionality available,
run pydoc on this file.

Ref: http://dev.chromium.org/developers/pyauto


Include the following in your PyAuto test script to make it run standalone.

from pyauto import Main

if __name__ == '__main__':
  Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto.py test_script
"""

import logging
import optparse
import os
import re
import sys
import time
import types
import unittest
import urllib


def _LocateBinDirs():
  """Setup a few dirs where we expect to find dependency libraries."""
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
  deps_dirs = [ os.path.join(script_dir, os.pardir,
                             os.pardir, os.pardir, 'third_party'),
                script_dir,
  ]
  sys.path += bin_dirs.get(sys.platform, []) + deps_dirs

_LocateBinDirs()

try:
  import pyautolib
  # Needed so that all additional classes (like: FilePath, GURL) exposed by
  # swig interface get available in this module.
  from pyautolib import *
except ImportError:
  print >>sys.stderr, "Could not locate built libraries. Did you build?"
  raise

# Should go after sys.path is set appropriately
import bookmark_model
import download_info
import history_info
import omnibox_info
import plugins_info
import prefs_info
from pyauto_errors import JSONInterfaceError
import simplejson as json  # found in third_party


class PyUITest(pyautolib.PyUITestBase, unittest.TestCase):
  """Base class for UI Test Cases in Python.

  A browser is created before executing each test, and is destroyed after
  each test irrespective of whether the test passed or failed.

  You should derive from this class and create methods with 'test' prefix,
  and use methods inherited from PyUITestBase (the C++ side).

  Example:

    class MyTest(PyUITest):

      def testNavigation(self):
        self.NavigateToURL("http://www.google.com")
        self.assertEqual("Google", self.GetActiveTabTitle())
  """

  def __init__(self, methodName='runTest', **kwargs):
    """Initialize PyUITest.

    When redefining __init__ in a derived class, make sure that:
      o you make a call this __init__
      o __init__ takes methodName as an arg. this is mandated by unittest module

    Args:
      methodName: the default method name. Internal use by unittest module

      (The rest of the args can be in any order. They can even be skipped in
       which case the defaults will be used.)

      clear_profile: If True, clean the profile dir before use. Defaults to True
      homepage: the home page. Defaults to "about:blank"
    """
    # Fetch provided keyword args, or fill in defaults.
    clear_profile = kwargs.get('clear_profile', True)
    homepage = kwargs.get('homepage', 'about:blank')

    pyautolib.PyUITestBase.__init__(self, clear_profile, homepage)
    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    self.Initialize(pyautolib.FilePath(browser_dir))
    unittest.TestCase.__init__(self, methodName)

  def __del__(self):
    pyautolib.PyUITestBase.__del__(self)

  def setUp(self):
    """Override this method to launch browser differently.

    Can be used to prevent launching the browser window by default in case a
    test wants to do some additional setup before firing browser.
    """
    self.SetUp()     # Fire browser

  def tearDown(self):
    self.TearDown()  # Destroy browser

  def RestartBrowser(self, clear_profile=True):
    """Restart the browser.

    For use with tests that require to restart the browser.

    Args:
      clear_profile: If True, the browser profile is cleared before restart.
                     Defaults to True, that is restarts browser with a clean
                     profile.
    """
    orig_clear_state = self.get_clear_profile()
    self.CloseBrowserAndServer()
    self.set_clear_profile(clear_profile)
    logging.debug('Restarting browser with clear_profile=%s' %
                  self.get_clear_profile())
    self.LaunchBrowserAndServer()
    self.set_clear_profile(orig_clear_state)  # Reset to original state.

  @staticmethod
  def DataDir():
    """Returns the path to the data dir chrome/test/data."""
    return os.path.join(os.path.dirname(__file__), os.pardir, "data")

  @staticmethod
  def GetFileURLForPath(path):
    """Get file:// url for the given path.

    Also quotes the url using urllib.quote().
    """
    abs_path = os.path.abspath(path)
    if sys.platform == 'win32':
      # Don't quote the ':' in drive letter ( say, C: ) on win.
      # Also, replace '\' with '/' as expected in a file:/// url.
      drive, rest = os.path.splitdrive(abs_path)
      quoted_path = drive.upper() + urllib.quote((rest.replace('\\', '/')))
      return 'file:///' + quoted_path
    else:
      quoted_path = urllib.quote(abs_path)
      return 'file://' + quoted_path

  def WaitUntil(self, function, timeout=-1, retry_sleep=0.25, args=[]):
    """Poll on a condition until timeout.

    Waits until the |function| evalues to True or until |timeout| secs,
    whichever occurs earlier.

    This is better than using a sleep, since it waits (almost) only as much
    as needed.

    WARNING: This method call should be avoided as far as possible in favor
    of a real wait from chromium (like wait-until-page-loaded).
    Only use in case there's really no better option.

    EXAMPLES:-
    Wait for "file.txt" to get created:
      WaitUntil(os.path.exists, args=["file.txt"])

    Same as above, but using lambda:
      WaitUntil(lambda: os.path.exists("file.txt"))

    Args:
      function: the function whose truth value is to be evaluated
      timeout: the max timeout (in secs) for which to wait. The default
               action is to wait for 60secs, and can be changed by
               changing kWaitForActionMaxMsec in ui_test.cc.
               Use None to wait indefinitely.
      retry_sleep: the sleep interval (in secs) before retrying |function|.
                   Deaults to 0.25 secs.
      args: the args to pass to |function|

    Returns:
      True, if returning when |function| evaluated to True
      False, when returning due to timeout
    """
    if timeout == -1:  # Default
      timeout = self.action_max_timeout_ms()/1000.0
    assert callable(function), "function should be a callable"
    begin = time.time()
    while timeout is None or time.time() - begin <= timeout:
      if function(*args):
        return True
      time.sleep(retry_sleep)
    return False

  def GetBookmarkModel(self):
    """Return the bookmark model as a BookmarkModel object.

    This is a snapshot of the bookmark model; it is not a proxy and
    does not get updated as the bookmark model changes.
    """
    return bookmark_model.BookmarkModel(self._GetBookmarksAsJSON())

  def GetDownloadsInfo(self):
    """Return info about downloads.

    This includes all the downloads recognized by the history system.

    Returns:
      an instance of downloads_info.DownloadInfo
    """
    return download_info.DownloadInfo(
        self._SendJSONRequest(0, json.dumps({'command': 'GetDownloadsInfo'})))

  def GetOmniboxInfo(self, windex=0):
    """Return info about Omnibox.

    This represents a snapshot of the omnibox.  If you expect changes
    you need to call this method again to get a fresh snapshot.
    Note that this DOES NOT shift focus to the omnibox; you've to ensure that
    the omnibox is in focus or else you won't get any interesting info.

    It's OK to call this even when the omnibox popup is not showing.  In this
    case however, there won't be any matches, but other properties (like the
    current text in the omnibox) will still be fetched.

    Due to the nature of the omnibox, this function is sensitive to mouse
    focus.  DO NOT HOVER MOUSE OVER OMNIBOX OR CHANGE WINDOW FOCUS WHEN USING
    THIS METHOD.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)

    Returns:
      an instance of omnibox_info.OmniboxInfo
    """
    return omnibox_info.OmniboxInfo(
        self._SendJSONRequest(windex,
                              json.dumps({'command': 'GetOmniboxInfo'})))

  def SetOmniboxText(self, text, windex=0):
    """Enter text into the omnibox. This shifts focus to the omnibox.

    Args:
      text: the text to be set.
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    cmd_dict = {
        'command': 'SetOmniboxText',
        'text': text,
    }
    ret_dict = json.loads(self._SendJSONRequest(windex, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def WaitUntilOmniboxQueryDone(self, windex=0):
    """Wait until omnibox has finished populating results.

    Uses WaitUntil() so the wait duration is capped by the timeout values
    used by automation, which WaitUntil() uses.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    return self.WaitUntil(
        lambda : not self.GetOmniboxInfo(windex).IsQueryInProgress())

  def OmniboxMovePopupSelection(self, count, windex=0):
    """Move omnibox popup selection up or down.

    Args:
      count: number of rows by which to move.
             -ve implies down, +ve implies up
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    cmd_dict = {
        'command': 'OmniboxMovePopupSelection',
        'count': count,
    }
    ret_dict = json.loads(self._SendJSONRequest(windex, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def OmniboxAcceptInput(self, windex=0):
    """Accepts the current string of text in the omnibox.

    This is equivalent to clicking or hiting enter on a popup selection.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    cmd_dict = {
        'command': 'OmniboxAcceptInput',
    }
    ret_dict = json.loads(self._SendJSONRequest(windex, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def GetPrefsInfo(self):
    """Return info about preferences.

    This represents a snapshot of the preferences. If you expect preferences
    to have changed, you need to call this method again to get a fresh
    snapshot.

    Returns:
      an instance of prefs_info.PrefsInfo
    """
    return prefs_info.PrefsInfo(
        self._SendJSONRequest(0, json.dumps({'command': 'GetPrefsInfo'})))

  def SetPrefs(self, path, value):
    """Set preference for the given path.

    Preferences are stored by Chromium as a hierarchical dictionary.
    dot-separated paths can be used to refer to a particular preference.
    example: "session.restore_on_startup"

    Some preferences are managed, that is, they cannot be changed by the
    user. It's upto the user to know which ones can be changed. Typically,
    the options available via Chromium preferences can be changed.

    Args:
      path: the path the preference key that needs to be changed
            example: "session.restore_on_startup"
            One of the equivalent names in chrome/common/pref_names.h could
            also be used.
      value: the value to be set. It could be plain values like int, bool,
             string or complex ones like list.
             The user has to ensure that the right value is specified for the
             right key. It's useful to dump the preferences first to determine
             what type is expected for a particular preference path.
    """
    cmd_dict = {
      'command': 'SetPrefs',
      'path': path,
      'value': value,
    }
    ret_dict = json.loads(self._SendJSONRequest(0, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def WaitForAllDownloadsToComplete(self):
    """Wait for all downloads to complete."""
    # Implementation detail: uses the generic "JSON command" model
    # (experimental)
    self._SendJSONRequest(0, json.dumps({'command':
                                         'WaitForAllDownloadsToComplete'}))

  def DownloadAndWaitForStart(self, file_url):
    """Trigger download for the given url and wait for downloads to start.

    It waits for download by looking at the download info from Chrome, so
    anything which isn't registered by the history service won't be noticed.
    This is not thread-safe, but it's fine to call this method to start
    downloading multiple files in parallel. That is after starting a
    download, it's fine to start another one even if the first one hasn't
    completed.
    """
    num_downloads = len(self.GetDownloadsInfo().Downloads())
    self.NavigateToURL(file_url)  # Trigger download.
    # It might take a while for the download to kick in, hold on until then.
    self.assertTrue(self.WaitUntil(
        lambda: len(self.GetDownloadsInfo().Downloads()) == num_downloads + 1))

  def GetBrowserInfo(self):
    """Return info about the browser.

    This includes things like the version number, the executable name,
    executable path, and so on.

    Returns:
      a dictionary of properties about the browser
      Sample:
        { u'BrowserProcessExecutableName': u'Chromium',
          u'BrowserProcessExecutablePath': u'Chromium.app/Contents/'
                                            'MacOS/Chromium',
          u'ChromeVersion': u'6.0.401.0',
          u'HelperProcessExecutableName': u'Chromium Helper',
          u'HelperProcessExecutablePath': u'Chromium Helper.app/Contents/'
                                           'MacOS/Chromium Helper',
          u'command_line_string': "command_line_string --with-flags"
        }

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetBrowserInfo',
    }
    ret_dict = json.loads(self._SendJSONRequest(0, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])
    return ret_dict['properties']

  def GetHistoryInfo(self, search_text=''):
    """Return info about browsing history.

    Args:
      search_text: the string to search in history.  Defaults to empty string
                   which means that all history would be returned. This is
                   functionally equivalent to searching for a text in the
                   chrome://history UI. So partial matches work too.
                   When non-empty, the history items returned will contain a
                   "snippet" field corresponding to the snippet visible in
                   the chrome://history/ UI.

    Returns:
      an instance of history_info.HistoryInfo
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetHistoryInfo',
      'search_text': search_text,
    }
    return history_info.HistoryInfo(
        self._SendJSONRequest(0, json.dumps(cmd_dict)))

  def AddHistoryItem(self, item):
    """Forge a history item for Chrome.

    Args:
      item: a python dictionary representing the history item.  Example:
      {
        # URL is the only mandatory item.
        'url': 'http://news.google.com',
        # Title is optional.
        'title': 'Google News',
        # Time is optional; if not set, assume "now".  Time is in
        # seconds since the Epoch.  The python construct to get "Now"
        # in the right scale is "time.time()".  Can be float or int.
        'time': 1271781612
      }
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'AddHistoryItem',
      'item': item
    }
    if not 'url' in item:
      raise JSONInterfaceError('must specify url')
    ret_dict = json.loads(self._SendJSONRequest(0, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def GetPluginsInfo(self):
    """Return info about plugins.

    This is the info available from about:plugins

    Returns:
      an instance of plugins_info.PluginsInfo
    """
    return plugins_info.PluginsInfo(
        self._SendJSONRequest(0, json.dumps({'command': 'GetPluginsInfo'})))

  def EnablePlugin(self, path):
    """Enable the plugin at the given path.

    Use GetPluginsInfo() to fetch path info about a plugin.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'EnablePlugin',
      'path': path,
    }
    ret_dict = json.loads(self._SendJSONRequest(0, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])

  def DisablePlugin(self, path):
    """Disable the plugin at the given path.

    Use GetPluginsInfo() to fetch path info about a plugin.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'DisablePlugin',
      'path': path,
    }
    ret_dict = json.loads(self._SendJSONRequest(0, json.dumps(cmd_dict)))
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])


class PyUITestSuite(pyautolib.PyUITestSuiteBase, unittest.TestSuite):
  """Base TestSuite for PyAuto UI tests."""

  def __init__(self, args):
    pyautolib.PyUITestSuiteBase.__init__(self, args)

    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    logging.debug('Loading pyauto libs from %s', browser_dir)
    self.Initialize(pyautolib.FilePath(browser_dir))
    os.environ['PATH'] = browser_dir + os.pathsep + os.environ['PATH']

    unittest.TestSuite.__init__(self)

  def __del__(self):
    # python unittest module is setup such that the suite gets deleted before
    # the test cases, which is odd because our test cases depend on
    # initializtions like exitmanager, autorelease pool provided by the
    # suite. Forcibly delete the test cases before the suite.
    del self._tests
    pyautolib.PyUITestSuiteBase.__del__(self)


# Implementation inspired from unittest.main()
class Main(object):
  """Main program for running PyAuto tests."""

  _options, _args = None, None
  _tests_filename = 'PYAUTO_TESTS'
  _platform_map = {
    'win32':  'win',
    'darwin': 'mac',
    'linux2': 'linux',
  }

  def __init__(self):
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parse command line args."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-v', '--verbose', action='store_true', default=False,
        help='Make PyAuto verbose.')
    parser.add_option(
        '-D', '--wait-for-debugger', action='store_true', default=False,
        help='Block PyAuto on startup for attaching debugger.')
    parser.add_option(
        '', '--ui-test-flags', type='string', default='',
        help='Flags passed to the UI test suite. Refer ui_test.h for options')
    parser.add_option(
        '', '--list-missing-tests', action='store_true', default=False,
        help='Print a list of tests not included in PYAUTO_TESTS, and exit')
    parser.add_option(
        '', '--repeat', type='int', default=1,
        help='Number of times to repeat the tests. Useful to determine '
             'flakiness. Defaults to 1.')

    self._options, self._args = parser.parse_args()

    # Setup logging
    if self._options.verbose:
      logging.basicConfig(level=logging.DEBUG,
                          format='%(asctime)s %(levelname)-8s %(message)s')
    if self._options.list_missing_tests:
      self._ListMissingTests()
      sys.exit(0)

  def TestsDir(self):
    """Returns the path to dir containing tests.

    This is typically the dir containing the tests description file.
    This method should be overridden by derived class to point to other dirs
    if needed.
    """
    return os.path.dirname(__file__)

  def _ListMissingTests(self):
    """Print tests missing from PYAUTO_TESTS."""
    def _GetTestsFrom(module_string):
      try:
        module = __import__(module_string)
      except ImportError:  # Probably just a test script
        return [module_string]
      tests = []
      for name in dir(module):
        obj = getattr(module, name)
        if (isinstance(obj, (type, types.ClassType)) and
            issubclass(obj, PyUITest) and obj != PyUITest):
          tests += [module_string + "." + obj.__name__ + "." + x for x in \
                    filter(lambda x: x.startswith('test'), dir(obj))]
      return tests
    # Fetch tests from all test scripts
    all_test_files = filter(lambda x: x.endswith('.py'),
                            os.listdir(self.TestsDir()))
    all_tests_modules = [os.path.splitext(x)[0] for x in all_test_files]
    all_tests = reduce(lambda x, y: x + y,
                       map(_GetTestsFrom, all_tests_modules))
    # Fetch tests included by PYAUTO_TESTS
    pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
    pyauto_tests = reduce(lambda x, y: x + y,
                          map(_GetTestsFrom,
                              self._GetTestNamesFrom(pyauto_tests_file)))
    for a_test in all_tests:
      if a_test not in pyauto_tests:
        print a_test

  def _HasTestCases(self, module_string):
    """Determines if we have any PyUITest test case classes in the module
       identified by |module_string|."""
    module = __import__(module_string)
    for name in dir(module):
      obj = getattr(module, name)
      if (isinstance(obj, (type, types.ClassType)) and
          issubclass(obj, PyUITest)):
        return True
    return False

  def _LoadTests(self, args):
    """Returns a suite of tests loaded from the given args.

    The given args can be either a module (ex: module1) or a testcase
    (ex: module2.MyTestCase) or a test (ex: module1.MyTestCase.testX)
    If empty, the tests in the already imported modules are loaded.

    Args:
      args: [module1, module2, module3.testcase, module4.testcase.testX]
            These modules or test cases or tests should be importable
    """
    if not args:  # Load tests ourselves
      if self._HasTestCases('__main__'):    # we are running a test script
        args.append('__main__')   # run the test cases found in it
      else:  # run tests from the test description file
        pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
        logging.debug("Reading %s", pyauto_tests_file)
        if not os.path.exists(pyauto_tests_file):
          logging.warn("%s missing. Cannot load tests." % pyauto_tests_file)
        else:
          args = self._GetTestNamesFrom(pyauto_tests_file)
    args = args * self._options.repeat
    logging.debug("Loading tests from %s", args)
    loaded_tests = unittest.defaultTestLoader.loadTestsFromNames(args)
    return loaded_tests

  def _GetTestNamesFrom(self, filename):
    contents = open(filename).read()
    modules = eval(contents, {'__builtins__': None}, None)
    args = modules.get('all', []) + \
           modules.get(self._platform_map[sys.platform], [])
    return args

  def _Run(self):
    """Run the tests."""
    if self._options.wait_for_debugger:
      raw_input('Attach debugger to process %s and hit <enter> ' % os.getpid())

    pyauto_suite = PyUITestSuite(re.split('\s+', self._options.ui_test_flags))
    loaded_tests = self._LoadTests(self._args)
    pyauto_suite.addTests(loaded_tests)
    verbosity = 1
    if self._options.verbose:
      verbosity = 2
    result = unittest.TextTestRunner(verbosity=verbosity).run(pyauto_suite)
    del loaded_tests  # Need to destroy test cases before the suite
    del pyauto_suite
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  Main()
