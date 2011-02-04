#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for ChromeDriver.

If your test is testing a specific part of the WebDriver API, consider adding
it to the appropriate place in the WebDriver tree instead.
"""

import platform
import os
import simplejson as json
import sys
import unittest
import urllib2
import urlparse

from chromedriver_launcher import ChromeDriverLauncher
import chromedriver_paths
from gtest_text_test_runner import GTestTextTestRunner

sys.path += [chromedriver_paths.PYTHON_BINDINGS]

from selenium.webdriver.remote.webdriver import WebDriver


class Request(urllib2.Request):
  """Extends urllib2.Request to support all HTTP request types."""

  def __init__(self, url, method=None, data=None):
    """Initialise a new HTTP request.

    Arguments:
      url: The full URL to send the request to.
      method: The HTTP request method to use; defaults to 'GET'.
      data: The data to send with the request as a string. Defaults to
          None and is ignored if |method| is not 'POST' or 'PUT'.
    """
    if method is None:
      method = data is not None and 'POST' or 'GET'
    elif method not in ('POST', 'PUT'):
      data = None
    self.method = method
    urllib2.Request.__init__(self, url, data=data)

  def get_method(self):
    """Returns the HTTP method used by this request."""
    return self.method


def SendRequest(url, method=None, data=None):
  """Sends a HTTP request to the WebDriver server.

  Return values and exceptions raised are the same as those of
  |urllib2.urlopen|.

  Arguments:
    url: The full URL to send the request to.
    method: The HTTP request method to use; defaults to 'GET'.
    data: The data to send with the request as a string. Defaults to
        None and is ignored if |method| is not 'POST' or 'PUT'.

    Returns:
      A file-like object.
  """
  request = Request(url, method=method, data=data)
  request.add_header('Accept', 'application/json')
  opener = urllib2.build_opener(urllib2.HTTPRedirectHandler())
  return opener.open(request)


class BasicTest(unittest.TestCase):
  """Basic ChromeDriver tests."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher()

  def tearDown(self):
    self._launcher.Kill()

  def testShouldReturn404WhenSentAnUnknownCommandURL(self):
    request_url = self._launcher.GetURL() + '/foo'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testShouldReturnHTTP405WhenSendingANonPostToTheSessionURL(self):
    request_url = self._launcher.GetURL() + '/session'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 405')
    except urllib2.HTTPError, expected:
      self.assertEquals(405, expected.code)
      self.assertEquals('POST', expected.hdrs['Allow'])

  def testShouldGetA404WhenAttemptingToDeleteAnUnknownSession(self):
    request_url = self._launcher.GetURL() + '/session/unkown_session_id'
    try:
      SendRequest(request_url, method='DELETE')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testCanStartChromeDriverOnSpecificPort(self):
    launcher = ChromeDriverLauncher(port=9520)
    self.assertEquals(9520, launcher.GetPort())
    driver = WebDriver(launcher.GetURL(), 'chrome', 'any')
    driver.quit()


class SessionTest(unittest.TestCase):
  """Tests dealing with WebDriver sessions."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher()

  def tearDown(self):
    self._launcher.Kill()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._launcher.GetURL() + '/session'
    response = SendRequest(request_url, method='POST', data='{}')
    self.assertEquals(200, response.code)
    self.session_url = response.geturl()  # TODO(jleyba): verify this URL?

    data = json.loads(response.read())
    self.assertTrue(isinstance(data, dict))
    self.assertEquals(0, data['status'])

    url_parts = urlparse.urlparse(self.session_url)[2].split('/')
    self.assertEquals(3, len(url_parts))
    self.assertEquals('', url_parts[0])
    self.assertEquals('session', url_parts[1])
    self.assertEquals(data['sessionId'], url_parts[2])

  def testShouldBeGivenCapabilitiesWhenStartingASession(self):
    driver = WebDriver(self._launcher.GetURL(), 'chrome', 'any')
    capabilities = driver.capabilities

    self.assertEquals('chrome', capabilities['browserName'])
    self.assertTrue(capabilities['javascriptEnabled'])

    # Value depends on what version the server is starting.
    self.assertTrue('version' in capabilities)
    self.assertTrue(
        isinstance(capabilities['version'], unicode),
        'Expected a %s, but was %s' % (unicode,
                                       type(capabilities['version'])))

    system = platform.system()
    if system == 'Linux':
      self.assertEquals('linux', capabilities['platform'].lower())
    elif system == 'Windows':
      self.assertEquals('windows', capabilities['platform'].lower())
    elif system == 'Darwin':
      self.assertEquals('mac', capabilities['platform'].lower())
    else:
      # No python on ChromeOS, so we won't have a platform value, but
      # the server will know and return the value accordingly.
      self.assertEquals('chromeos', capabilities['platform'].lower())
    driver.quit()

  def testSessionCreationDeletion(self):
    driver = WebDriver(self._launcher.GetURL(), 'chrome', 'any')
    driver.quit()

  def testMultipleSessionCreationDeletion(self):
    for i in range(10):
      driver = WebDriver(self._launcher.GetURL(), 'chrome', 'any')
      driver.quit()

  def testSessionCommandsAfterSessionDeletionReturn404(self):
    driver = WebDriver(self._launcher.GetURL(), 'chrome', 'any')
    session_id = driver.session_id
    driver.quit()
    try:
      response = SendRequest(self._launcher.GetURL() + '/session/' + session_id,
                             method='GET')
      self.fail('Should have thrown 404 exception')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testMultipleConcurrentSessions(self):
    drivers = []
    for i in range(10):
      drivers += [WebDriver(self._launcher.GetURL(), 'chrome', 'any')]
    for driver in drivers:
      driver.quit()


if __name__ == '__main__':
  unittest.main(module='chromedriver_tests',
                testRunner=GTestTextTestRunner(verbosity=1))
