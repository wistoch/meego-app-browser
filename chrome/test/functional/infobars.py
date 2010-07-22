#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto


class InfobarTest(pyauto.PyUITest):
  """TestCase for Infobars."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    To run:
      python chrome/test/functional/infobars.py infobars.InfobarTest.Debug
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetBrowserInfo()
      for window in info['windows']:
        for tab in window['tabs']:
          print 'Window', window['index'], 'tab', tab['index']
          pp.pprint(tab['infobars'])

  def _GetTabInfo(self, windex=0, tab_index=0):
    """Helper to return info for the given tab in the given window.

    Defaults to first tab in first window.
    """
    return self.GetBrowserInfo()['windows'][windex]['tabs'][tab_index]

  def testPluginCrashInfobar(self):
    """Verify the "plugin crashed" infobar."""
    flash_url = self.GetFileURLForPath(os.path.join(self.DataDir(),
                                                    'plugin', 'flash.swf'))
    # Trigger flash plugin
    self.NavigateToURL(flash_url)
    child_processes = self.GetBrowserInfo()['child_processes']
    flash = [x for x in child_processes if
             x['type'] == 'Plug-in' and x['name'] == 'Shockwave Flash'][0]
    self.assertTrue(flash)
    logging.info('Killing flash plugin. pid %d' % flash['pid'])
    self.Kill(flash['pid'])
    self.WaitForInfobarCount(1)
    crash_infobar = self._GetTabInfo()['infobars']
    self.assertTrue(crash_infobar)
    self.assertEqual(1, len(crash_infobar))
    self.assertTrue(re.match('The following plug-in has crashed:',
                             crash_infobar[0]['text']))
    self.assertEqual('alert_infobar', crash_infobar[0]['type'])
    # Dismiss the infobar
    self.PerformActionOnInfobar('dismiss', infobar_index=0)
    self.assertFalse(self._GetTabInfo()['infobars'])

  def _VerifyGeolocationInfobar(self, match_text, windex, tab_index):
    """Verify geolocation infobar and match given text.

    Assumes that geolocation infobar is showing up in the given tab in the
    given window.
    """
    tab_info = self._GetTabInfo(windex, tab_index)
    geolocation_infobar = tab_info['infobars']
    self.assertTrue(geolocation_infobar)
    self.assertEqual(1, len(geolocation_infobar))
    self.assertEqual(match_text, geolocation_infobar[0]['text'])
    self.assertEqual('Learn more', geolocation_infobar[0]['link_text'])
    self.assertEqual(2, len(geolocation_infobar[0]['buttons']))
    self.assertEqual('Allow', geolocation_infobar[0]['buttons'][0])
    self.assertEqual('Deny', geolocation_infobar[0]['buttons'][1])

  def testGeolocationInfobar(self):
    """Verify geoLocation infobar."""
    url = 'http://m.flickr.com/#/nearby/'  # automatically triggers geolocation
    match_text='m.flickr.com wants to track your physical location'
    self.NavigateToURL(url)
    self._VerifyGeolocationInfobar(windex=0, tab_index=0, match_text=match_text)
    # Accept, and verify that the infobar went away
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.assertFalse(self._GetTabInfo()['infobars'])

  def testGeolocationInfobarInMultipleTabsAndWindows(self):
    """Verify GeoLocation inforbar in multiple tabs."""
    url = 'http://m.flickr.com/#/nearby/'  # automatically triggers geolocation
    match_text='m.flickr.com wants to track your physical location'
    for tab_index in range(1, 2):
      self.AppendTab(pyauto.GURL(url))
      self._VerifyGeolocationInfobar(windex=0, tab_index=tab_index,
                                     match_text=match_text)
    # Try in a new window
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(url, 1, 0)
    self._VerifyGeolocationInfobar(windex=1, tab_index=0, match_text=match_text)
    # Incognito window
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 2, 0)
    self._VerifyGeolocationInfobar(windex=2, tab_index=0, match_text=match_text)


if __name__ == '__main__':
  pyauto_functional.Main()
