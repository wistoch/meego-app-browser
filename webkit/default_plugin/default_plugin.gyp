# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'default_plugin',
          'type': 'static_library',
          'dependencies': [
            '../../third_party/libxml/libxml.gyp:libxml',
            '../../third_party/icu38/icu38.gyp:icui18n',
            '../../third_party/icu38/icu38.gyp:icuuc',
            '../../third_party/npapi/npapi.gyp:npapi',
            '../webkit.gyp:webkit_resources',
            '../../net/net.gyp:net_resources',
          ],
          'include_dirs': [
            '../..',
            # TODO(bradnelson): this should fall out of the dependencies.
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'sources': [
            'activex_installer.cc',
            'activex_installer.h',
            'default_plugin.cc',
            'default_plugin_resources.h',
            'default_plugin_shared.h',
            'install_dialog.cc',
            'install_dialog.h',
            'plugin_database_handler.cc',
            'plugin_database_handler.h',
            'plugin_impl_win.cc',
            'plugin_impl_win.h',
            'plugin_install_job_monitor.cc',
            'plugin_install_job_monitor.h',
            'plugin_main.cc',
            'plugin_main.h',
          ],
          'link_settings': {
            'libraries': ['-lUrlmon.lib'],
          },
        },
      ],
    },],
  ],
}
