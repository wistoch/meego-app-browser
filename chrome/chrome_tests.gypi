# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables' : {
    'browser_tests_sources_views_specific': [
      'browser/extensions/browser_action_test_util_views.cc',
      'browser/views/browser_actions_container_browsertest.cc',
    ],
    'browser_tests_sources_win_specific': [
      'browser/extensions/extension_shelf_model_browsertest.cc',
      'browser/extensions/extension_popup_apitest.cc',
      # TODO(jam): http://crbug.com/15101 These tests fail on Linux and Mac.
      'browser/child_process_security_policy_browsertest.cc',
      'browser/renderer_host/test/web_cache_manager_browsertest.cc',
      'browser/renderer_host/test/render_view_host_manager_browsertest.cc',
    ],
    # TODO(jcampan): move these vars to views.gyp.
    'views_unit_tests_sources': [
      '../views/animation/bounds_animator_unittest.cc',
      '../views/view_unittest.cc',
      '../views/focus/focus_manager_unittest.cc',
      '../views/controls/label_unittest.cc',
      '../views/controls/progress_bar_unittest.cc',
      '../views/controls/table/table_view_unittest.cc',
      '../views/grid_layout_unittest.cc',
    ],
    'pyautolib_sources': [
      'app/chrome_dll_resource.h',
      'common/pref_names.cc',
      'common/pref_names.h',
      'test/automation/automation_constants.h',
      'test/automation/browser_proxy.cc',
      'test/automation/browser_proxy.h',
      'test/automation/tab_proxy.cc',
      'test/automation/tab_proxy.h',
    ],
  },
  'targets': [
    {
      # This target contains mocks and test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      'target_name': 'test_support_common',
      'type': '<(library)',
      'dependencies': [
        'browser',
        'common',
        'renderer',
        'chrome_resources',
        'chrome_strings',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'theme_resources',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'renderer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        # The only thing used from browser is Browser::Type.
        'browser/browser.h',
        'browser/cocoa/browser_test_helper.h',
        'browser/dummy_pref_store.cc',
        'browser/dummy_pref_store.h',
        'browser/extensions/test_extension_prefs.cc',
        'browser/extensions/test_extension_prefs.h',
        'browser/geolocation/mock_location_provider.cc',
        'browser/geolocation/mock_location_provider.h',
        'browser/mock_browsing_data_appcache_helper.cc',
        'browser/mock_browsing_data_appcache_helper.h',
        'browser/mock_browsing_data_database_helper.cc',
        'browser/mock_browsing_data_database_helper.h',
        'browser/mock_browsing_data_local_storage_helper.cc',
        'browser/mock_browsing_data_local_storage_helper.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (in chrome.gyp).
        #'browser/net/url_request_mock_http_job.cc',
        #'browser/net/url_request_mock_http_job.h',
        'browser/net/url_request_mock_net_error_job.cc',
        'browser/net/url_request_mock_net_error_job.h',
        'browser/pref_value_store.cc',
        'browser/pref_value_store.h',
        'browser/renderer_host/mock_render_process_host.cc',
        'browser/renderer_host/mock_render_process_host.h',
        'browser/renderer_host/test/test_backing_store.cc',
        'browser/renderer_host/test/test_backing_store.h',
        'browser/renderer_host/test/test_render_view_host.cc',
        'browser/renderer_host/test/test_render_view_host.h',
        'browser/tab_contents/test_tab_contents.cc',
        'browser/tab_contents/test_tab_contents.h',
        'common/ipc_test_sink.cc',
        'common/ipc_test_sink.h',
        'renderer/mock_keyboard.cc',
        'renderer/mock_keyboard.h',
        'renderer/mock_keyboard_driver_win.cc',
        'renderer/mock_keyboard_driver_win.h',
        'renderer/mock_printer.cc',
        'renderer/mock_printer.h',
        'renderer/mock_render_process.cc',
        'renderer/mock_render_process.h',
        'renderer/mock_render_thread.cc',
        'renderer/mock_render_thread.h',
        'test/automation/autocomplete_edit_proxy.cc',
        'test/automation/autocomplete_edit_proxy.h',
        'test/automation/automation_constants.h',
        'test/automation/automation_handle_tracker.cc',
        'test/automation/automation_handle_tracker.h',
        'test/automation/automation_proxy.cc',
        'test/automation/automation_proxy.h',
        'test/automation/browser_proxy.cc',
        'test/automation/browser_proxy.h',
        'test/automation/dom_element_proxy.cc',
        'test/automation/dom_element_proxy.h',
        'test/automation/extension_proxy.cc',
        'test/automation/extension_proxy.h',
        'test/automation/javascript_execution_controller.cc',
        'test/automation/javascript_execution_controller.h',
        'test/automation/javascript_message_utils.h',
        'test/automation/tab_proxy.cc',
        'test/automation/tab_proxy.h',
        'test/automation/window_proxy.cc',
        'test/automation/window_proxy.h',
        'test/chrome_process_util.cc',
        'test/chrome_process_util.h',
        'test/chrome_process_util_mac.cc',
        'test/model_test_utils.cc',
        'test/model_test_utils.h',
        'test/profile_mock.h',
        'test/test_browser_window.h',
        'test/test_location_bar.h',
        'test/testing_profile.cc',
        'test/testing_profile.h',
        'test/thread_observer_helper.h',
        'test/ui_test_utils.cc',
        'test/ui_test_utils.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.cc',
        'test/ui_test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_ui',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automated_ui_tests/automated_ui_test_base.cc',
        'test/automated_ui_tests/automated_ui_test_base.h',
        'test/testing_browser_process.h',
        'test/ui/javascript_test_util.cc',
        'test/ui/npapi_test_helper.cc',
        'test/ui/npapi_test_helper.h',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_layout_test.cc',
        'test/ui/ui_layout_test.h',
        'test/ui/ui_test.cc',
        'test/ui/ui_test.h',
        'test/ui/ui_test_suite.cc',
        'test/ui/ui_test_suite.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_unit',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/unit/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   test/unit/run_all_unittests.cc
            #   test/unit/chrome_test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
     },
    {
      'target_name': 'automated_ui_tests',
      'type': 'executable',
      'msvs_guid': 'D2250C20-3A94-4FB9-AF73-11BC5B73884B',
      'dependencies': [
        'browser',
        'renderer',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/libxml/libxml.gyp:libxml',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automated_ui_tests/automated_ui_test_interactive_test.cc',
        'test/automated_ui_tests/automated_ui_tests.cc',
        'test/automated_ui_tests/automated_ui_tests.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_tests',
      'type': 'executable',
      'msvs_guid': '76235B67-1C27-4627-8A33-4B2E1EF93EDE',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'profile_import',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        # run time dependencies
        '../third_party/ppapi/ppapi.gyp:ppapi_tests',
        '../webkit/support/webkit_support.gyp:npapi_layout_test_plugin',
        '../webkit/default_plugin/default_plugin.gyp:default_plugin',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/chrome_main_uitest.cc',
        'browser/browser_encoding_uitest.cc',
        'browser/browser_uitest.cc',
        'browser/cookie_modal_dialog_uitest.cc',
        'browser/dom_ui/bookmarks_ui_uitest.cc',
        'browser/dom_ui/new_tab_ui_uitest.cc',
        'browser/download/download_uitest.cc',
        'browser/download/save_page_uitest.cc',
        'browser/errorpage_uitest.cc',
        'browser/default_plugin_uitest.cc',
        'browser/extensions/extension_uitest.cc',
        'browser/history/multipart_uitest.cc',
        'browser/history/redirect_uitest.cc',
        'browser/iframe_uitest.cc',
        'browser/images_uitest.cc',
        'browser/in_process_webkit/dom_storage_uitest.cc',
        'browser/locale_tests_uitest.cc',
        'browser/login_prompt_uitest.cc',
        'browser/media_uitest.cc',
        'browser/metrics/metrics_service_uitest.cc',
        'browser/pref_service_uitest.cc',
        'browser/printing/printing_layout_uitest.cc',
        'browser/process_singleton_linux_uitest.cc',
        'browser/process_singleton_uitest.cc',
        'browser/renderer_host/resource_dispatcher_host_uitest.cc',
        'browser/repost_form_warning_uitest.cc',
        'browser/sanity_uitest.cc',
        'browser/session_history_uitest.cc',
        'browser/sessions/session_restore_uitest.cc',
        'browser/tab_contents/view_source_uitest.cc',
        'browser/tab_restore_uitest.cc',
        'browser/unload_uitest.cc',
        'browser/views/find_bar_host_uitest.cc',
        'common/logging_chrome_uitest.cc',
        'test/automation/automation_proxy_uitest.cc',
        'test/automation/extension_proxy_uitest.cc',
        'test/automated_ui_tests/automated_ui_test_test.cc',
        'test/chrome_process_util_uitest.cc',
        'test/ui/dom_checker_uitest.cc',
        'test/ui/dromaeo_benchmark_uitest.cc',
        'test/ui/fast_shutdown_uitest.cc',
        'test/ui/history_uitest.cc',
        'test/ui/layout_plugin_uitest.cc',
        'test/ui/npapi_uitest.cc',
        'test/ui/omnibox_uitest.cc',
        'test/ui/pepper_uitest.cc',
        'test/ui/ppapi_uitest.cc',
        'test/ui/sandbox_uitests.cc',
        'test/ui/sunspider_uitest.cc',
        'test/ui/v8_benchmark_uitest.cc',
        'worker/worker_uitest.cc',
      ],
      'conditions': [
        # http://code.google.com/p/chromium/issues/detail?id=18337
        ['target_arch!="x64" and target_arch!="arm"', {
          'dependencies': [
            '../webkit/webkit.gyp:npapi_test_plugin',
            '../webkit/webkit.gyp:npapi_pepper_test_plugin',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, { # else: OS != "linux"
          'sources!': [
            'browser/process_singleton_linux_uitest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            'browser/download/download_uitest.cc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          'sources!': [
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_uitest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            # TODO(port): http://crbug.com/45770
            'browser/printing/printing_layout_uitest.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
          'sources!': [
            # TODO(port): http://crbug.com/30700
            'test/ui/npapi_uitest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'nacl_ui_tests',
      'type': 'executable',
      'msvs_guid': '43E2004F-CD62-4595-A8A6-31E9BFA1EE5E',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'test_support_ui',
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/nacl/nacl_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_nacl_win64',
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/webkit.gyp:npapi_test_plugin',
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'unit_tests',
      'type': 'executable',
      'msvs_guid': 'ECFC2BEC-9FC0-4AD9-9649-5F26793F65FC',
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'common',
        'common_net_test_support',
        'debugger',
        'profile_import',
        'renderer',
        'service',
        'test_support_unit',
        'utility',
        '../app/app.gyp:app_base',
        '../app/app.gyp:app_resources',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../webkit/support/webkit_support.gyp:webkit_resources',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/expat/expat.gyp:expat',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
        '../third_party/cld',
      ],
      'defines': [
        'CLD_WINDOWS',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CLD_WINDOWS',
        ],
      },
      'sources': [
        'app/breakpad_mac_stubs.mm',
        # All unittests in browser, common, renderer and service.
        'browser/app_controller_mac_unittest.mm',
        'browser/app_menu_model_unittest.cc',
        'browser/autocomplete_history_manager_unittest.cc',
        'browser/autocomplete/autocomplete_edit_unittest.cc',
        'browser/autocomplete/autocomplete_edit_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_popup_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/autofill/address_field_unittest.cc',
        'browser/autofill/autofill_address_model_mac_unittest.mm',
        'browser/autofill/autofill_address_sheet_controller_mac_unittest.mm',
        'browser/autofill/autofill_common_unittest.cc',
        'browser/autofill/autofill_common_unittest.h',
        'browser/autofill/autofill_credit_card_model_mac_unittest.mm',
        'browser/autofill/autofill_credit_card_sheet_controller_mac_unittest.mm',
        'browser/autofill/autofill_dialog_controller_mac_unittest.mm',
        'browser/autofill/autofill_download_unittest.cc',
        'browser/autofill/autofill_field_unittest.cc',
        'browser/autofill/autofill_infobar_delegate_unittest.cc',
        'browser/autofill/autofill_manager_unittest.cc',
        'browser/autofill/autofill_profile_unittest.cc',
        'browser/autofill/autofill_type_unittest.cc',
        'browser/autofill/autofill_xml_parser_unittest.cc',
        'browser/autofill/billing_address_unittest.cc',
        'browser/autofill/contact_info_unittest.cc',
        'browser/autofill/credit_card_field_unittest.cc',
        'browser/autofill/credit_card_unittest.cc',
        'browser/autofill/fax_field_unittest.cc',
        'browser/autofill/form_structure_unittest.cc',
        'browser/autofill/name_field_unittest.cc',
        'browser/autofill/personal_data_manager_unittest.cc',
        'browser/autofill/phone_field_unittest.cc',
        'browser/autofill/phone_number_unittest.cc',
        'browser/automation/automation_provider_unittest.cc',
        'browser/back_forward_menu_model_unittest.cc',
        'browser/bookmarks/bookmark_codec_unittest.cc',
        'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
        'browser/bookmarks/bookmark_drag_data_unittest.cc',
        'browser/bookmarks/bookmark_html_writer_unittest.cc',
        'browser/bookmarks/bookmark_index_unittest.cc',
        'browser/bookmarks/bookmark_model_test_utils.cc',
        'browser/bookmarks/bookmark_model_test_utils.h',
        'browser/bookmarks/bookmark_model_unittest.cc',
        'browser/bookmarks/bookmark_utils_unittest.cc',
        'browser/browser_about_handler_unittest.cc',
        'browser/browser_accessibility_win_unittest.cc',
        'browser/browser_commands_unittest.cc',
        'browser/browser_theme_pack_unittest.cc',
        'browser/browser_theme_provider_unittest.cc',
        'browser/browser_unittest.cc',
        'browser/browsing_data_appcache_helper_unittest.cc',
        'browser/browsing_data_database_helper_unittest.cc',
        'browser/browsing_data_local_storage_helper_unittest.cc',
        'browser/child_process_security_policy_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/chrome_plugin_unittest.cc',
        'browser/chrome_thread_unittest.cc',
        'browser/chromeos/customization_document_unittest.cc',
        'browser/chromeos/external_cookie_handler_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/input_method/input_method_util_unittest.cc',
        'browser/chromeos/login/cookie_fetcher_unittest.cc',
        'browser/chromeos/login/google_authenticator_unittest.cc',
        'browser/chromeos/login/mock_auth_response_handler.cc',
        'browser/chromeos/options/language_config_model_unittest.cc',
        'browser/chromeos/pipe_reader_unittest.cc',
        'browser/chromeos/status/language_menu_button_unittest.cc',
        'browser/chromeos/version_loader_unittest.cc',
        'browser/content_setting_bubble_model_unittest.cc',
        'browser/content_setting_image_model_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.h',
        'browser/debugger/devtools_remote_message_unittest.cc',
        'browser/diagnostics/diagnostics_model_unittest.cc',
        # It is safe to list */cocoa/* files in the "common" file list
        # without an explicit exclusion since gyp is smart enough to
        # exclude them from non-Mac builds.
        'browser/cocoa/about_ipc_controller_unittest.mm',
        'browser/cocoa/about_window_controller_unittest.mm',
        'browser/cocoa/animatable_view_unittest.mm',
        'browser/cocoa/autocomplete_text_field_cell_unittest.mm',
        'browser/cocoa/autocomplete_text_field_editor_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest_helper.mm',
        'browser/cocoa/background_gradient_view_unittest.mm',
        'browser/cocoa/background_tile_view_unittest.mm',
        'browser/cocoa/base_view_unittest.mm',
        'browser/cocoa/bookmark_all_tabs_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_bridge_unittest.mm',
        'browser/cocoa/bookmark_bar_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_folder_button_cell_unittest.mm',
        'browser/cocoa/bookmark_bar_folder_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_folder_hover_state_unittest.mm',
        'browser/cocoa/bookmark_bar_folder_view_unittest.mm',
        'browser/cocoa/bookmark_bar_folder_window_unittest.mm',
        'browser/cocoa/bookmark_bar_toolbar_view_unittest.mm',
        'browser/cocoa/bookmark_bar_unittest_helper.h',
        'browser/cocoa/bookmark_bar_unittest_helper.mm',
        'browser/cocoa/bookmark_bar_view_unittest.mm',
        'browser/cocoa/bookmark_bubble_controller_unittest.mm',
        'browser/cocoa/bookmark_button_cell_unittest.mm',
        'browser/cocoa/bookmark_button_unittest.mm',
        'browser/cocoa/bookmark_editor_base_controller_unittest.mm',
        'browser/cocoa/bookmark_editor_controller_unittest.mm',
        'browser/cocoa/bookmark_folder_target_unittest.mm',
        'browser/cocoa/bookmark_menu_bridge_unittest.mm',
        'browser/cocoa/bookmark_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/bookmark_menu_unittest.mm',
        'browser/cocoa/bookmark_model_observer_for_cocoa_unittest.mm',
        'browser/cocoa/bookmark_name_folder_controller_unittest.mm',
        'browser/cocoa/bookmark_tree_browser_cell_unittest.mm',
        'browser/cocoa/browser_frame_view_unittest.mm',
        'browser/cocoa/browser_window_cocoa_unittest.mm',
        'browser/cocoa/browser_window_controller_unittest.mm',
        'browser/cocoa/bubble_view_unittest.mm',
        'browser/cocoa/bug_report_window_controller_unittest.mm',
        'browser/cocoa/chrome_browser_window_unittest.mm',
        'browser/cocoa/chrome_event_processing_window_unittest.mm',
        'browser/cocoa/clear_browsing_data_controller_unittest.mm',
        'browser/cocoa/clickhold_button_cell_unittest.mm',
        'browser/cocoa/cocoa_test_helper.h',
        'browser/cocoa/cocoa_test_helper.mm',
        'browser/cocoa/command_observer_bridge_unittest.mm',
        'browser/cocoa/content_blocked_bubble_controller_unittest.mm',
        'browser/cocoa/content_exceptions_window_controller_unittest.mm',
        'browser/cocoa/content_settings_dialog_controller_unittest.mm',
        'browser/cocoa/cookie_details_unittest.mm',
        'browser/cocoa/cookie_details_view_controller_unittest.mm',
        'browser/cocoa/cookie_prompt_window_controller_unittest.mm',
        'browser/cocoa/cookies_window_controller_unittest.mm',
        'browser/cocoa/custom_home_pages_model_unittest.mm',
        'browser/cocoa/delayedmenu_button_unittest.mm',
        'browser/cocoa/download_item_button_unittest.mm',
        'browser/cocoa/download_shelf_mac_unittest.mm',
        'browser/cocoa/download_shelf_view_unittest.mm',
        'browser/cocoa/download_util_mac_unittest.mm',
        'browser/cocoa/draggable_button_unittest.mm',
        'browser/cocoa/edit_search_engine_cocoa_controller_unittest.mm',
        'browser/cocoa/event_utils_unittest.mm',
        'browser/cocoa/extension_installed_bubble_controller_unittest.mm',
        'browser/cocoa/extensions/browser_actions_container_view_unittest.mm',
        'browser/cocoa/extensions/extension_install_prompt_controller_unittest.mm',
        'browser/cocoa/extensions/extension_popup_controller_unittest.mm',
        'browser/cocoa/fast_resize_view_unittest.mm',
        'browser/cocoa/find_bar_bridge_unittest.mm',
        'browser/cocoa/find_bar_cocoa_controller_unittest.mm',
        'browser/cocoa/find_bar_text_field_cell_unittest.mm',
        'browser/cocoa/find_bar_text_field_unittest.mm',
        'browser/cocoa/find_bar_view_unittest.mm',
        'browser/cocoa/find_pasteboard_unittest.mm',
        'browser/cocoa/floating_bar_backing_view_unittest.mm',
        'browser/cocoa/focus_tracker_unittest.mm',
        'browser/cocoa/font_language_settings_controller_unittest.mm',
        'browser/cocoa/fullscreen_window_unittest.mm',
        'browser/cocoa/geolocation_exceptions_window_controller_unittest.mm',
        'browser/cocoa/gradient_button_cell_unittest.mm',
        'browser/cocoa/history_menu_bridge_unittest.mm',
        'browser/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/html_dialog_window_controller_unittest.mm',
        'browser/cocoa/hung_renderer_controller_unittest.mm',
        'browser/cocoa/hyperlink_button_cell_unittest.mm',
        'browser/cocoa/image_utils_unittest.mm',
        'browser/cocoa/import_settings_dialog_unittest.mm',
        'browser/cocoa/info_bubble_view_unittest.mm',
        'browser/cocoa/info_bubble_window_unittest.mm',
        'browser/cocoa/infobar_container_controller_unittest.mm',
        'browser/cocoa/infobar_controller_unittest.mm',
        'browser/cocoa/infobar_gradient_view_unittest.mm',
        'browser/cocoa/keystone_glue_unittest.mm',
        'browser/cocoa/keyword_editor_cocoa_controller_unittest.mm',
        'browser/cocoa/location_bar_view_mac_unittest.mm',
        'browser/cocoa/menu_button_unittest.mm',
        'browser/cocoa/menu_controller_unittest.mm',
        'browser/cocoa/notifications/balloon_controller_unittest.mm',
        'browser/cocoa/nsimage_cache_unittest.mm',
        'browser/cocoa/nsmenuitem_additions_unittest.mm',
        'browser/cocoa/objc_method_swizzle_unittest.mm',
        'browser/cocoa/page_info_window_mac_unittest.mm',
        'browser/cocoa/preferences_window_controller_unittest.mm',
        'browser/cocoa/rwhvm_editcommand_helper_unittest.mm',
        'browser/cocoa/sad_tab_controller_unittest.mm',
        'browser/cocoa/sad_tab_view_unittest.mm',
        'browser/cocoa/search_engine_list_model_unittest.mm',
        'browser/cocoa/status_bubble_mac_unittest.mm',
        'browser/cocoa/status_icons/status_icon_mac_unittest.mm',
        'browser/cocoa/styled_text_field_cell_unittest.mm',
        'browser/cocoa/styled_text_field_test_helper.h',
        'browser/cocoa/styled_text_field_test_helper.mm',
        'browser/cocoa/styled_text_field_unittest.mm',
        'browser/cocoa/sync_customize_controller_unittest.mm',
        'browser/cocoa/tab_controller_unittest.mm',
        'browser/cocoa/tab_strip_controller_unittest.mm',
        'browser/cocoa/tab_strip_view_unittest.mm',
        'browser/cocoa/tab_view_unittest.mm',
        'browser/cocoa/tab_view_picker_table_unittest.mm',
        'browser/cocoa/table_row_nsimage_cache_unittest.mm',
        'browser/cocoa/task_manager_mac_unittest.mm',
        'browser/cocoa/test_event_utils.h',
        'browser/cocoa/test_event_utils.mm',
        'browser/cocoa/throbber_view_unittest.mm',
        'browser/cocoa/toolbar_controller_unittest.mm',
        'browser/cocoa/toolbar_view_unittest.mm',
        'browser/cocoa/translate_infobar_unittest.mm',
        'browser/cocoa/view_resizer_pong.h',
        'browser/cocoa/view_resizer_pong.mm',
        'browser/cocoa/web_drop_target_unittest.mm',
        'browser/cocoa/window_size_autosaver_unittest.mm',
        'browser/config_dir_policy_provider_unittest.cc',
        'browser/command_updater_unittest.cc',
        'browser/configuration_policy_pref_store_unittest.cc',
        'browser/configuration_policy_provider_mac_unittest.cc',
        'browser/configuration_policy_provider_win_unittest.cc',
        'browser/cookies_tree_model_unittest.cc',
        'browser/debugger/devtools_manager_unittest.cc',
        'browser/dom_ui/dom_ui_theme_source_unittest.cc',
        'browser/dom_ui/dom_ui_unittest.cc',
        'browser/dom_ui/html_dialog_tab_contents_delegate_unittest.cc',
        'browser/dom_ui/shown_sections_handler_unittest.cc',
        'browser/download/download_manager_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_manager_unittest.cc',
        'browser/download/save_package_unittest.cc',
        'browser/encoding_menu_controller_unittest.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/extension_menu_manager_unittest.cc',
        'browser/extensions/extension_messages_unittest.cc',
        'browser/extensions/extension_prefs_unittest.cc',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_updater_unittest.cc',
        'browser/extensions/extensions_quota_service_unittest.cc',
        'browser/extensions/extensions_service_unittest.cc',
        'browser/extensions/extensions_service_unittest.h',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/image_loading_tracker_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/file_watcher_unittest.cc',
        'browser/find_backend_unittest.cc',
        'browser/geolocation/fake_access_token_store.h',
        'browser/geolocation/geolocation_content_settings_map_unittest.cc',
        'browser/geolocation/geolocation_exceptions_table_model_unittest.cc',
        'browser/geolocation/geolocation_permission_context_unittest.cc',
        'browser/geolocation/geolocation_settings_state_unittest.cc',
        'browser/geolocation/gps_location_provider_unittest_linux.cc',
        'browser/geolocation/location_arbitrator_unittest.cc',
        'browser/geolocation/network_location_provider_unittest.cc',
        'browser/geolocation/wifi_data_provider_common_unittest.cc',
        'browser/geolocation/wifi_data_provider_unittest_win.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.mm',
        'browser/google_update_settings_unittest.cc',
        'browser/google_url_tracker_unittest.cc',
        'browser/gtk/bookmark_bar_gtk_unittest.cc',
        'browser/gtk/bookmark_editor_gtk_unittest.cc',
        'browser/gtk/bookmark_utils_gtk_unittest.cc',
        'browser/gtk/gtk_chrome_shrinkable_hbox_unittest.cc',
        'browser/gtk/gtk_expanded_container_unittest.cc',
        'browser/gtk/gtk_theme_provider_unittest.cc',
        'browser/gtk/keyword_editor_view_unittest.cc',
        'browser/gtk/options/cookies_view_unittest.cc',
        'browser/gtk/options/languages_page_gtk_unittest.cc',
        'browser/gtk/reload_button_gtk_unittest.cc',
        'browser/gtk/status_icons/status_tray_gtk_unittest.cc',
        'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/history/expire_history_backend_unittest.cc',
        'browser/history/history_backend_unittest.cc',
        'browser/history/history_querying_unittest.cc',
        'browser/history/history_types_unittest.cc',
        'browser/history/history_unittest.cc',
        'browser/history/query_parser_unittest.cc',
        'browser/history/snippet_unittest.cc',
        'browser/history/starred_url_database_unittest.cc',
        'browser/history/text_database_manager_unittest.cc',
        'browser/history/text_database_unittest.cc',
        'browser/history/thumbnail_database_unittest.cc',
        'browser/history/top_sites_unittest.cc',
        'browser/history/url_database_unittest.cc',
        'browser/history/visit_database_unittest.cc',
        'browser/history/visit_tracker_unittest.cc',
        'browser/host_content_settings_map_unittest.cc',
        'browser/host_zoom_map_unittest.cc',
        'browser/importer/firefox_importer_unittest.cc',
        'browser/importer/firefox_importer_unittest_messages_internal.h',
        'browser/importer/firefox_importer_unittest_utils.h',
        'browser/importer/firefox_importer_unittest_utils_mac.cc',
        'browser/importer/firefox_profile_lock_unittest.cc',
        'browser/importer/firefox_proxy_settings_unittest.cc',
        'browser/importer/importer_unittest.cc',
        'browser/importer/safari_importer_unittest.mm',
        'browser/importer/toolbar_importer_unittest.cc',
        'browser/in_process_webkit/dom_storage_dispatcher_host_unittest.cc',
        'browser/in_process_webkit/webkit_context_unittest.cc',
        'browser/in_process_webkit/webkit_thread_unittest.cc',
        'browser/pref_value_store_unittest.cc',
        'browser/keychain_mock_mac.cc',
        'browser/keychain_mock_mac.h',
        'browser/login_prompt_unittest.cc',
        'browser/mach_broker_mac_unittest.cc',
        'browser/managed_prefs_banner_base_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/mock_configuration_policy_provider.h',
        'browser/mock_configuration_policy_store.h',
        'browser/net/chrome_url_request_context_unittest.cc',
        'browser/net/connection_tester_unittest.cc',
        'browser/net/dns_host_info_unittest.cc',
        'browser/net/dns_master_unittest.cc',
        'browser/net/passive_log_collector_unittest.cc',
        'browser/net/resolve_proxy_msg_helper_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/notifications/notification_test_util.h',
        'browser/page_menu_model_unittest.cc',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/password_form_data.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_manager_unittest.cc',
        'browser/password_manager/password_store_default_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/password_manager/password_store_win_unittest.cc',
        'browser/password_manager/password_store_x_unittest.cc',
        'browser/pref_member_unittest.cc',
        'browser/pref_service_unittest.cc',
        'browser/preferences_mock_mac.cc',
        'browser/preferences_mock_mac.h',
        'browser/printing/print_dialog_cloud_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/privacy_blacklist/blacklist_interceptor_unittest.cc',
        'browser/privacy_blacklist/blacklist_unittest.cc',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/profile_manager_unittest.cc',
        'browser/renderer_host/audio_renderer_host_unittest.cc',
        'browser/renderer_host/render_widget_host_unittest.cc',
        'browser/renderer_host/resource_dispatcher_host_unittest.cc',
        'browser/renderer_host/resource_queue_unittest.cc',
        'browser/renderer_host/test/render_view_host_unittest.cc',
        'browser/renderer_host/test/site_instance_unittest.cc',
        'browser/renderer_host/web_cache_manager_unittest.cc',
        'browser/rlz/rlz_unittest.cc',
        'browser/safe_browsing/bloom_filter_unittest.cc',
        'browser/safe_browsing/chunk_range_unittest.cc',
        'browser/safe_browsing/protocol_manager_unittest.cc',
        'browser/safe_browsing/protocol_parser_unittest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
        'browser/safe_browsing/safe_browsing_database_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_file_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_sqlite_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_unittest_helper.cc',
        'browser/safe_browsing/safe_browsing_util_unittest.cc',
        'browser/search_engines/keyword_editor_controller_unittest.cc',
        'browser/search_engines/template_url_model_unittest.cc',
        'browser/search_engines/template_url_parser_unittest.cc',
        'browser/search_engines/template_url_prepopulate_data_unittest.cc',
        'browser/search_engines/template_url_scraper_unittest.cc',
        'browser/search_engines/template_url_unittest.cc',
        'browser/sessions/session_backend_unittest.cc',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/sessions/session_service_unittest.cc',
        'browser/sessions/tab_restore_service_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/ssl/ssl_host_state_unittest.cc',
        'browser/status_icons/status_icon_unittest.cc',
        'browser/status_icons/status_tray_unittest.cc',
        'browser/sync/abstract_profile_sync_service_test.h',
        'browser/sync/glue/autofill_data_type_controller_unittest.cc',
        'browser/sync/glue/autofill_model_associator_unittest.cc',
        'browser/sync/glue/bookmark_data_type_controller_unittest.cc',
        'browser/sync/glue/change_processor_mock.h',
        'browser/sync/glue/data_type_controller_mock.h',
        'browser/sync/glue/data_type_manager_impl_unittest.cc',
        'browser/sync/glue/data_type_manager_mock.h',
        'browser/sync/glue/database_model_worker_unittest.cc',
        'browser/sync/glue/extension_data_unittest.cc',
        'browser/sync/glue/extension_data_type_controller_unittest.cc',
        'browser/sync/glue/extension_util_unittest.cc',
        'browser/sync/glue/http_bridge_unittest.cc',
        'browser/sync/glue/preference_data_type_controller_unittest.cc',
        'browser/sync/glue/preference_model_associator_unittest.cc',
        'browser/sync/glue/sync_backend_host_mock.h',
        'browser/sync/glue/theme_data_type_controller_unittest.cc',
        'browser/sync/glue/theme_util_unittest.cc',
        'browser/sync/glue/typed_url_model_associator_unittest.cc',
        'browser/sync/glue/ui_model_worker_unittest.cc',
        'browser/sync/net/network_change_notifier_io_thread_unittest.cc',
        'browser/sync/profile_sync_factory_impl_unittest.cc',
        'browser/sync/profile_sync_factory_mock.cc',
        'browser/sync/profile_sync_factory_mock.h',
        'browser/sync/profile_sync_service_autofill_unittest.cc',
        'browser/sync/profile_sync_service_mock.h',
        'browser/sync/profile_sync_service_password_unittest.cc',
        'browser/sync/profile_sync_service_preference_unittest.cc',
        'browser/sync/profile_sync_service_startup_unittest.cc',
        'browser/sync/profile_sync_service_typed_url_unittest.cc',
        'browser/sync/profile_sync_service_unittest.cc',
        'browser/sync/profile_sync_test_util.h',
        'browser/sync/sync_setup_wizard_unittest.cc',
        'browser/sync/sync_ui_util_mac_unittest.mm',
        'browser/sync/test_profile_sync_service.h',
        'browser/sync/util/cryptographer_unittest.cc',
        'browser/sync/util/nigori_unittest.cc',
        'browser/tab_contents/navigation_controller_unittest.cc',
        'browser/tab_contents/navigation_entry_unittest.cc',
        'browser/tab_contents/render_view_host_manager_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/tab_contents/web_contents_unittest.cc',
        'browser/tab_menu_model_unittest.cc',
        'browser/tabs/pinned_tab_codec_unittest.cc',
        'browser/tabs/tab_strip_model_unittest.cc',
        'browser/task_manager_unittest.cc',
        'browser/theme_resources_util_unittest.cc',
        'browser/thumbnail_store_unittest.cc',
        'browser/translate/translate_manager2_unittest.cc',
        'browser/user_style_sheet_watcher_unittest.cc',
        'browser/views/accessibility_event_router_views_unittest.cc',
        'browser/views/bookmark_bar_view_unittest.cc',
        'browser/views/bookmark_context_menu_test.cc',
        'browser/views/bookmark_editor_view_unittest.cc',
        'browser/views/extensions/browser_action_drag_data_unittest.cc',
        'browser/views/generic_info_view_unittest.cc',
        'browser/views/info_bubble_unittest.cc',
        'browser/views/status_icons/status_tray_win_unittest.cc',
        'browser/visitedlink_unittest.cc',
        'browser/webdata/web_data_service_test_util.h',
        'browser/webdata/web_data_service_unittest.cc',
        'browser/webdata/web_database_unittest.cc',
        'browser/window_sizer_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/common_param_traits_unittest.cc',
        'common/deprecated/event_sys_unittest.cc',
        'common/desktop_notifications/active_notification_tracker_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_extent_unittest.cc',
        'common/extensions/extension_file_util_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_localization_peer_unittest.cc',
        'common/extensions/extension_manifests_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
        'common/extensions/update_manifest_unittest.cc',
        'common/extensions/url_pattern_unittest.cc',
        'common/extensions/user_script_unittest.cc',
        'common/font_descriptor_mac_unittest.mm',
        'common/important_file_writer_unittest.cc',
        'common/json_pref_store_unittest.cc',
        'common/json_value_serializer_unittest.cc',
        'common/mru_cache_unittest.cc',
        # TODO(sanjeevr): Move the 2 below files to common_net_unit_tests
        'common/net/gaia/gaia_authenticator_unittest.cc',
        'common/net/url_fetcher_unittest.cc',
        'common/net/test_url_fetcher_factory.cc',
        'common/net/test_url_fetcher_factory.h',
        'common/notification_service_unittest.cc',
        'common/process_watcher_unittest.cc',
        'common/property_bag_unittest.cc',
        'common/render_messages_unittest.cc',
        'common/resource_dispatcher_unittest.cc',
        'common/sandbox_mac_diraccess_unittest.mm',
        'common/sandbox_mac_fontloading_unittest.mm',
        'common/sandbox_mac_unittest_helper.h',
        'common/sandbox_mac_unittest_helper.mm',
        'common/sandbox_mac_system_access_unittest.mm',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_unittest.cc',
        'renderer/audio_message_filter_unittest.cc',
        'renderer/extensions/extension_api_json_validity_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/form_manager_unittest.cc',
        'renderer/media/audio_renderer_impl_unittest.cc',
        'renderer/net/render_dns_master_unittest.cc',
        'renderer/net/render_dns_queue_unittest.cc',
        'renderer/paint_aggregator_unittest.cc',
        'renderer/pepper_devices_unittest.cc',
        'renderer/render_process_unittest.cc',
        'renderer/render_thread_unittest.cc',
        'renderer/render_view_unittest.cc',
        'renderer/render_view_unittest_mac.mm',
        'renderer/render_widget_unittest.cc',
        'renderer/renderer_about_handler_unittest.cc',
        'renderer/renderer_main_unittest.cc',
        'renderer/spellchecker/spellcheck_unittest.cc',
        'renderer/spellchecker/spellcheck_worditerator_unittest.cc',
        'renderer/translate_helper_unittest.cc',
        'service/cloud_print/cloud_print_helpers_unittest.cc',
        'service/net/service_network_change_notifier_thread_unittest.cc',
        'test/browser_with_test_window_test.cc',
        'test/browser_with_test_window_test.h',
        'test/file_test_utils.cc',
        'test/file_test_utils.h',
        'test/menu_model_test.cc',
        'test/menu_model_test.h',
        'test/render_view_test.cc',
        'test/render_view_test.h',
        'test/sync/test_http_bridge_factory.h',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/v8_unit_test.cc',
        'test/v8_unit_test.h',
        'tools/convert_dict/convert_dict_unittest.cc',
        '../third_party/cld/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
      ],
      'conditions': [
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
        }],
        ['chromeos==0 and toolkit_views==0 and OS!="win"', {
          'sources/': [
             ['exclude', 'browser/views/info_bubble_unittest.cc'],
          ]
        }],
        ['OS=="linux" and selinux==0', {
          'dependencies': [
            '../sandbox/sandbox.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            [ 'gcc_version==44', {
              # Avoid gcc 4.4 strict aliasing issues in stl_tree.h when
              # building mru_cache_unittest.cc.
              'cflags': [
                '-fno-strict-aliasing',
              ],
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
          ],
          'sources!': [
            'browser/views/bookmark_bar_view_unittest.cc',
            'browser/views/bookmark_context_menu_test.cc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources': [
            '<@(views_unit_tests_sources)',
            '../views/focus/accelerator_handler_gtk_unittest.cc',
          ],
          # We must use 'sources/' instead of 'source!' as there is a
          # target-default 'sources/' including gtk_unittest and 'source/' takes
          # precedence over 'sources!'.
          'sources/': [
             ['exclude', 'browser/gtk/bookmark_bar_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/bookmark_editor_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/gtk_chrome_shrinkable_hbox_unittest\\.cc$'],
             ['exclude', 'browser/gtk/gtk_expanded_container_unittest\\.cc$'],
             ['exclude', 'browser/gtk/gtk_theme_provider_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/cookies_view_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/languages_page_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/reload_button_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/status_icons/status_tray_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/tabs/tab_renderer_gtk_unittest\\.cc$'],
             ['include', 'browser/views/bookmark_bar_view_unittest.cc$'],
             ['include', 'browser/views/bookmark_context_menu_test.cc$'],
          ],
        }],
        ['OS=="linux" and chromeos==1', {
          'sources': [
             'browser/chromeos/notifications/desktop_notifications_unittest.cc',
          ],
          'sources/': [
             ['exclude', 'browser/notifications/desktop_notifications_unittest.cc'],
          ]
        }],
        ['OS=="mac"', {
           # The test fetches resources which means Mac need the app bundle to
           # exist on disk so it can pull from it.
          'dependencies': [
            'chrome',
            '../third_party/ocmock/ocmock.gyp:ocmock',
          ],
          'include_dirs': [
            '../third_party/GTM',
            '../third_party/GTM/AppKit',
          ],
          'sources': [
            'browser/spellchecker_platform_engine_unittest.cc',
            'browser/translate/translate_manager_unittest.cc',
          ],
          'sources!': [
            # Blocked on bookmark manager.
            'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
            'browser/gtk/reload_button_gtk_unittest.cc',
            'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'browser/password_manager/password_store_default_unittest.cc',
            'browser/views/accessibility_event_router_views_unittest.cc',
            'browser/views/bookmark_bar_view_unittest.cc',
            'browser/views/bookmark_context_menu_test.cc',
            'browser/translate/translate_manager2_unittest.cc',
            'tools/convert_dict/convert_dict_unittest.cc',
            '../third_party/hunspell/google/hunspell_tests.cc',
          ],
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},

          # libwebcore.a is so large that ld may not have a sufficiently large
          # "hole" in its address space into which it can be mmaped by the
          # time it reaches this library. As of May 10, 2010, libwebcore.a is
          # about 1GB in some builds. In the Mac OS X 10.5 toolchain, using
          # Xcode 3.1, ld is only a 32-bit executable, and address space
          # exhaustion is the result, with ld failing and producing
          # the message:
          # ld: in .../libwebcore.a, can't map file, errno=12
          #
          # As a workaround, ensure that libwebcore.a appears to ld first when
          # linking unit_tests. This allows the library to be mmapped when
          # ld's address space is "wide open." Other libraries are small
          # enough that they'll be able to "squeeze" into the remaining holes.
          # The Mac linker isn't so sensitive that moving this library to the
          # front of the list will cause problems.
          #
          # Enough pluses to make get this target prepended to the target's
          # list of dependencies.
          'dependencies+++': [
            '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
        }, { # OS != "mac"
          'dependencies': [
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            'test_chrome_plugin',  # run time dependency
            '<(allocator_target)',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'app/chrome_dll.rc',
            'test/data/resource.rc',

            '<@(views_unit_tests_sources)',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'sources/': [
             ['exclude', 'browser/gtk/tabs/tab_renderer_gtk_unittest\\.cc$'],
          ],
          'link_settings': {
            'libraries': [
              '-lcomsupp.lib',
              '-loleacc.lib',
              '-lrpcrt4.lib',
              '-lurlmon.lib',
              '-lwinmm.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/bookmarks/bookmark_codec_unittest.cc',
            'browser/bookmarks/bookmark_drag_data_unittest.cc',
            'browser/browser_accessibility_win_unittest.cc',
            'browser/chrome_plugin_unittest.cc',
            'browser/extensions/extension_process_manager_unittest.cc',
            'browser/importer/importer_unittest.cc',
            'browser/login_prompt_unittest.cc',
            'browser/printing/print_job_unittest.cc',
            'browser/rlz/rlz_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/views/bookmark_editor_view_unittest.cc',
            'browser/views/extensions/browser_action_drag_data_unittest.cc',
            'browser/views/find_bar_host_unittest.cc',
            'browser/views/generic_info_view_unittest.cc',
            'browser/views/keyword_editor_view_unittest.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_guid': 'D7589D0D-304E-4589-85A4-153B7D84B07F',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'profile_import',
        'renderer',
        'test_support_common',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        # Test support sources
        'app/breakpad_mac_stubs.mm',
        'test/in_process_browser_test.cc',
        'test/in_process_browser_test.h',
        'test/test_launcher/out_of_proc_test_runner.cc',
        'test/test_launcher/test_runner.cc',
        'test/test_launcher/test_runner.h',
        'test/unit/chrome_test_suite.h',
        'browser/chromeos/login/wizard_in_process_browser_test.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.h',
        # Actual test sources
        'browser/autocomplete/autocomplete_browsertest.cc',
        'browser/browser_browsertest.cc',
        'browser/browser_init_browsertest.cc',
        'browser/browsing_data_database_helper_browsertest.cc',
        'browser/browsing_data_local_storage_helper_browsertest.cc',
        'browser/crash_recovery_browsertest.cc',
        'browser/dom_ui/file_browse_browsertest.cc',
        'browser/dom_ui/mediaplayer_browsertest.cc',
        'browser/download/save_page_browsertest.cc',
        'browser/extensions/alert_apitest.cc',
        'browser/extensions/app_background_page_apitest.cc',
        'browser/extensions/app_process_apitest.cc',
        'browser/extensions/autoupdate_interceptor.cc',
        'browser/extensions/autoupdate_interceptor.h',
        'browser/extensions/browser_action_apitest.cc',
        'browser/extensions/browser_action_test_util.h',
        'browser/extensions/content_script_all_frames_apitest.cc',
        'browser/extensions/content_script_extension_process_apitest.cc',
        'browser/extensions/cross_origin_xhr_apitest.cc',
        'browser/extensions/extension_devtools_browsertest.cc',
        'browser/extensions/extension_devtools_browsertest.h',
        'browser/extensions/extension_devtools_browsertests.cc',
        'browser/extensions/execute_script_apitest.cc',
        'browser/extensions/extension_apitest.cc',
        'browser/extensions/extension_apitest.h',
        'browser/extensions/extension_bookmarks_apitest.cc',
        'browser/extensions/extension_bookmarks_unittest.cc',
        'browser/extensions/extension_bookmark_manager_apitest.cc',
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_browsertest.h',
        'browser/extensions/extension_browsertests_misc.cc',
        'browser/extensions/extension_clipboard_apitest.cc',
        'browser/extensions/extension_context_menu_apitest.cc',
        'browser/extensions/extension_cookies_apitest.cc',
        'browser/extensions/extension_cookies_unittest.cc',
        'browser/extensions/extension_crash_recovery_browsertest.cc',
        'browser/extensions/extension_geolocation_apitest.cc',
        'browser/extensions/extension_get_views_apitest.cc',
        'browser/extensions/extension_history_apitest.cc',
        'browser/extensions/extension_idle_apitest.cc',
        'browser/extensions/extension_i18n_apitest.cc',
        'browser/extensions/extension_incognito_apitest.cc',
        'browser/extensions/extension_infobar_apitest.cc',
        'browser/extensions/extension_install_ui_browsertest.cc',
        'browser/extensions/extension_javascript_url_apitest.cc',
        'browser/extensions/extension_management_browsertest.cc',
        'browser/extensions/extension_messages_apitest.cc',
        'browser/extensions/extension_metrics_apitest.cc',
        'browser/extensions/extension_omnibox_apitest.cc',
        'browser/extensions/extension_override_apitest.cc',
        'browser/extensions/extension_processes_apitest.cc',
        'browser/extensions/extension_startup_browsertest.cc',
        'browser/extensions/extension_storage_apitest.cc',
        'browser/extensions/extension_tabs_apitest.cc',
        'browser/extensions/extension_toolbar_model_browsertest.cc',
        'browser/extensions/extension_toolstrip_apitest.cc',
        'browser/extensions/extension_websocket_apitest.cc',
        'browser/extensions/fragment_navigation_apitest.cc',
        'browser/extensions/isolated_world_apitest.cc',
        'browser/extensions/notifications_apitest.cc',
        'browser/extensions/page_action_apitest.cc',
        'browser/extensions/permissions_apitest.cc',
        'browser/extensions/stubs_apitest.cc',
        'browser/find_bar_host_browsertest.cc',
        'browser/geolocation/access_token_store_browsertest.cc',
        'browser/geolocation/geolocation_browsertest.cc',
        'browser/net/cookie_policy_browsertest.cc',
        'browser/net/ftp_browsertest.cc',
        'browser/printing/print_dialog_cloud_uitest.cc',
        'browser/sessions/session_restore_browsertest.cc',
        'browser/ssl/ssl_browser_tests.cc',
        'browser/task_manager_browsertest.cc',
        'renderer/form_autocomplete_browsertest.cc',
        'test/automation/dom_automation_browsertest.cc',
        'test/render_view_test.cc',
        'test/render_view_test.h',
      ],
      'conditions': [
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
            ['exclude', '^browser/dom_ui/mediaplayer_browsertest.cc'],
            ['exclude', '^browser/dom_ui/file_browse_browsertest.cc'],
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_dll_version.rc.version',
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
            '<@(browser_tests_sources_win_specific)',
            '<@(browser_tests_sources_views_specific)'
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          }
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
          'sources': [
            '<@(browser_tests_sources_views_specific)',
          ],
        }],
        ['OS=="linux" and chromeos==1', {
          'sources': [
            'browser/chromeos/cros/cros_in_process_browser_test.cc',
            'browser/chromeos/cros/cros_in_process_browser_test.h',
            'browser/chromeos/cros/mock_cros_library.h',
            'browser/chromeos/cros/mock_cryptohome_library.h',
            'browser/chromeos/cros/mock_keyboard_library.h',
            'browser/chromeos/cros/mock_language_library.h',
            'browser/chromeos/cros/mock_mount_library.cc',
            'browser/chromeos/cros/mock_mount_library.h',
            'browser/chromeos/cros/mock_network_library.h',
            'browser/chromeos/cros/mock_power_library.h',
            'browser/chromeos/cros/mock_speech_synthesis_library.h',
            'browser/chromeos/cros/mock_synaptics_library.h',
            'browser/chromeos/login/account_screen_browsertest.cc',
            'browser/chromeos/login/login_browsertest.cc',
            'browser/chromeos/login/login_screen_browsertest.cc',
            'browser/chromeos/login/mock_authenticator.h',
            'browser/chromeos/login/network_screen_browsertest.cc',
            'browser/chromeos/login/screen_locker_browsertest.cc',
            'browser/chromeos/login/screen_locker_tester.cc',
            'browser/chromeos/login/screen_locker_tester.h',
            'browser/chromeos/login/wizard_controller_browsertest.cc',
            'browser/chromeos/notifications/notification_browsertest.cc',
            'browser/chromeos/options/wifi_config_view_browsertest.cc',
            'browser/chromeos/panels/panel_browsertest.cc',
            'browser/chromeos/status/clock_menu_button_browsertest.cc',
            'browser/chromeos/status/language_menu_button_browsertest.cc',
            'browser/chromeos/status/power_menu_button_browsertest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==0', {
          'sources': [
            'browser/extensions/browser_action_test_util_gtk.cc',
            'browser/gtk/view_id_util_browsertest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'browser/extensions/browser_action_test_util_mac.mm',
          ],
          'include_dirs': [
            '../third_party/GTM',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      'target_name': 'startup_tests',
      'type': 'executable',
      'msvs_guid': 'D3E6C0FD-54C7-4FF2-9AE1-72F2DAFD820C',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'test_support_ui',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/startup/feature_startup_test.cc',
        'test/startup/shutdown_test.cc',
        'test/startup/startup_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        },],
        ['OS=="linux" or OS=="freebsd"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      # To run the tests from page_load_test.cc on Linux, we need to:
      #
      #   a) Build with Breakpad (GYP_DEFINES="linux_chromium_breakpad=1")
      #   b) Run with CHROME_HEADLESS=1 to generate crash dumps.
      #   c) Strip the binary if it's a debug build. (binary may be over 2GB)
      'target_name': 'reliability_tests',
      'type': 'executable',
      'msvs_guid': '8A3E1774-1DE9-445C-982D-3EE37C8A752A',
      'dependencies': [
        'browser',
        'chrome',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/page_load_test.cc',
        'test/reliability/page_load_test.h',
        'test/reliability/reliability_test_suite.h',
        'test/reliability/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        },],
      ],
    },
    {
      'target_name': 'page_cycler_tests',
      'type': 'executable',
      'msvs_guid': 'C9E0BD1D-B175-4A91-8380-3FDC81FAB9D7',
      'dependencies': [
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/page_cycler/page_cycler_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'tab_switching_test',
      'type': 'executable',
      'msvs_guid': 'A34770EA-A574-43E8-9327-F79C04770E98',
      'run_as': {
        'action': ['$(TargetPath)', '--gtest_print_time'],
      },
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/tab_switching/tab_switching_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
      ],
    },
    {
      'target_name': 'memory_test',
      'type': 'executable',
      'msvs_guid': 'A5F831FD-9B9C-4FEF-9FBA-554817B734CE',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/memory_test/memory_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      'target_name': 'url_fetch_test',
      'type': 'executable',
      'msvs_guid': '7EFD0C91-198E-4043-9E71-4A4C7879B929',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/url_fetch_test/url_fetch_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
        }], # OS="win"
      ], # conditions
    },
    {
      'target_name': 'common_net_test_support',
      'type': '<(library)',
      'sources': [
        'common/net/fake_network_change_notifier_thread.cc',
        'common/net/fake_network_change_notifier_thread.h',
        'common/net/thread_blocker.cc',
        'common/net/thread_blocker.h',
      ],
      'dependencies': [
        'common_net',
        '../base/base.gyp:base',
        '../net/net.gyp:net_base',
      ],
    },
    {
      # TODO(akalin): Add this to all.gyp.
      'target_name': 'common_net_unit_tests',
      'type': 'executable',
      'sources': [
        # TODO(akalin): Write our own test suite and runner.
        '../base/test/run_all_unittests.cc',
        '../base/test/test_suite.h',
        'common/net/fake_network_change_notifier_thread_unittest.cc',
        'common/net/mock_network_change_observer.h',
        'common/net/network_change_notifier_proxy_unittest.cc',
        'common/net/network_change_observer_proxy_unittest.cc',
        'common/net/thread_blocker_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'common_net',
        'common_net_test_support',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      # TODO(akalin): Remove this once we have our own test suite and
      # runner.
      'conditions': [
        ['OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'dependencies': [
            # Needed to handle the #include chain:
            #   base/test/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'notifier_unit_tests',
      'type': 'executable',
      'sources': [
        # TODO(akalin): Write our own test suite and runner.
        '../base/test/run_all_unittests.cc',
        '../base/test/test_suite.h',
        'common/net/notifier/listener/talk_mediator_unittest.cc',
        'common/net/notifier/listener/send_update_task_unittest.cc',
        'common/net/notifier/listener/subscribe_task_unittest.cc',
        'common/net/notifier/listener/xml_element_util_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'common_net_test_support',
        'notifier',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      # TODO(akalin): Remove this once we have our own test suite and
      # runner.
      'conditions': [
        ['OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'dependencies': [
            # Needed to handle the #include chain:
            #   base/test/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'browser/sync/engine/all_status_unittest.cc',
        'browser/sync/engine/apply_updates_command_unittest.cc',
        'browser/sync/engine/auth_watcher_unittest.cc',
        'browser/sync/engine/download_updates_command_unittest.cc',
        'browser/sync/engine/mock_model_safe_workers.h',
        'browser/sync/engine/process_commit_response_command_unittest.cc',
        'browser/sync/engine/syncapi_unittest.cc',
        'browser/sync/engine/syncer_proto_util_unittest.cc',
        'browser/sync/engine/syncer_thread_unittest.cc',
        'browser/sync/engine/syncer_unittest.cc',
        'browser/sync/engine/syncproto_unittest.cc',
        'browser/sync/engine/verify_updates_command_unittest.cc',
        'browser/sync/glue/change_processor_mock.h',
        'browser/sync/profile_sync_factory_mock.cc',
        'browser/sync/profile_sync_factory_mock.h',
        'browser/sync/sessions/ordered_commit_set_unittest.cc',
        'browser/sync/sessions/status_controller_unittest.cc',
        'browser/sync/sessions/sync_session_unittest.cc',
        'browser/sync/syncable/directory_backing_store_unittest.cc',
        'browser/sync/syncable/syncable_id_unittest.cc',
        'browser/sync/syncable/syncable_unittest.cc',
        'browser/sync/util/channel_unittest.cc',
        'browser/sync/util/crypto_helpers_unittest.cc',
        'browser/sync/util/extensions_activity_monitor_unittest.cc',
        'browser/sync/util/user_settings_unittest.cc',
        'test/file_test_utils.cc',
        'test/sync/engine/mock_gaia_authenticator.cc',
        'test/sync/engine/mock_gaia_authenticator.h',
        'test/sync/engine/mock_gaia_authenticator_unittest.cc',
        'test/sync/engine/mock_server_connection.cc',
        'test/sync/engine/mock_server_connection.h',
        'test/sync/engine/syncer_command_test.h',
        'test/sync/engine/test_directory_setter_upper.cc',
        'test/sync/engine/test_directory_setter_upper.h',
        'test/sync/engine/test_id_factory.h',
        'test/sync/engine/test_syncable_utils.cc',
        'test/sync/engine/test_syncable_utils.h',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'common',
        'common_net_test_support',
        'debugger',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'profile_import',
        'syncapi',
        'test_support_unit',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lcrypt32.lib',
              '-lws2_32.lib',
              '-lsecur32.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            'packed_resources'
          ],
        }],
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          'dependencies': [
            'helper_app'
          ],
        },{  # OS!="mac"
          'dependencies': [
            'packed_extra_resources',
          ],
        }],
        ['OS=="linux" and chromeos==1', {
          'include_dirs': [
            '<(grit_out_dir)',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_resources',
        'common',
        'debugger',
        'profile_import',
        'renderer',
        'chrome_strings',
        'test_support_common',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      # TODO(phajdan.jr): Only temporary, to make transition easier.
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'app/chrome_dll_resource.h',
        'browser/autofill/autofill_common_unittest.cc',
        'browser/autofill/autofill_common_unittest.h',
        'test/in_process_browser_test.cc',
        'test/in_process_browser_test.h',
        'test/test_launcher/out_of_proc_test_runner.cc',
        'test/test_launcher/test_runner.cc',
        'test/test_launcher/test_runner.h',
        'test/live_sync/bookmark_model_verifier.cc',
        'test/live_sync/bookmark_model_verifier.h',
        'test/live_sync/live_sync_test.cc',
        'test/live_sync/live_sync_test.h',
        'test/live_sync/profile_sync_service_test_harness.cc',
        'test/live_sync/profile_sync_service_test_harness.h',
        'test/live_sync/single_client_live_bookmarks_sync_test.cc',
        'test/live_sync/single_client_live_preferences_sync_test.cc',
        'test/live_sync/two_client_live_autofill_sync_test.cc',
        'test/live_sync/two_client_live_bookmarks_sync_test.cc',
        'test/live_sync/two_client_live_preferences_sync_test.cc',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/testing_browser_process.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.cc',
        'test/ui_test_utils_win.cc',
        'test/data/resource.h',
      ],
      'conditions': [
        # Plugin code.
        ['OS=="linux" or OS=="win"', {
          'dependencies': [
            'plugin',
           ],
          'export_dependent_settings': [
            'plugin',
          ],
        }],
        ['OS=="linux"', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
           ],
        }],
        ['OS=="win"', {
          'sources': [
            'app/chrome_dll.rc',
            'app/chrome_dll_version.rc.version',
            'test/data/resource.rc',
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../views/views.gyp:views',
            '../sandbox/sandbox.gyp:sandbox',
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'plugin_tests',
      'type': 'executable',
      'msvs_guid': 'A1CAA831-C507-4B2E-87F3-AEC63C9907F9',
      'dependencies': [
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'test_support_common',
        'test_support_ui',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/plugin/plugin_test.cpp',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
            'security_tests',  # run time dependency
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        },],
      ],
    },
  ],
  'conditions': [
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'perf_tests',
          'type': 'executable',
          'msvs_guid': '9055E088-25C6-47FD-87D5-D9DD9FD75C9F',
          'dependencies': [
            'browser',
            'common',
            'debugger',
            'renderer',
            'chrome_resources',
            'chrome_strings',
            '../app/app.gyp:app_base',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../webkit/support/webkit_support.gyp:glue',
          ],
          'sources': [
            'browser/privacy_blacklist/blacklist_perftest.cc',
            'browser/safe_browsing/filter_false_positive_perftest.cc',
            'browser/visitedlink_perftest.cc',
            'common/json_value_serializer_perftest.cc',
            'test/perf/perftests.cc',
            'test/perf/url_parse_perftest.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
              'sources!': [
                # TODO(port):
                'browser/safe_browsing/filter_false_positive_perftest.cc',
                'browser/visitedlink_perftest.cc',
              ],
            }],
            ['toolkit_views==1', {
              'dependencies': [
                '../views/views.gyp:views',
              ],
            }],
            ['OS=="win"', {
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
        },
        # TODO(port): enable on mac.
        {
          'includes': ['test/interactive_ui/interactive_ui_tests.gypi']
        },
      ],
    },],  # OS!="mac"
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'security_tests',
          'type': 'shared_library',
          'msvs_guid': 'E750512D-FC7C-4C98-BF04-0A0DAF882055',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'test/injection_test_dll.h',
            'test/security_tests/ipc_security_tests.cc',
            'test/security_tests/ipc_security_tests.h',
            'test/security_tests/security_tests.cc',
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
          ],
        },
        {
          'target_name': 'selenium_tests',
          'type': 'executable',
          'msvs_guid': 'E3749617-BA3D-4230-B54C-B758E56D9FA5',
          'dependencies': [
            'chrome_resources',
            'chrome_strings',
            'test_support_common',
            'test_support_ui',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'test/selenium/selenium_test.cc',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(allocator_target)',
              ],
            },],
          ],
        },
        {
          'target_name': 'test_chrome_plugin',
          'type': 'shared_library',
          'msvs_guid': '7F0A70F6-BE3F-4C19-B435-956AB8F30BA4',
          'dependencies': [
            '../base/base.gyp:base',
            '../build/temp_gyp/googleurl.gyp:googleurl',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '-lwinmm.lib',
            ],
          },
          'sources': [
            'test/chrome_plugin/test_chrome_plugin.cc',
            'test/chrome_plugin/test_chrome_plugin.def',
            'test/chrome_plugin/test_chrome_plugin.h',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    # Build on linux x86_64 only if linux_fpic==1
    ['OS=="mac" or OS=="win" or (OS=="linux" and target_arch==python_arch '
     'and (target_arch!="x64" or linux_fpic==1))', {
      'targets': [
        {
          # Documentation: http://dev.chromium.org/developers/pyauto
          'target_name': 'pyautolib',
          'type': 'shared_library',
          'product_prefix': '_',
          'dependencies': [
            'chrome',
            'debugger',
            'syncapi',
            'test_support_common',
            'chrome_resources',
            'chrome_strings',
            'theme_resources',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
          ],
          'export_dependent_settings': [
            'test_support_common',
          ],
          'include_dirs': [
            '..',
          ],
          'cflags': [
             '-Wno-uninitialized',
          ],
          'sources': [
            'test/pyautolib/pyautolib.cc',
            'test/pyautolib/pyautolib.h',
            'test/ui/ui_test.cc',
            'test/ui/ui_test.h',
            'test/ui/ui_test_suite.cc',
            'test/ui/ui_test_suite.h',
            '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
            '<@(pyautolib_sources)',
          ],
          'xcode_settings': {
            # Need a shared object named _pyautolib.so (not libpyautolib.dylib
            # that xcode would generate)
            # Change when gyp can support a platform-neutral way for this
            # (http://code.google.com/p/gyp/issues/detail?id=135)
            'EXECUTABLE_EXTENSION': 'so',
            # When generated, pyautolib_wrap.cc includes some swig support
            # files which, as of swig 1.3.31 that comes with 10.5 and 10.6,
            # may not compile cleanly at -Wall.
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',  # -Wno-error
          },
          'conditions': [
            ['OS=="linux"', {
              'include_dirs': [
                '..',
                '<(sysroot)/usr/include/python<(python_ver)',
              ],
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
              'link_settings': {
                'libraries': [
                  '-lpython<(python_ver)',
                ],
              },
            }],
            ['OS=="mac"', {
              'include_dirs': [
                '..',
                '$(SDKROOT)/usr/include/python2.5',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libpython2.5.dylib',
                ],
              }
            }],
            ['OS=="win"', {
              'include_dirs': [
                '..',
                '../third_party/python_24/include',
              ],
              'link_settings': {
                'libraries': [
                  '../third_party/python_24/libs/python24.lib',
                ],
              }
            }],
          ],
          'actions': [
            {
              'action_name': 'pyautolib_swig',
              'inputs': [
                'test/pyautolib/argc_argv.i',
                'test/pyautolib/pyautolib.i',
                '<@(pyautolib_sources)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                '<(PRODUCT_DIR)/pyautolib.py',
              ],
              'action': [ 'python',
                          '../tools/swig/swig.py',
                          '-I..',
                          '-python',
                          '-c++',
                          '-outdir',
                          '<(PRODUCT_DIR)',
                          '-o',
                          '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                          'test/pyautolib/pyautolib.i',
              ],
              'message': 'Generating swig wrappers for pyautolib.',
            },
          ],  # actions
        },  # target 'pyautolib'
      ]  # targets
    }],
    # To enable the coverage targets, do
    #    GYP_DEFINES='coverage=1' gclient sync
    # To match the coverage buildbot more closely, do this:
    #    GYP_DEFINES='coverage=1 enable_svg=0 fastbuild=1' gclient sync
    # (and, on MacOS, be sure to switch your SDK from "Base SDK" to "Mac OS X 10.6")
    ['coverage!=0',
      { 'targets': [
        {
          ### Coverage BUILD AND RUN.
          ### Not named coverage_build_and_run for historical reasons.
          'target_name': 'coverage',
          'dependencies': [ 'coverage_build', 'coverage_run' ],
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              'message': 'Coverage is now complete.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage-build-and-run.stamp' ],
              'action_name': 'coverage',
              # Wish gyp had some basic builtin commands (e.g. 'touch').
              'action': [ 'python', '-c',
                          'import os; ' \
                          'open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage-build-and-run.stamp\'' \
                          ', \'w\').close()' ],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage BUILD.  Compile only; does not run the bundles.
        ### Intended as the build phase for our coverage bots.
        ###
        ### Builds unit test bundles needed for coverage.
        ### Outputs this list of bundles into coverage_bundles.py.
        ###
        ### If you want to both build and run coverage from your IDE,
        ### use the 'coverage' target.
        {
          'target_name': 'coverage_build',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'automated_ui_tests',
            '../app/app.gyp:app_unittests',
            '../base/base.gyp:base_unittests',
            'browser_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            'nacl_ui_tests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            # ui_tests seem unhappy on both Mac and Win when run under
            # coverage (all tests fail, often with a
            # "server_->WaitForInitialLoads()").  TODO(jrg):
            # investigate why.
            # 'ui_tests',
            'unit_tests',
          ],  # 'dependencies'
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                # Courgette has not been ported from Windows.
                # Note build/win/chrome_win.croc uniquely has the
                # courgette source directory in an include path.
                '../courgette/courgette.gyp:courgette_unittests',
                ]}],
            ['OS=="linux"', {
              'dependencies': [
                # Reason for disabling UI tests on non-Linux above.
                'ui_tests',
                # Win bot needs to be turned into an interactive bot.
                'test/interactive_ui/interactive_ui_tests.gypi:interactive_ui_tests',
              ]}],
            ['OS=="mac"', {
              'dependencies': [
              # Placeholder; empty for now.
              ]}],
          ],  # 'conditions'
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Compiling coverage bundles.',
              # MSVS must have an input file and an output file.
              #
              # TODO(jrg):
              # Technically I want inputs to be the list of
              # executables created in <@(_dependencies) but use of
              # that variable lists the dep by dep name, not their
              # output executable name.
              # Is there a better way to force this action to run, always?
              #
              # If a test bundle is added to this coverage_build target it
              # necessarily means this file (chrome_tests.gypi) is changed,
              # so the action is run (coverage_bundles.py is generated).
              # Exceptions to that rule are theoretically possible
              # (e.g. re-gyp with a GYP_DEFINES set).
              # Else it's the same list of bundles as last time.  They are
              # built (since on the deps list) but the action may not run.
              # For now, things work, but it's less than ideal.
              'inputs': [ 'chrome_tests.gypi' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'action_name': 'coverage_build',
              'action': [ 'python', '-c',
                          'import os; '
                          'f = open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage_bundles.py\'' \
                          ', \'w\'); ' \
                          'deplist = \'' \
                          '<@(_dependencies)' \
                          '\'.split(\' \'); ' \
                          'f.write(str(deplist)); ' \
                          'f.close()'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage RUN.  Does not compile the bundles.  Mirrors the
        ### run_coverage_bundles buildbot phase.  If you update this
        ### command update the mirror in
        ### $BUILDBOT/scripts/master/factory/chromium_commands.py.
        ### If you want both build and run, use the 'coverage' target.
        {
          'target_name': 'coverage_run',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Running the coverage script.  NOT building anything.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'action_name': 'coverage_run',
              'action': [ 'python',
                          '../tools/code_coverage/coverage_posix.py',
                          '--directory',
                          '<(PRODUCT_DIR)',
                          '--src_root',
                          '..',
                          '--bundles',
                          '<(PRODUCT_DIR)/coverage_bundles.py'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
      ]
    }],  # 'coverage!=0'
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
