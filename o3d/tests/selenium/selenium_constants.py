#!/usr/bin/python2.4
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


"""Constants for the selenium scripts.

Variables that are shared across all the selenium scripts.
"""


import os
import sys

script_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
o3d_dir = os.path.dirname(os.path.dirname(script_dir))

# Default path where screenshots will be stored.
DEFAULT_SCREENSHOT_PATH = os.path.join(o3d_dir,
                                       "tests",
                                       "selenium",
                                       "screenshots")

# Path where reference screenshots will be stored.
# Unfortunately we need separate reference images for certain platforms
# for certain tests.
DEFAULT_SCREENSHOT_DIR = "reference"
if sys.platform == "darwin":
  PLATFORM_SCREENSHOT_DIR = "reference-mac"
elif sys.platform[:5] == "linux":
  PLATFORM_SCREENSHOT_DIR = "reference-linux"
elif sys.platform == "win32" or sys.platform == "cygwin":
  PLATFORM_SCREENSHOT_DIR = "reference-win"
else:
  raise Exception, 'Platform %s not supported' % sys.platform



SELENIUM_BROWSER_SET = ["*iexplore", "*firefox", "*googlechrome", "*safari"]

# Dimensions to resize window to
# on the Mac, the window has to be big enough to hold the entire o3d div
# otherwise the OpenGL context will be clipped to the size of the window
RESIZE_WIDTH = 1400
RESIZE_HEIGHT = 1200
