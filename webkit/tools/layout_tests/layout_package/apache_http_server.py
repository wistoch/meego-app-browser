# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to start/stop the apache http server used by layout tests."""

import logging
import optparse
import os
import subprocess
import sys
import time

import google.httpd_utils

import path_utils
import platform_utils

class LayoutTestApacheHttpd(object):
  _PORTS = [
    {'port': 8000},
    {'port': 8080},
    {'port': 8443, 'is_ssl': True}
  ]

  def __init__(self, output_dir):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._httpd_proc = None
    path_utils.MaybeMakeDirectory(output_dir)

    # The upstream .conf file assumed the existence of /tmp/WebKit for placing
    # apache files like the lock file there.
    path_utils.MaybeMakeDirectory(os.path.join("/tmp", "WebKit"))

    test_dir = path_utils.PathFromBase('third_party', 'WebKit',
        'LayoutTests')
    js_test_resources_dir = os.path.join(test_dir, "fast", "js", "resources")
    mime_types_path = os.path.join(test_dir, "http", "conf", "mime.types")
    cert_file = os.path.join(test_dir, "http", "conf", "webkit-httpd.pem")

    cmd = [os.path.join("/usr", "sbin", "httpd"),
        '-f', self._GetApacheConfigFilePath(test_dir, output_dir),
        '-C', "DocumentRoot %s" % os.path.join(test_dir, "http", "tests"),
        '-c', "Alias /js-test-resources %s" % js_test_resources_dir,
        '-C', "Listen %s" % "127.0.0.1:8000",
        '-C', "Listen %s" % "127.0.0.1:8081",
        '-c', "TypesConfig \"%s\"" % mime_types_path,
        '-c', "CustomLog \"%s/access_log.txt\" common" % output_dir,
        '-c', "ErrorLog \"%s/error_log.txt\"" % output_dir,
        '-C', "User \"%s\"" % os.environ.get("USERNAME",
                                             os.environ.get("USER", "")),
        '-c', "SSLCertificateFile %s" % cert_file,
         ]

    self._start_cmd = cmd

  def _GetApacheConfigFilePath(self, test_dir, output_dir):
    """Returns the path to the apache config file to use.
    Args:
      test_dir: absolute path to the LayoutTests directory.
      output_dir: absolute path to the layout test results directory.
    """
    conf_file_name = "apache2-httpd.conf"
    httpd_config = os.path.join(test_dir, "http", "conf", conf_file_name)
    httpd_config_copy = os.path.join(output_dir, conf_file_name)
    main_document_root = os.path.join(path_utils.LayoutTestsDir(),
        "LayoutTests", "http", "tests")
    chrome_document_root = path_utils.PathFromBase('webkit', 'data',
        'layout_tests')
    httpd_conf = open(httpd_config).read()
    # TODO(ojan): Instead of writing an extra file, checkin a conf file
    # upstream. Or, even better, upstream/delete all our chrome http tests so we
    # don't need this special-cased DocumentRoot and then just use the upstream
    # conf file.
    httpd_conf = (httpd_conf +
        self._GetVirtualHostConfig(chrome_document_root, 8081))

    f = open(httpd_config_copy, 'wb')
    f.write(httpd_conf)
    f.close()

    return httpd_config_copy

  def _GetVirtualHostConfig(self, document_root, port, ssl=False):
    """Returns a <VirtualHost> directive block for an httpd.conf file.  It will
    listen to 127.0.0.1 on each of the given port.
    """
    return '\n'.join(('<VirtualHost 127.0.0.1:%s>' % port,
                      'DocumentRoot %s' % document_root,
                      ssl and 'SSLEngine On' or '',
                      '</VirtualHost>', ''))

  def _IsServerRunningOnAllPorts(self):
    """Returns whether the server is running on all the desired ports."""
    for mapping in self._PORTS:
      url = 'http%s://127.0.0.1:%d/' % ('is_ssl' in mapping and 's' or '',
                                        mapping['port'])
      if not google.httpd_utils.UrlIsAlive(url):
        return False

    return True

  def _StartHttpdProcess(self):
    """Starts the httpd process and returns whether there were errors."""
    self._httpd_proc = subprocess.Popen(self._start_cmd, stderr=subprocess.PIPE)
    if len(self._httpd_proc.stderr.read()):
      return False
    return True

  def _WaitForAction(self, action):
    """Repeat the action for 20 seconds or until it succeeds. Returns whether
    it succeeded."""
    succeeded = False

    start_time = time.time()
    # Give apache up to 20 seconds to start up.
    while time.time() - start_time < 20 and not succeeded:
      time.sleep(0.1)
      succeeded = action()

    return succeeded

  def Start(self):
    """Starts the apache http server."""
    # Stop any currently running servers.
    self.Stop()

    logging.debug("Starting apache http server")
    server_started = self._WaitForAction(self._StartHttpdProcess)
    if server_started:
      server_started = self._WaitForAction(self._IsServerRunningOnAllPorts)

    if server_started:
      logging.debug("Server successfully started")
    else:
      raise google.httpd_utils.HttpdNotStarted('Failed to start httpd')

  def Stop(self):
    """Stops the apache http server."""
    logging.debug("Shutting down any running http servers")
    path_utils.ShutDownHTTPServer(self._httpd_proc)
