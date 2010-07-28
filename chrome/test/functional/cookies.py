#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class CookiesTest(pyauto.PyUITest):
  """Tests for Cookies."""

  def _ClearCookiesAndCheck(self, url):
    self.ClearBrowsingData(['COOKIES'], 'EVERYTHING')
    cookie_data = self.GetCookie(pyauto.GURL(url))
    self.assertEqual(0, len(cookie_data))

  def _CookieCheckRegularWindow(self, url):
    """Check the cookie for the given URL in a regular window."""
    self._ClearCookiesAndCheck(url)
    # Assert that the cookie data isn't empty after navigating to the url.
    self.NavigateToURL(url)
    cookie_data = self.GetCookie(pyauto.GURL(url))
    self.assertNotEqual(0, len(cookie_data))
    # Restart the browser and ensure the cookie data is the same.
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(cookie_data, self.GetCookie(pyauto.GURL(url)))

  def _CookieCheckIncognitoWindow(self, url):
    """Check the cookie for the given URL in an incognito window."""
    self._ClearCookiesAndCheck(url)
    # Navigate to the URL in an incognito window and verify no cookie is set.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 1, 0)
    cookie_data = self.GetCookie(pyauto.GURL(url))
    self.assertEqual(0, len(cookie_data))

  def testSetCookies(self):
    """Test setting cookies and getting the value."""
    cookie_url = pyauto.GURL(self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'title1.html')))
    cookie_val = 'foo=bar'
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url))

  def testCookiesHttp(self):
    """Test cookies set over HTTP for incognito and regular windows."""
    http_url = 'http://www.google.com'
    self._CookieCheckRegularWindow(http_url)
    self._CookieCheckIncognitoWindow(http_url)

  def testCookiesHttps(self):
    """Test cookies set over HTTPS for incognito and regular windows."""
    https_url = 'https://www.google.com'
    self._CookieCheckRegularWindow(https_url)
    self._CookieCheckIncognitoWindow(https_url)

  def testCookiesFile(self):
    """Test cookies set from an HTML file for incognito and regular windows."""
    file_url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'setcookie.html'))
    self._CookieCheckRegularWindow(file_url)
    self._CookieCheckIncognitoWindow(file_url)


if __name__ == '__main__':
  pyauto_functional.Main()
