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
            'input_path': 'webkit_resources.grd',
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
            'input_path': 'webkit_strings.grd',
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
      'target_name': 'glue',
      'type': '<(library)',
      'msvs_guid': 'C66B126D-0ECE-4CA2-B6DC-FA780AFBBF09',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/net/net.gyp:net',
        '<(webkit_src_dir)/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        'webkit_resources',
        'webkit_strings',
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
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        'devtools_message_data.cc',
        'devtools_message_data.h',
        'media/buffered_data_source.cc',
        'media/buffered_data_source.h',
        'media/media_resource_loader_bridge_factory.cc',
        'media/media_resource_loader_bridge_factory.h',
        'media/simple_data_source.cc',
        'media/simple_data_source.h',
        'media/video_renderer_impl.cc',
        'media/video_renderer_impl.h',
        'media/web_video_renderer.h',
        'plugins/carbon_plugin_window_tracker_mac.h',
        'plugins/carbon_plugin_window_tracker_mac.cc',
        'plugins/coregraphics_private_symbols_mac.h',
        'plugins/nphostapi.h',
        'plugins/gtk_plugin_container.h',
        'plugins/gtk_plugin_container.cc',
        'plugins/gtk_plugin_container_manager.h',
        'plugins/gtk_plugin_container_manager.cc',
        'plugins/npapi_extension_thunk.cc',
        'plugins/npapi_extension_thunk.h',
        'plugins/plugin_constants_win.h',
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
        'plugins/webplugin_2d_device_delegate.h',
        'plugins/webplugin_3d_device_delegate.h',
        'plugins/webplugin_delegate_impl.cc',
        'plugins/webplugin_delegate_impl.h',
        'plugins/webplugin_delegate_impl_gtk.cc',
        'plugins/webplugin_delegate_impl_mac.mm',
        'plugins/webplugin_delegate_impl_win.cc',
        'alt_error_page_resource_fetcher.cc',
        'alt_error_page_resource_fetcher.h',
        'context_menu.h',
        'cpp_binding_example.cc',
        'cpp_binding_example.h',
        'cpp_bound_class.cc',
        'cpp_bound_class.h',
        'cpp_variant.cc',
        'cpp_variant.h',
        'dom_operations.cc',
        'dom_operations.h',
        'form_data.h',
        'form_field.cc',
        'form_field.h',
        'form_field_values.cc',
        'form_field_values.h',
        'ftp_directory_listing_response_delegate.cc',
        'ftp_directory_listing_response_delegate.h',
        'glue_serialize.cc',
        'glue_serialize.h',
        'image_decoder.cc',
        'image_decoder.h',
        'image_resource_fetcher.cc',
        'image_resource_fetcher.h',
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'npruntime_util.cc',
        'npruntime_util.h',
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
        'webaccessibility.cc',
        'webaccessibility.h',
        'webclipboard_impl.cc',
        'webclipboard_impl.h',
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
        'webplugin.cc',
        'webplugin.h',
        'webplugin_delegate.h',
        'webplugin_impl.cc',
        'webplugin_impl.h',
        'webplugininfo.h',
        'webpreferences.cc',
        'webpreferences.h',
        'websocketstreamhandle_bridge.h',
        'websocketstreamhandle_delegate.h',
        'websocketstreamhandle_impl.cc',
        'websocketstreamhandle_impl.h',
        'webthemeengine_impl_win.cc',
        'weburlloader_impl.cc',
        'weburlloader_impl.h',
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
        '../extensions/v8/interval_extension.cc',
        '../extensions/v8/interval_extension.h',
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
            '<(DEPTH)/base/base.gyp:linux_versioninfo',
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
              '$(SDKROOT)/QuartzCore.framework',
            ],
          },
        }],
        ['enable_gpu==1 and inside_chromium_build==1', {
          'dependencies': [
            '<(DEPTH)/gpu/gpu.gyp:gpu_plugin',
          ],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            # These files are Windows-only now but may be ported to other
            # platforms.
            'webaccessibility.cc',
            'webaccessibility.h',
            'webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            '<(DEPTH)/build/win/system.gyp:cygwin',
            '<(DEPTH)/webkit/default_plugin/default_plugin.gyp:default_plugin',
          ],
          'sources!': [
            'plugins/plugin_stubs.cc',
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
