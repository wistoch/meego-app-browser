# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:*',
        '../../<(fcolladadir)/fcollada.gyp:*',
        '../../<(jpegdir)/libjpeg.gyp:*',
        '../../<(pngdir)/libpng.gyp:*',
        '../../<(zlibdir)/zlib.gyp:*',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../converter/converter.gyp:o3dConverter',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../import/archive.gyp:o3dArchive',
        '../import/import.gyp:o3dImport',
        '../plugin/idl/idl.gyp:o3dPluginIdl',
        '../plugin/plugin.gyp:add_version',
        '../plugin/plugin.gyp:o3dPlugin',
        '../plugin/plugin.gyp:o3dPluginLogging',
        '../serializer/serializer.gyp:o3dSerializer',
        '../statsreport/statsreport.gyp:o3dStatsReport',
        '../tests/tests.gyp:unit_tests',
        '../utils/utils.gyp:o3dUtils',
      ],
      'conditions': [
        ['OS=="win"',
          {
            'dependencies': [
              '../plugin/plugin.gyp:o3dActiveXHost',
            ],
          },
        ],
      ],
    },
  ],
}
