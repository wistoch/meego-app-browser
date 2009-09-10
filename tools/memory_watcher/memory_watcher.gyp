# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'memory_watcher',
      'type': 'shared_library',
      'msvs_guid': '3BD81303-4E14-4559-AA69-B30C3BAB08DD',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_gfx',
      ],
      'defines': [
        'BUILD_MEMORY_WATCHER',
      ],
      'include_dirs': [
        '../..',
        # TODO(beng): move wtl to src/third_party
        '../../chrome/third_party/wtl/include',
      ],
      'sources': [
        'call_stack.cc',
        'call_stack.h',
        'dllmain.cc',
        'hotkey.h',
        'ia32_modrm_map.cc',
        'ia32_opcode_map.cc',
        'memory_hook.cc',
        'memory_hook.h',
        'memory_watcher.cc',
        'memory_watcher.h',
        'mini_disassembler.cc',
        'preamble_patcher.cc',
        'preamble_patcher.h',
        'preamble_patcher_with_stub.cc',
      ],
    },
  ],
}
