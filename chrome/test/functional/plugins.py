#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PluginsTest(pyauto.PyUITest):
  """TestCase for Plugins."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter> to list plugins...')
      pp.pprint(self.GetPluginsInfo().Plugins())

  def _ObtainPluginsList(self):
    """Obtain a list of plugins for each platform.

    Produces warnings for plugins which are not installed on the machine.

    Returns:
      a list of 2-tuple, corresponding to the html file used for test and the
      name of the plugin
    """
    plugins = [('flash-clicktoplay.html', 'Shockwave Flash'),
               ('java_new.html', 'Java'),]    # common to all platforms
    if self.IsWin() or self.IsMac():
      plugins = plugins + [
         ('silverlight_new.html', 'Silverlight'),
         ('quicktime.html', 'QuickTime'),
         ('wmp_new.html', 'Windows Media'),
         ('real.html', 'RealPlayer'),
      ]

    out = []
    # Emit warnings for plugins that are not installed on the machine and
    # therefore cannot be tested.
    plugins_info = self.GetPluginsInfo()
    for fname, name in plugins:
      for a_plugin in plugins_info.Plugins():
        is_installed = False
        if re.search(name, a_plugin['name']):
          is_installed = True
          break
      if not is_installed:
        logging.warn('%s plugin is not installed and cannot be tested' % name)
      else:
        out.append((fname, name))
    return out

  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    for x in child_processes:
       if x['type'] == 'Plug-in' and re.search(plugin_name, x['name']):
         return x['pid']
    return None

  def _TogglePlugin(self, plugin_name):
    """Toggle a plugin's status.

    If enabled, disable it.
    If disabled, enable it.
    """
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
        if plugin['enabled']:
          self.DisablePlugin(plugin['path'])
        else:
          self.EnablePlugin(plugin['path'])

  def _IsEnabled(self, plugin_name):
    """Checks if plugin is enabled."""
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
         return plugin['enabled']

  def testKillAndReloadAllPlugins(self):
    """Verify plugin processes and check if they can reload after killing."""
    for fname, plugin_name in self._ObtainPluginsList():
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      pid = self._GetPluginPID(plugin_name)
      self.assertTrue(pid, 'No plugin process for %s' % plugin_name)
      self.Kill(pid)
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name) is None),
          msg='Expected %s plugin to die after killing' % plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      pid_reload = self._GetPluginPID(plugin_name)
      self.assertTrue(pid_reload,
                      'No plugin process for %s after reloading' % plugin_name)
      # Verify that it's in fact a new process.
      self.assertNotEqual(pid, pid_reload,
                          'Did not get new pid for %s after reloading' %
                          plugin_name)

  def testDisableEnableAllPlugins(self):
    """Verify if all the plugins can be disabled and enabled.

    This is equivalent to testing the enable/disable functionality in
    chrome://plugins
    """
    for fname, plugin_name in self._ObtainPluginsList():
      # Verify initial state
      self.assertTrue(self._IsEnabled(plugin_name),
                      '%s not enabled initially.' % plugin_name)
      # Disable
      self._TogglePlugin(plugin_name)
      self.assertFalse(self._IsEnabled(plugin_name))
      # Attempt to load a page that triggers the plugin and verify that it
      # indeed could not be loaded.
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      self.assertFalse([x for x in self.GetBrowserInfo()['child_processes']
                        if x['type'] == 'Plug-in' and
                        re.search(plugin_name, x['name'])])
      if 'Shockwave Flash' == plugin_name:
        continue  # cannot reload file:// flash URL - crbug.com/47249
      # Enable
      self._TogglePlugin(plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      self.assertTrue([x for x in self.GetBrowserInfo()['child_processes']
                       if x['type'] == 'Plug-in' and
                       re.search(plugin_name, x['name'])])
      self.assertTrue(self._IsEnabled(plugin_name), plugin_name)


if __name__ == '__main__':
  pyauto_functional.Main()
