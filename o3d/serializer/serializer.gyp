# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
  },
  'targets': [
    {
      'target_name': 'o3dSerializer',
      'type': 'static_library',
      'dependencies': [
        '../import/import.gyp:o3dSerializationObjects',
      ],
      'sources': [
        'cross/serializer.cc',
        'cross/serializer.h',
        'cross/serializer_binary.cc',
        'cross/serializer_binary.h',
        'cross/version.h',
      ],
    },
    {
      'target_name': 'o3dSerializerTest',
      'type': 'none',
      'dependencies': [
        'o3dSerializer',
      ],
      'direct_dependent_settings': {
        'sources': [
          'cross/serializer_test.cc',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
