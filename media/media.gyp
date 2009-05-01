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
    'conditions': [
      ['OS!="linux"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
  },
  'targets': [
    {
      'target_name': 'media',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'include_dirs': [
        '..',
      ],
      'msvs_guid': '6AE76406-B03B-11DD-94B1-80B556D89593',
      'sources': [
        'audio/audio_output.h',
        'audio/linux/audio_manager_linux.cc',
        'audio/mac/audio_manager_mac.cc',
        'audio/mac/audio_manager_mac.h',
        'audio/mac/audio_output_mac.cc',
        'audio/mac/audio_output_mac.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/win/audio_manager_win.h',
        'audio/win/audio_output_win.cc',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/buffers.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/factory.h',
        'base/filter_host.h',
        'base/filter_host_impl.cc',
        'base/filter_host_impl.h',
        'base/filters.h',
        'base/media_posix.cc',
        'base/media_win.cc',
        'base/media.h',
        'base/media_format.cc',
        'base/media_format.h',
        'base/mock_filter_host.h',
        'base/mock_media_filters.h',
        'base/mock_pipeline.h',
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/synchronizer.cc',
        'base/synchronizer.h',
        'base/video_frame_impl.cc',
        'base/video_frame_impl.h',
        'base/yuv_convert.cc',
        'base/yuv_convert.h',
        'base/yuv_scale.cc',
        'base/yuv_scale.h',
        'filters/audio_renderer_base.cc',
        'filters/audio_renderer_base.h',
        'filters/audio_renderer_impl.cc',
        'filters/audio_renderer_impl.h',
        'filters/decoder_base.h',
        'filters/ffmpeg_audio_decoder.cc',
        'filters/ffmpeg_audio_decoder.h',
        'filters/ffmpeg_common.cc',
        'filters/ffmpeg_common.h',
        'filters/ffmpeg_demuxer.cc',
        'filters/ffmpeg_demuxer.h',
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/file_data_source.h',
        'filters/null_audio_renderer.cc',
        'filters/null_audio_renderer.h',
        'filters/test_video_decoder.h',
        'filters/test_video_renderer.h',
        'filters/video_renderer_base.cc',
        'filters/video_renderer_base.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS =="linux"', {
          'sources/': [ ['exclude', '_(mac|win)\\.cc$'] ],
        }],
        ['OS =="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
            ],
          },
          'sources!': [
            'filters/ffmpeg_audio_decoder.cc',
            'filters/ffmpeg_glue.cc',
            'filters/ffmpeg_video_decoder.cc',
          ],
          'sources/': [ ['exclude', '_(linux|win)\\.cc$'] ],
        }],
        [ 'OS == "win"', {
          'sources/': [ ['exclude', '_(linux|mac|posix)\\.cc$'],
                        ['exclude', '\\.mm?$' ] ],
        }],
      ],
    },
    {
      'target_name': 'media_unittests',
      'type': 'executable',
      'msvs_guid': 'C8C6183C-B03C-11DD-B471-DFD256D89593',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'sources': [
        'audio/win/audio_output_win_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/pipeline_impl_unittest.cc',
        'base/run_all_unittests.cc',
        'base/video_frame_impl_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'base/yuv_scale_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/video_decoder_unittest.cc',
        'filters/video_renderer_unittest.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   base/run_all_unittests.cc
            #   ../base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'filters/ffmpeg_demuxer_unittest.cc',
            'filters/ffmpeg_glue_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources!': [
            'filters/ffmpeg_demuxer_unittest.cc',
            'filters/ffmpeg_glue_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'media_bench',
      'type': 'executable',
      'msvs_guid': '45BC4F87-4604-4962-A751-7C7B29A080BF',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'sources': [
        'bench/bench.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'media_player',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'include_dirs': [
            '../chrome/third_party/wtl/include',
          ],
          'dependencies': [
            'media',
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'player/player.cc',
            'player/player_wtl.rc',
            'player/resource.h',
            'player/stdafx.h',
            'player/stdafx.cc',
          ],
        },
      ],
    }],
  ],
}
