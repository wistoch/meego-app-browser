# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'candidate_window',
      'type': 'executable',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../build/linux/system.gyp:gtk',
        '../../../../build/linux/system.gyp:x11',
        '../../../../chrome/chrome.gyp:common_constants',
        '../../../../skia/skia.gyp:skia',
        '../../../../views/views.gyp:views',
        '../cros/cros_api.gyp:cros_api',
      ],
      'sources': [
        'candidate_window.cc',
        # For loading libcros.
        '../cros/cros_library.cc',
      ],
    },
  ],
}
