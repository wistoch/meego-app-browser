# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'antlrdir': 'third_party/antlr3',
    'breakpaddir': 'breakpad/src',
    'fcolladadir': 'third_party/fcollada/files',
    'glewdir': 'third_party/glew/files',
    'gtestdir': 'testing/gtest/include',
    'internaldir':'o3d-internal',
    'jpegdir': 'third_party/libjpeg',
    'nacldir': 'third_party/native_client/googleclient',
    'nixysadir': 'o3d/third_party/nixysa',
    'npapidir': 'o3d/third_party/npapi',
    'pdiffdir': 'third_party/pdiff/files',
    'pngdir': 'third_party/libpng',
    'screenshotsdir': 'o3d_assets/tests/screenshots',
    'seleniumdir': 'third_party/selenium_rc/files',
    'skiadir': 'third_party/skia/include',
    'zlibdir': 'third_party/zlib',

    # Hack to ensure that these variables (specifically "renderer") are
    # available later in the file. Long term solution is late
    # evaluation of variables.
    'variables': {
      # If the DEPS file exists two levels up, then we're in a Chrome tree.
      'o3d_in_chrome%': '<!(python <(DEPTH)/o3d/build/file_exists.py <(DEPTH)/DEPS)',
      'conditions' : [
        # These have to come first because GYP doesn't like it when
        # they're part of the same conditional as a conditions clause that
        # uses them.
        ['OS == "win"',
          {
            'cgdir': 'third_party/cg/files/win',
            'renderer%': 'd3d9',
            'swiftshaderdir': 'o3d-internal/third_party/swiftshader/files',
          },
        ],
        ['OS == "mac"',
          {
            'cgdir': 'third_party/cg/files/mac',
            'renderer%': 'gl',
            'swiftshaderdir': '',
          },
        ],
        ['OS == "linux"',
          {
            'cgdir': 'third_party/cg/files/linux',
            'renderer%': 'gl',
            'swiftshaderdir': '',
          },
        ],
      ],
    },
    'o3d_in_chrome%': '<(o3d_in_chrome)',
    'renderer%': '<(renderer)',
    'cgdir%': '<(cgdir)',
    'swiftshaderdir%': '<(swiftshaderdir)',

    # We default to building everything only if the assets exist.
    # (and the teapot is the least likely asset to change).
    # This is so that chrome developers get a much reduced dependency set.
    'o3d_developer%': '<!(python <(DEPTH)/o3d/build/file_exists.py '
                      '<(DEPTH)/o3d/o3d_assets/samples/convert_assets/teapot.zip)',
    'selenium_screenshots%': 0,
  },
  'target_defaults': {
    'defines': [
      'GYP_BUILD',  # Needed to make a change in base/types.h conditional.
    ],
    # This needs to be in a target_conditions block in order to successfully
    # override the xcode_settings in ../../build/common.gypi.
    # Something to do with evaluation order.
    'target_conditions': [
      ['OS=="mac"', {
          'xcode_settings': {
            'MACOSX_DEPLOYMENT_TARGET': '10.4',
            # TODO(maf): figure out proper fix for the following.
            # There is only one place in plugin_mac.mm which attempts
            # to use ObjC exception handling.
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          },
      }],
    ],
  },
  'conditions' : [
    ['OS == "win"',
      {
        'target_defaults': {
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',
            'OS_WIN',
            'UNICODE',
            'NACL_WINDOWS',
            '_X86_',
          ],
          # Disable warning: "'this' : used in base member initialization list."
          'msvs_disabled_warnings': [4355],
          'conditions': [
            ['renderer == "d3d9"',
              {
                'defines': [
                  'RENDERER_D3D9',
                ],
              },
            ],
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "gles2"',
              {
                'defines': [
                  'RENDERER_GLES2',
                ],
              },
            ],
          ],
        },
      },
    ],
    ['OS == "mac"',
      {
        'target_defaults': {
          'defines': [
            'OS_MACOSX',
            'UNICODE',
            'GTEST_NOT_MAC_FRAMEWORK_MODE',
            'NACL_OSX=1',
            'MAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_4',
            'SK_BUILD_FOR_MAC',
          ],
          'configurations': {
            'Debug': {
              'xcode_settings': {
                'GCC_DEBUGGING_SYMBOLS': 'full',
              },
            },
          },
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
            'OTHER_CFLAGS': [
               '-fno-eliminate-unused-debug-symbols',
               '-mmacosx-version-min=10.4'],
            'WARNING_CFLAGS': ['-Wno-deprecated-declarations'],
            'WARNING_CXXFLAGS': ['-Wstrict-aliasing',
                                 '-Wno-deprecated',],
          },
          'conditions': [
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "gles2"',
              {
                'defines': [
                  'RENDERER_GLES2',
                ],
              },
            ],
          ],
        },
      },
    ],
    ['OS == "linux"',
      {
        'target_defaults': {
          'defines': [
            'LINUX',
            'MOZ_X11',
            'NACL_LINUX=1',
            'OS_LINUX',
            'SK_BUILD_FOR_UNIX',
            'UNICODE',
            'XP_UNIX',
          ],
          'cflags': [
            '-fvisibility=hidden',
            '-Wstrict-aliasing',
          ],
          'conditions': [
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "gles2"',
              {
                'defines': [
                  'RENDERER_GLES2',
                ],
              },
            ],
          ],
        },
      },
    ],
  ],
}
