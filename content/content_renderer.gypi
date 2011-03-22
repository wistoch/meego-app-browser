# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_renderer',
      'msvs_guid': '9AAA8CF2-9B3D-4895-8CB9-D70BBD125EAD',
      'type': '<(library)',
      'dependencies': [
        'content_common',
        '../ppapi/ppapi.gyp:ppapi_proxy',
        '../skia/skia.gyp:skia',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'renderer/active_notification_tracker.cc',
        'renderer/active_notification_tracker.h',
        'renderer/audio_device.cc',
        'renderer/audio_device.h',
        'renderer/audio_message_filter.cc',
        'renderer/audio_message_filter.h',
        'renderer/content_renderer_client.cc',
        'renderer/content_renderer_client.h',
        'renderer/cookie_message_filter.cc',
        'renderer/cookie_message_filter.h',
        'renderer/device_orientation_dispatcher.cc',
        'renderer/device_orientation_dispatcher.h',
        'renderer/external_popup_menu.cc',
        'renderer/external_popup_menu.h',
        'renderer/geolocation_dispatcher.cc',
        'renderer/geolocation_dispatcher.h',
        'renderer/ggl.cc',
        'renderer/ggl.h',
        'renderer/gpu_channel_host.cc',
        'renderer/gpu_channel_host.h',
        'renderer/gpu_video_decoder_host.cc',
        'renderer/gpu_video_decoder_host.h',
        'renderer/gpu_video_service_host.cc',
        'renderer/gpu_video_service_host.h',
        'renderer/indexed_db_dispatcher.cc',
        'renderer/indexed_db_dispatcher.h',
        'renderer/load_progress_tracker.cc',
        'renderer/load_progress_tracker.h',
        'renderer/media/audio_renderer_impl.cc',
        'renderer/media/audio_renderer_impl.h',
        'renderer/media/gles2_video_decode_context.cc',
        'renderer/media/gles2_video_decode_context.h',
        'renderer/media/ipc_video_decoder.cc',
        'renderer/media/ipc_video_decoder.h',
        'renderer/navigation_state.cc',
        'renderer/navigation_state.h',
        'renderer/notification_provider.cc',
        'renderer/notification_provider.h',
        'renderer/p2p/ipc_network_manager.cc',
        'renderer/p2p/ipc_network_manager.h',
        'renderer/p2p/ipc_socket_factory.cc',
        'renderer/p2p/ipc_socket_factory.h',
        'renderer/p2p/socket_client.cc',
        'renderer/p2p/socket_client.h',
        'renderer/p2p/socket_dispatcher.cc',
        'renderer/p2p/socket_dispatcher.h',
        'renderer/paint_aggregator.cc',
        'renderer/paint_aggregator.h',
        'renderer/pepper_platform_context_3d_impl.cc',
        'renderer/pepper_platform_context_3d_impl.h',
        'renderer/pepper_plugin_delegate_impl.cc',
        'renderer/pepper_plugin_delegate_impl.h',
        'renderer/plugin_channel_host.cc',
        'renderer/plugin_channel_host.h',
        'renderer/render_view.cc',
        'renderer/render_view.h',
        'renderer/render_view_linux.cc',
        'renderer/render_view_observer.cc',
        'renderer/render_view_observer.h',
        'renderer/render_view_visitor.h',
        'renderer/render_widget.cc',
        'renderer/render_widget.h',
        'renderer/render_widget_fullscreen.cc',
        'renderer/render_widget_fullscreen.h',
        'renderer/render_widget_fullscreen_pepper.cc',
        'renderer/render_widget_fullscreen_pepper.h',
        'renderer/renderer_sandbox_support_linux.cc',
        'renderer/renderer_sandbox_support_linux.h',
        'renderer/renderer_webapplicationcachehost_impl.cc',
        'renderer/renderer_webapplicationcachehost_impl.h',
        'renderer/renderer_webaudiodevice_impl.cc',
        'renderer/renderer_webaudiodevice_impl.h',
        'renderer/renderer_webcookiejar_impl.cc',
        'renderer/renderer_webcookiejar_impl.h',
        'renderer/renderer_webidbcursor_impl.cc',
        'renderer/renderer_webidbcursor_impl.h',
        'renderer/renderer_webidbdatabase_impl.cc',
        'renderer/renderer_webidbdatabase_impl.h',
        'renderer/renderer_webidbfactory_impl.cc',
        'renderer/renderer_webidbfactory_impl.h',
        'renderer/renderer_webidbindex_impl.cc',
        'renderer/renderer_webidbindex_impl.h',
        'renderer/renderer_webidbobjectstore_impl.cc',
        'renderer/renderer_webidbobjectstore_impl.h',
        'renderer/renderer_webidbtransaction_impl.cc',
        'renderer/renderer_webidbtransaction_impl.h',
        'renderer/renderer_webkitclient_impl.cc',
        'renderer/renderer_webkitclient_impl.h',
        'renderer/renderer_webstoragearea_impl.cc',
        'renderer/renderer_webstoragearea_impl.h',
        'renderer/renderer_webstoragenamespace_impl.cc',
        'renderer/renderer_webstoragenamespace_impl.h',
        'renderer/speech_input_dispatcher.cc',
        'renderer/speech_input_dispatcher.h',
        'renderer/webgraphicscontext3d_command_buffer_impl.cc',
        'renderer/webgraphicscontext3d_command_buffer_impl.h',
        'renderer/webplugin_delegate_proxy.cc',
        'renderer/webplugin_delegate_proxy.h',
        'renderer/websharedworker_proxy.cc',
        'renderer/websharedworker_proxy.h',
        'renderer/websharedworkerrepository_impl.cc',
        'renderer/websharedworkerrepository_impl.h',
        'renderer/webworker_base.cc',
        'renderer/webworker_base.h',
        'renderer/webworker_proxy.cc',
        'renderer/webworker_proxy.h',
        'renderer/web_ui_bindings.cc',
        'renderer/web_ui_bindings.h',
      ],
      'conditions': [
        ['enable_gpu==1', {
          'dependencies': [
            '../gpu/gpu.gyp:gles2_c_lib',
          ],
          'sources': [
            'renderer/command_buffer_proxy.cc',
            'renderer/command_buffer_proxy.h',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="mac"', {
          'sources!': [
            'common/process_watcher_posix.cc',
          ],
          'link_settings': {
            'mac_bundle_resources': [
              'renderer/renderer.sb',
            ],
          },
        }],
      ],
    },
  ],
}
