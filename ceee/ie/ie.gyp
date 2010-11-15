# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ceee_ie_all',
      'type': 'none',
      'dependencies': [
        'common/common.gyp:*',
        'broker/broker.gyp:*',
        'plugin/bho/bho.gyp:*',
        'plugin/scripting/scripting.gyp:*',
        'plugin/toolband/toolband.gyp:*',
        'ie_unittests',
        'mediumtest_ie',
      ]
    },
    {
      'target_name': 'ie_unittests',
      'type': 'executable',
      'sources': [
        'broker/api_dispatcher_unittest.cc',
        'broker/broker_unittest.cc',
        'broker/cookie_api_module_unittest.cc',
        'broker/executors_manager_unittest.cc',
        'broker/infobar_api_module_unittest.cc',
        'broker/tab_api_module_unittest.cc',
        'broker/window_api_module_unittest.cc',
        'broker/window_events_funnel_unittest.cc',
        'broker/broker_rpc_unittest.cc',
        'common/chrome_frame_host_unittest.cc',
        'common/crash_reporter_unittest.cc',
        'common/extension_manifest_unittest.cc',
        'common/ceee_module_util_unittest.cc',
        'plugin/bho/browser_helper_object_unittest.cc',
        'plugin/bho/cookie_accountant_unittest.cc',
        'plugin/bho/cookie_events_funnel_unittest.cc',
        'plugin/bho/dom_utils_unittest.cc',
        'plugin/bho/events_funnel_unittest.cc',
        'plugin/bho/executor_unittest.cc',
        'plugin/bho/extension_port_manager.cc',
        'plugin/bho/frame_event_handler_unittest.cc',
        'plugin/bho/infobar_events_funnel_unittest.cc',
        'plugin/bho/tab_events_funnel_unittest.cc',
        'plugin/bho/tool_band_visibility_unittest.cc',
        'plugin/bho/webnavigation_events_funnel_unittest.cc',
        'plugin/bho/webrequest_events_funnel_unittest.cc',
        'plugin/bho/webrequest_notifier_unittest.cc',
        'plugin/bho/web_progress_notifier_unittest.cc',
        'plugin/scripting/content_script_manager.rc',
        'plugin/scripting/content_script_manager_unittest.cc',
        'plugin/scripting/content_script_native_api_unittest.cc',
        'plugin/scripting/renderer_extension_bindings_unittest.cc',
        'plugin/scripting/renderer_extension_bindings_unittest.rc',
        'plugin/scripting/script_host_unittest.cc',
        'plugin/scripting/userscripts_librarian_unittest.cc',
        'plugin/toolband/tool_band_unittest.cc',
        'plugin/toolband/toolband_module_reporting_unittest.cc',
        'testing/ie_unittest_main.cc',
        'testing/mock_broker_and_friends.h',
        'testing/mock_chrome_frame_host.h',
        'testing/mock_browser_and_friends.h',
        'testing/precompile.cc',
        'testing/precompile.h',
      ],
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # GMock and GTest appear to be really fat, so bump
              # precompile header memory setting to 332 megs.
              'AdditionalOptions': ['/Zm332', '/bigobj'],
            },
          },
        },
      },
      'dependencies': [
        'common/common.gyp:ie_common',
        'common/common.gyp:ie_common_settings',
        'common/common.gyp:ie_guids',
        'broker/broker.gyp:broker',
        'broker/broker.gyp:broker_rpc_lib',
        'plugin/bho/bho.gyp:bho',
        'plugin/scripting/scripting.gyp:javascript_bindings',
        'plugin/scripting/scripting.gyp:scripting',
        'plugin/toolband/toolband.gyp:ceee_ie_lib',
        'plugin/toolband/toolband.gyp:ie_toolband_common',
        'plugin/toolband/toolband.gyp:toolband_idl',
        '../../base/base.gyp:base',
        '../../breakpad/breakpad.gyp:breakpad_handler',
        '../testing/sidestep/sidestep.gyp:sidestep',
        '../testing/utils/test_utils.gyp:test_utils',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
      ],
      'libraries': [
        'oleacc.lib',
        'iepmapi.lib',
        'rpcrt4.lib',
      ],
    },
    {
      'target_name': 'mediumtest_ie',
      'type': 'executable',
      'sources': [
        'plugin/bho/mediumtest_browser_event.cc',
        'plugin/bho/mediumtest_browser_helper_object.cc',
        'testing/mediumtest_ie_common.cc',
        'testing/mediumtest_ie_common.h',
        'testing/mediumtest_ie_main.cc',
        'testing/precompile.cc',
        'testing/precompile.h',
      ],
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # GMock and GTest appear to be really fat, so bump
              # precompile header memory setting to 332 megs.
              'AdditionalOptions': ['/Zm332'],
            },
          },
        },
      },
      'dependencies': [
        'common/common.gyp:ie_common',
        'common/common.gyp:ie_common_settings',
        'common/common.gyp:ie_guids',
        'plugin/bho/bho.gyp:bho',
        'plugin/scripting/scripting.gyp:scripting',
        'plugin/toolband/toolband.gyp:toolband_idl',
        '../../base/base.gyp:base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../testing/utils/test_utils.gyp:test_utils',
      ],
      'libraries': [
        'oleacc.lib',
        'iepmapi.lib',
      ],
    },
  ]
}
