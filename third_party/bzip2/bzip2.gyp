# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'bzip2',
      'type': 'static_library',
      'defines': ['BZ_NO_STDIO'],
      'sources': [
        'blocksort.c',
        'bzlib.c',
        'bzlib.h',
        'bzlib_private.h',
        'compress.c',
        'crctable.c',
        'decompress.c',
        'huffman.c',
        'randtable.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'product_name': 'libbzip2',
        }, {  # else: OS!="win"
          'product_name': 'bz2',
        }],
      ],
    },
  ],
}
