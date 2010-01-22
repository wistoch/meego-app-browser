# Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is the Mac implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import os
import platform
import signal
import subprocess

import path_utils


def PlatformName():
    """Returns the name of the platform we're currently running on."""
    # At the moment all chromium mac results are version-independent. At some
    # point we may need to return 'chromium-mac' + PlatformVersion()
    return 'chromium-mac'


def PlatformVersion():
    """Returns the version string for the platform, e.g. '-vista' or
    '-snowleopard'. If the platform does not distinguish between
    minor versions, it returns ''."""
    os_version_string = platform.mac_ver()[0]  # e.g. "10.5.6"
    if not os_version_string:
        return '-leopard'

    release_version = int(os_version_string.split('.')[1])

    # we don't support 'tiger' or earlier releases
    if release_version == 5:
        return '-leopard'
    elif release_version == 6:
        return '-snowleopard'

    return ''


def GetNumCores():
    """Returns the number of cores on the machine. For hyperthreaded machines,
    this will be double the number of actual processors."""
    return int(os.popen2("sysctl -n hw.ncpu")[1].read())


def BaselinePath(platform=None):
    """Returns the path relative to the top of the source tree for the
    baselines for the specified platform version. If |platform| is None,
    then the version currently in use is used."""
    if platform is None:
        platform = PlatformName()
    return path_utils.PathFromBase('webkit', 'data', 'layout_tests',
                                   'platform', platform, 'LayoutTests')

# TODO: We should add leopard and snowleopard to the list of paths to check
# once we start running the tests from snowleopard.


def BaselineSearchPath(platform=None):
    """Returns the list of directories to search for baselines/results, in
    order of preference. Paths are relative to the top of the source tree."""
    return [BaselinePath(platform),
            path_utils.WebKitBaselinePath('mac' + PlatformVersion()),
            path_utils.WebKitBaselinePath('mac')]


def WDiffPath():
    """Path to the WDiff executable, which we assume is already installed and
    in the user's $PATH."""
    return 'wdiff'


def ImageDiffPath(target):
    """Path to the image_diff executable

    Args:
      target: build type - 'Debug','Release',etc."""
    return path_utils.PathFromBase('xcodebuild', target, 'image_diff')


def LayoutTestHelperPath(target):
    """Path to the layout_test_helper executable, if needed, empty otherwise

    Args:
      target: build type - 'Debug','Release',etc."""
    return path_utils.PathFromBase('xcodebuild', target, 'layout_test_helper')


def TestShellPath(target):
    """Path to the test_shell executable.

    Args:
      target: build type - 'Debug','Release',etc."""
    # TODO(pinkerton): make |target| happy with case-sensitive file systems.
    return path_utils.PathFromBase('xcodebuild', target, 'TestShell.app',
                                   'Contents', 'MacOS', 'TestShell')


def ApacheExecutablePath():
    """Returns the executable path to start Apache"""
    return os.path.join("/usr", "sbin", "httpd")


def ApacheConfigFilePath():
    """Returns the path to Apache config file"""
    return path_utils.PathFromBase("third_party", "WebKit", "LayoutTests",
        "http", "conf", "apache2-httpd.conf")


def LigHTTPdExecutablePath():
    """Returns the executable path to start LigHTTPd"""
    return path_utils.PathFromBase('third_party', 'lighttpd', 'mac',
                                   'bin', 'lighttpd')


def LigHTTPdModulePath():
    """Returns the library module path for LigHTTPd"""
    return path_utils.PathFromBase('third_party', 'lighttpd', 'mac', 'lib')


def LigHTTPdPHPPath():
    """Returns the PHP executable path for LigHTTPd"""
    return path_utils.PathFromBase('third_party', 'lighttpd', 'mac', 'bin',
                                   'php-cgi')


def ShutDownHTTPServer(server_pid):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

      Args:
        server_pid: The process ID of the running server.
    """
    # server_pid is not set when "http_server.py stop" is run manually.
    if server_pid is None:
        # TODO(mmoss) This isn't ideal, since it could conflict with lighttpd
        # processes not started by http_server.py, but good enough for now.
        KillAllProcess('lighttpd')
        KillAllProcess('httpd')
    else:
        try:
            os.kill(server_pid, signal.SIGTERM)
            # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
        except OSError:
            # Sometimes we get a bad PID (e.g. from a stale httpd.pid file),
            # so if kill fails on the given PID, just try to 'killall' web
            # servers.
            ShutDownHTTPServer(None)


def KillProcess(pid):
    """Forcefully kill the process.

    Args:
      pid: The id of the process to be killed.
    """
    os.kill(pid, signal.SIGKILL)


def KillAllProcess(process_name):
    # On Mac OS X 10.6, killall has a new constraint: -SIGNALNAME or
    # -SIGNALNUMBER must come first.  Example problem:
    #   $ killall -u $USER -TERM lighttpd
    #   killall: illegal option -- T
    # Use of the earlier -TERM placement is just fine on 10.5.
    null = open(os.devnull)
    subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'),
                    process_name], stderr=null)
    null.close()


def KillAllTestShells():
    """Kills all instances of the test_shell binary currently running."""
    KillAllProcess('TestShell')
