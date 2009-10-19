#!/usr/bin/python2.6.2
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Utility functions for running the lab tests.

"""

import logging
import os
import subprocess
import shutil
import sys
import time

import runner_constants as const
import util

CHANGE_RESOLUTION_PATH = (const.O3D_PATH + '/o3d/tests/lab' 
                          '/ChangeResolution/Debug/changeresolution.exe')

def EnsureWindowsScreenResolution(width, height, bpp):
  """Performs all steps needed to configure system for testing on Windows.

  Args:
    width: new screen resolution width
    height: new screen resolution height
    bpp: new screen resolution bytes per pixel
  Returns:
    True on success.
  """
  
  command = 'call "%s" %d %d %d' % (CHANGE_RESOLUTION_PATH, width, height, bpp)
  
  
  our_process = subprocess.Popen(command,
                                 shell=True,
                                 stdout=None,
                                 stderr=None,
                                 universal_newlines=True)

  our_process.wait()

  return our_process.returncode == 0


def AddPythonPath(path):
  """Add path to PYTHONPATH in environment."""
  try:
    os.environ['PYTHONPATH'] = path + os.pathsep + os.environ['PYTHONPATH']
  except KeyError:
    os.environ['PYTHONPATH'] = path

  # Need to put at front of sys.path so python will try to import modules from
  # path before locations further down the sys.path.
  sys.path = [path] + sys.path


def InstallO3DPlugin():
  """Installs O3D plugin."""
  
  if util.IsWindows():
    installer_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.msi')
  elif util.IsMac():
    dmg_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.dmg')
    volumes_path = util.MountDiskImage(dmg_path)
    if volumes_path is None:
      return False
    else:
      installer_path = os.path.join(volumes_path, 'O3D.mpkg')
  else:
    plugin_path = os.path.join(const.PRODUCT_DIR_PATH, 'libnpo3dautoplugin.so')
    plugin_dst_dir = os.path.expanduser('~/.mozilla/plugins')
    try:
      os.makedirs(plugin_dst_dir)
    except os.error:
      pass

    plugin_dst = os.path.join(plugin_dst_dir, 'libnpo3dautoplugin.so')
    shutil.copyfile(plugin_path, plugin_dst)
    return True
    
  logging.info('Installing plugin:"%s"', installer_path)

  if not os.path.exists(installer_path):
    logging.error('Installer path not found, %s' % installer_path)
    return False

  if util.IsWindows():
    install_command = 'msiexec.exe /i "%s"' % installer_path
  elif util.IsMac():
    admin_password = 'g00gl3'
    install_command = ('echo %s | sudo -S /usr/sbin/installer -pkg '
                       '"%s" -target /' % (admin_password, installer_path))

  logging.info('Installing...')
  result = os.system(install_command)
  if result:
    logging.error('Install failed.')
    return False
  logging.info('Installed.')
  
  if util.IsMac():
    util.UnmountDiskImage(volumes_path)
  
  return True

def UninstallO3DPlugin():
  """Uninstalls O3D.
  
  Returns:
    True, if O3D is no longer installed."""
  
  if util.IsWindows():
    installer_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.msi')
    os.system('msiexec.exe /x "%s" /q' % installer_path)
    
  else:
    for path in const.INSTALL_PATHS:
      if os.path.exists(path):
        os.remove(path)
    
  return not DoesAnO3DPluginExist()

def DoesAnO3DPluginExist():
  for path in const.INSTALL_PATHS:
    if os.path.exists(path):
      return True
  return False
    

  