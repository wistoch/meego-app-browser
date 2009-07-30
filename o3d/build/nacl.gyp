# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'build_nacl',
      'type': 'none',
      'variables': {
        'nacl_output_dir': '<(SHARED_INTERMEDIATE_DIR)/nacl_libs',
      },
      'actions': [
        {
          'action_name': 'build_nacl',
          'inputs' : [
            'build_nacl.py',
          ],
          'outputs': [
            '<(nacl_output_dir)/google_nacl_imc<(LIBRARY_SUFFIX)',
            '<(nacl_output_dir)/google_nacl_imc_c<(LIBRARY_SUFFIX)',
            'dummy_file_that_never_gets_built_so_scons_always_runs',
          ],
          'action': [
            'python',
            '<@(_inputs)',
            '--output="<(nacl_output_dir)"',
            '--configuration="<(CONFIGURATION)"',
            '--platform=<(OS)',
            'google_nacl_imc',
            'google_nacl_imc_c',
          ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../<(nacldir)',
        ],
        'libraries': [
          '<(nacl_output_dir)/google_nacl_imc<(LIBRARY_SUFFIX)',
          '<(nacl_output_dir)/google_nacl_imc_c<(LIBRARY_SUFFIX)',
        ],
      },
    },
  ],
}
