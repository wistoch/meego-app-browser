# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS!="linux"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
  },
  'targets': [
    {
      'variables': {
        'generate_stubs_script': 'generate_stubs.py',
        'sig_files': [
          'avcodec-52.sigs',
          'avformat-52.sigs',
          'avutil-50.sigs',
        ],
        'extra_header': 'ffmpeg_stub_headers.fragment',
      },
      'target_name': 'ffmpeg',
      'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
      'sources': [
        'include/libavcodec/avcodec.h',
        'include/libavcodec/opt.h',
        'include/libavcodec/vdpau.h',
        'include/libavcodec/xvmc.h',
        'include/libavformat/avformat.h',
        'include/libavformat/avio.h',
        'include/libavutil/adler32.h',
        'include/libavutil/avstring.h',
        'include/libavutil/avutil.h',
        'include/libavutil/base64.h',
        'include/libavutil/common.h',
        'include/libavutil/crc.h',
        'include/libavutil/fifo.h',
        'include/libavutil/intfloat_readwrite.h',
        'include/libavutil/log.h',
        'include/libavutil/lzo.h',
        'include/libavutil/mathematics.h',
        'include/libavutil/md5.h',
        'include/libavutil/mem.h',
        'include/libavutil/pixfmt.h',
        'include/libavutil/rational.h',
        'include/libavutil/sha1.h',
        'include/win/inttypes.h',
        'include/win/stdint.h',
        '<@(sig_files)',
        '<(extra_header)'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'conditions': [
        ['OS=="win"',
          {
            'variables': {
              'outfile_type': 'windows_lib',
              'output_dir': '<(SHARED_INTERMEDIATE_DIR)'
            },
            'type': 'none',
            'dependencies': [
              'ffmpeg_binaries',
            ],
            'sources!': [
              '<(extra_header)',
            ],
            'direct_dependent_settings': {
              'link_settings': {
                'libraries': [
                  '<(output_dir)/avcodec-52.lib',
                  '<(output_dir)/avformat-52.lib',
                  '<(output_dir)/avutil-50.lib',
                ],
              },
            },
            'rules': [
              {
                'rule_name': 'generate_libs',
                'extension': 'sigs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(output_dir)/<(RULE_INPUT_ROOT).lib',
                ],
                'action': ['python', '<(generate_stubs_script)',
                           '-o', '<(output_dir)',
                           '-t', '<(outfile_type)',
                           '<@(RULE_INPUT_PATH)',
                ],
                'message': 'Generating FFmpeg import libraries.',
              },
            ],
          }
        ],
        ['OS=="linux"',
          # TODO(ajwong): Mac distributed build fails because media will
          # build before this action is taken.  Enable this for mac when
          # that is fixed.
          {
            'variables': {
              'outfile_type': 'posix_stubs',
              'stubs_filename_root': 'ffmpeg_stubs',
              'project_path': 'third_party/ffmpeg',
              'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
            },
            'type': '<(library)',
            'include_dirs': [
              'include',
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '<(output_root)',
                '../..',  # The chromium 'src' directory.
              ],
            },
            'actions': [
              {
                'action_name': 'generate_stubs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<(extra_header)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(output_root)/<(project_path)/<(stubs_filename_root).cc',
                  '<(output_root)/<(project_path)/<(stubs_filename_root).h',
                ],
                'action': ['python',
                           '<(generate_stubs_script)',
                           '-o', '<(output_root)/<(project_path)',
                           '-t', '<(outfile_type)',
                           '-e', '<(extra_header)',
                           '-s', '<(stubs_filename_root)',
                           '-p', '<(project_path)',
                           '<@(_inputs)',
                ],
                'message': 'Generating FFmpeg stubs for dynamic loading.',
                'process_outputs_as_sources': 1,
              },
            ],
          }
        ],
      ],
    },
    {
      'target_name': 'ffmpeg_binaries',
      'type': 'none',
      'msvs_guid': '4E4070E1-EFD9-4EF1-8634-3960956F6F10',
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'binaries/avcodec-52.dll',
            'binaries/avformat-52.dll',
            'binaries/avutil-50.dll',
            'binaries/pthreadGC2.dll',
          ],
          'dependencies': ['../../build/win/system.gyp:cygwin'],
          'rules': [
            {
              'rule_name': 'copy_binaries',
              'extension': 'dll',
              'inputs': [
                'copy_binaries.sh',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(RULE_INPUT_NAME)',
              ],
              'action': ['./copy_binaries.sh', '"<@(RULE_INPUT_PATH)"', '"<@(PRODUCT_DIR)/<@(RULE_INPUT_NAME)"'],
              'message': 'Copying binaries...',
            },
          ],
        }],
      ],
    },
  ],
}
