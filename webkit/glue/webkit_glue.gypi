# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../..',
      },{
        'webkit_src_dir': '../../third_party/WebKit',
      }],
    ],

    'grit_info_cmd': ['python', '<(DEPTH)/tools/grit/grit_info.py'],
    'grit_cmd': ['python', '<(DEPTH)/tools/grit/grit.py'],
  },
  'targets': [
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'msvs_guid': '0B469837-3D46-484A-AFB3-C5A6C68730B9',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'input_path': './webkit_resources.grd',
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(input_path)', 'build',
                     '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
        {
          'action_name': 'webkit_chromium_resources',
          'variables': {
            'input_path': '<(webkit_src_dir)/WebKit/chromium/WebKit.grd',
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(input_path)', 'build',
                     '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['<(DEPTH)/build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'msvs_guid': '60B43839-95E6-4526-A661-209F16335E0E',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'input_path': './webkit_strings.grd',
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(input_path)', 'build',
                     '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['<(DEPTH)/build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webkit_user_agent',
      'type': '<(library)',
      'msvs_guid': 'DB162DE1-7D56-4C4A-8A9F-80D396CD7AA8',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/base/base.gyp:base_i18n',
      ],
      'actions': [
        {
          'action_name': 'webkit_version',
          'inputs': [
            '../build/webkit_version.py',
            '<(webkit_src_dir)/WebCore/Configurations/Version.xcconfig',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/webkit_version.h',
          ],
          'action': ['python', '<@(_inputs)', '<(INTERMEDIATE_DIR)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        'user_agent.cc',
        'user_agent.h',
      ],
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'conditions': [
      ],
    },
    {
      'target_name': 'glue',
      'type': '<(library)',
      'msvs_guid': 'C66B126D-0ECE-4CA2-B6DC-FA780AFBBF09',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/ppapi/ppapi.gyp:ppapi_c',
        'webkit_resources',
        'webkit_strings',
        'webkit_user_agent',
      ],
      'actions': [
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        'media/buffered_data_source.cc',
        'media/buffered_data_source.h',
        'media/media_resource_loader_bridge_factory.cc',
        'media/media_resource_loader_bridge_factory.h',
        'media/simple_data_source.cc',
        'media/simple_data_source.h',
        'media/video_renderer_impl.cc',
        'media/video_renderer_impl.h',
        'media/web_data_source.cc',
        'media/web_data_source.h',
        'media/web_video_renderer.h',
        'plugins/carbon_plugin_window_tracker_mac.h',
        'plugins/carbon_plugin_window_tracker_mac.cc',
        'plugins/coregraphics_private_symbols_mac.h',
        'plugins/default_plugin_shared.h',
        'plugins/nphostapi.h',
        'plugins/gtk_plugin_container.h',
        'plugins/gtk_plugin_container.cc',
        'plugins/gtk_plugin_container_manager.h',
        'plugins/gtk_plugin_container_manager.cc',
        'plugins/npapi_extension_thunk.cc',
        'plugins/npapi_extension_thunk.h',
        'plugins/pepper_audio.cc',
        'plugins/pepper_audio.h',
        'plugins/pepper_buffer.cc',
        'plugins/pepper_buffer.h',
        'plugins/pepper_char_set.cc',
        'plugins/pepper_char_set.h',
        'plugins/pepper_cursor_control.cc',
        'plugins/pepper_cursor_control.h',
        'plugins/pepper_directory_reader.cc',
        'plugins/pepper_directory_reader.h',
        'plugins/pepper_error_util.cc',
        'plugins/pepper_error_util.h',
        'plugins/pepper_event_conversion.cc',
        'plugins/pepper_event_conversion.h',
        'plugins/pepper_file_callbacks.cc',
        'plugins/pepper_file_callbacks.h',
        'plugins/pepper_file_chooser.cc',
        'plugins/pepper_file_chooser.h',
        'plugins/pepper_file_io.cc',
        'plugins/pepper_file_io.h',
        'plugins/pepper_file_ref.cc',
        'plugins/pepper_file_ref.h',
        'plugins/pepper_file_system.cc',
        'plugins/pepper_file_system.h',
        'plugins/pepper_font.cc',
        'plugins/pepper_font.h',
        'plugins/pepper_graphics_2d.cc',
        'plugins/pepper_graphics_2d.h',
        'plugins/pepper_image_data.cc',
        'plugins/pepper_image_data.h',
        'plugins/pepper_plugin_delegate.h',
        'plugins/pepper_plugin_instance.cc',
        'plugins/pepper_plugin_instance.h',
        'plugins/pepper_plugin_module.cc',
        'plugins/pepper_plugin_module.h',
        'plugins/pepper_plugin_object.cc',
        'plugins/pepper_plugin_object.h',
        'plugins/pepper_private.cc',
        'plugins/pepper_private.h',
        'plugins/pepper_private2.cc',
        'plugins/pepper_private2.h',
        'plugins/pepper_private2_linux.cc',
        'plugins/pepper_resource_tracker.cc',
        'plugins/pepper_resource_tracker.h',
        'plugins/pepper_resource.cc',
        'plugins/pepper_resource.h',
        'plugins/pepper_scrollbar.cc',
        'plugins/pepper_scrollbar.h',
        'plugins/pepper_string.cc',
        'plugins/pepper_string.h',
        'plugins/pepper_transport.cc',
        'plugins/pepper_transport.h',
        'plugins/pepper_url_loader.cc',
        'plugins/pepper_url_loader.h',
        'plugins/pepper_url_request_info.cc',
        'plugins/pepper_url_request_info.h',
        'plugins/pepper_url_response_info.cc',
        'plugins/pepper_url_response_info.h',
        'plugins/pepper_url_util.cc',
        'plugins/pepper_url_util.h',
        'plugins/pepper_var.cc',
        'plugins/pepper_var.h',
        'plugins/pepper_video_decoder.cc',
        'plugins/pepper_video_decoder.h',
        'plugins/pepper_webplugin_impl.cc',
        'plugins/pepper_webplugin_impl.h',
        'plugins/pepper_widget.cc',
        'plugins/pepper_widget.h',
        'plugins/plugin_constants_win.h',
        'plugins/plugin_group.cc',
        'plugins/plugin_group.h',
        'plugins/plugin_host.cc',
        'plugins/plugin_host.h',
        'plugins/plugin_instance.cc',
        'plugins/plugin_instance.h',
        'plugins/plugin_instance_mac.mm',
        'plugins/plugin_lib.cc',
        'plugins/plugin_lib.h',
        'plugins/plugin_lib_mac.mm',
        'plugins/plugin_lib_posix.cc',
        'plugins/plugin_lib_win.cc',
        'plugins/plugin_list.cc',
        'plugins/plugin_list.h',
        'plugins/plugin_list_mac.mm',
        'plugins/plugin_list_posix.cc',
        'plugins/plugin_list_win.cc',
        'plugins/plugin_stream.cc',
        'plugins/plugin_stream.h',
        'plugins/plugin_stream_posix.cc',
        'plugins/plugin_stream_url.cc',
        'plugins/plugin_stream_url.h',
        'plugins/plugin_stream_win.cc',
        'plugins/plugin_string_stream.cc',
        'plugins/plugin_string_stream.h',
        'plugins/plugin_stubs.cc',
        'plugins/plugin_switches.cc',
        'plugins/plugin_switches.h',
        'plugins/plugin_web_event_converter_mac.h',
        'plugins/plugin_web_event_converter_mac.mm',
        'plugins/ppb_private.h',
        'plugins/quickdraw_drawing_manager_mac.h',
        'plugins/quickdraw_drawing_manager_mac.cc',
        'plugins/webview_plugin.cc',
        'plugins/webview_plugin.h',
        'plugins/webplugin.cc',
        'plugins/webplugin.h',
        'plugins/webplugin_2d_device_delegate.h',
        'plugins/webplugin_3d_device_delegate.h',
        'plugins/webplugin_accelerated_surface_mac.h',
        'plugins/webplugin_delegate.h',
        'plugins/webplugin_delegate_impl.cc',
        'plugins/webplugin_delegate_impl.h',
        'plugins/webplugin_delegate_impl_gtk.cc',
        'plugins/webplugin_delegate_impl_mac.mm',
        'plugins/webplugin_delegate_impl_win.cc',
        'plugins/webplugin_impl.cc',
        'plugins/webplugin_impl.h',
        'plugins/webplugininfo.cc',
        'plugins/webplugininfo.h',
        'alt_error_page_resource_fetcher.cc',
        'alt_error_page_resource_fetcher.h',
        'context_menu.cc',
        'context_menu.h',
        'cpp_binding_example.cc',
        'cpp_binding_example.h',
        'cpp_bound_class.cc',
        'cpp_bound_class.h',
        'cpp_variant.cc',
        'cpp_variant.h',
        'dom_operations.cc',
        'dom_operations.h',
        'form_data.cc',
        'form_data.h',
        'form_field.cc',
        'form_field.h',
        'ftp_directory_listing_response_delegate.cc',
        'ftp_directory_listing_response_delegate.h',
        'glue_serialize.cc',
        'glue_serialize.h',
        'idb_bindings.cc',
        'idb_bindings.h',
        'image_decoder.cc',
        'image_decoder.h',
        'image_resource_fetcher.cc',
        'image_resource_fetcher.h',
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'npruntime_util.cc',
        'npruntime_util.h',
        'password_form.cc',
        'password_form.h',
        'password_form_dom_manager.cc',
        'password_form_dom_manager.h',
        'resource_fetcher.cc',
        'resource_fetcher.h',
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'resource_type.h',
        'scoped_clipboard_writer_glue.h',
        'simple_webmimeregistry_impl.cc',
        'simple_webmimeregistry_impl.h',
        'site_isolation_metrics.cc',
        'site_isolation_metrics.h',
        'webaccessibility.cc',
        'webaccessibility.h',
        'webclipboard_impl.cc',
        'webclipboard_impl.h',
        'web_io_operators.cc',
        'web_io_operators.h',
        'webcookie.cc',
        'webcookie.h',
        'webcursor.cc',
        'webcursor.h',
        'webcursor_gtk.cc',
        'webcursor_gtk_data.h',
        'webcursor_mac.mm',
        'webcursor_win.cc',
        'webdropdata.cc',
        'webdropdata_win.cc',
        'webdropdata.h',
        'webfileutilities_impl.cc',
        'webfileutilities_impl.h',
        'webkit_glue.cc',
        'webkit_glue.h',
        'webkitclient_impl.cc',
        'webkitclient_impl.h',
        'webmediaplayer_impl.h',
        'webmediaplayer_impl.cc',
        'webmenuitem.h',
        'webmenurunner_mac.h',
        'webmenurunner_mac.mm',
        'webpasswordautocompletelistener_impl.cc',
        'webpasswordautocompletelistener_impl.h',
        'webpreferences.cc',
        'webpreferences.h',
        'websocketstreamhandle_bridge.h',
        'websocketstreamhandle_delegate.h',
        'websocketstreamhandle_impl.cc',
        'websocketstreamhandle_impl.h',
        'webthemeengine_impl_linux.cc',
        'webthemeengine_impl_win.cc',
        'weburlloader_impl.cc',
        'weburlloader_impl.h',
        'webvideoframe_impl.cc',
        'webvideoframe_impl.h',
        'window_open_disposition.h',
        'window_open_disposition.cc',

        # These files used to be built in the webcore target, but moved here
        # since part of glue.
        '../extensions/v8/benchmarking_extension.cc',
        '../extensions/v8/benchmarking_extension.h',
        '../extensions/v8/gc_extension.cc',
        '../extensions/v8/gc_extension.h',
        '../extensions/v8/gears_extension.cc',
        '../extensions/v8/gears_extension.h',
        '../extensions/v8/heap_profiler_extension.cc',
        '../extensions/v8/heap_profiler_extension.h',
        '../extensions/v8/playback_extension.cc',
        '../extensions/v8/playback_extension.h',
        '../extensions/v8/profiler_extension.cc',
        '../extensions/v8/profiler_extension.h',

      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'plugins/plugin_stubs.cc',
          ],
        }, { # else: OS!="linux" and OS!="freebsd" and OS!="openbsd" \
             # and OS!="solaris"'
          'sources/': [['exclude', '_(linux|gtk)(_data)?\\.cc$'],
                       ['exclude', r'/gtk_']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
        }, {  # else: OS=="mac"
          'sources/': [['exclude', 'plugin_(lib|list)_posix\\.cc$']],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['enable_gpu==1', {
          'sources': [
            'plugins/pepper_graphics_3d_gl.cc',
            'plugins/pepper_graphics_3d.cc',
            'plugins/pepper_graphics_3d.h',
          ],
          'dependencies': [
            '<(DEPTH)/gpu/gpu.gyp:gpu_plugin',
          ],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            'webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            '<(DEPTH)/build/win/system.gyp:cygwin',
          ],
          'sources!': [
            'plugins/plugin_stubs.cc',
          ],
          'conditions': [
            ['inside_chromium_build==1 and component=="shared_library"', {
              'dependencies': [
                '<(DEPTH)/third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
               ],
               'export_dependent_settings': [
                 '<(DEPTH)/third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
                 '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
               ],
            }],
          ],
        }],
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
