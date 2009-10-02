# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/WebKit/WebKit/chromium/features.gypi',
    '../third_party/WebKit/WebCore/WebCore.gypi',
  ],
  'variables': {
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '../chrome/tools/build/apply_locales.py',],

    # We can't turn on warnings on Windows and Linux until we upstream the
    # WebKit API.
    'conditions': [
      ['OS=="mac"', {
        'chromium_code': 1,
      }],
    ],
  },
  'targets': [
    {
      # Currently, builders assume webkit.sln builds test_shell on windows.
      # We should change this, but for now allows trybot runs.
      # for now.
      'target_name': 'pull_in_test_shell',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'tools/test_shell/test_shell.gyp:*',
          ],
        }],
      ],
    },
    {
      'target_name': 'webkit',
      'type': '<(library)',
      'msvs_guid': '5ECEC9E5-8F23-47B6-93E0-C3B328B3BE65',
      'dependencies': [
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
      ],
      'include_dirs': [
        'api/public',
        'api/src',
      ],
      'defines': [
        'WEBKIT_IMPLEMENTATION',
      ],
      'sources': [
        'api/public/gtk/WebInputEventFactory.h',
        'api/public/linux/WebFontRendering.h',
        'api/public/x11/WebScreenInfoFactory.h',
        'api/public/mac/WebInputEventFactory.h',
        'api/public/mac/WebScreenInfoFactory.h',
        'api/public/WebApplicationCacheHost.h',
        'api/public/WebApplicationCacheHostClient.h',
        'api/public/WebBindings.h',
        'api/public/WebCache.h',
        'api/public/WebCanvas.h',
        'api/public/WebClipboard.h',
        'api/public/WebColor.h',
        'api/public/WebColorName.h',
        'api/public/WebCommon.h',
        'api/public/WebCompositionCommand.h',
        'api/public/WebConsoleMessage.h',
        'api/public/WebCString.h',
        'api/public/WebCursorInfo.h',
        'api/public/WebData.h',
        'api/public/WebDataSource.h',
        'api/public/WebDragData.h',
        'api/public/WebEditingAction.h',
        'api/public/WebFindOptions.h',
        'api/public/WebFrame.h',
        'api/public/WebFrameClient.h',
        'api/public/WebForm.h',
        'api/public/WebHistoryItem.h',
        'api/public/WebHTTPBody.h',
        'api/public/WebImage.h',
        'api/public/WebInputEvent.h',
        'api/public/WebKit.h',
        'api/public/WebKitClient.h',
        'api/public/WebLocalizedString.h',
        'api/public/WebMediaPlayer.h',
        'api/public/WebMediaPlayerClient.h',
        'api/public/WebMessagePortChannel.h',
        'api/public/WebMessagePortChannelClient.h',
        'api/public/WebMimeRegistry.h',
        'api/public/WebNavigationType.h',
        'api/public/WebNode.h',
        'api/public/WebNonCopyable.h',
        'api/public/WebNotification.h',
        'api/public/WebNotificationPresenter.h',
        'api/public/WebNotificationPermissionCallback.h',
        'api/public/WebPlugin.h',
        'api/public/WebPluginContainer.h',
        'api/public/WebPluginListBuilder.h',
        'api/public/WebPoint.h',
        'api/public/WebPopupMenu.h',
        'api/public/WebPopupMenuInfo.h',
        'api/public/WebRange.h',
        'api/public/WebRect.h',
        'api/public/WebScreenInfo.h',
        'api/public/WebScriptSource.h',
        'api/public/WebSecurityOrigin.h',
        'api/public/WebSettings.h',
        'api/public/WebSize.h',
        'api/public/WebStorageArea.h',
        'api/public/WebStorageNamespace.h',
        'api/public/WebString.h',
        'api/public/WebTextAffinity.h',
        'api/public/WebTextDirection.h',
        'api/public/WebURL.h',
        'api/public/WebURLError.h',
        'api/public/WebURLLoader.h',
        'api/public/WebURLLoaderClient.h',
        'api/public/WebURLRequest.h',
        'api/public/WebURLResponse.h',
        'api/public/WebVector.h',
        'api/public/WebView.h',
        'api/public/WebViewClient.h',
        'api/public/WebWidget.h',
        'api/public/WebWidgetClient.h',
        'api/public/WebWorker.h',
        'api/public/WebWorkerClient.h',
        'api/public/win/WebInputEventFactory.h',
        'api/public/win/WebSandboxSupport.h',
        'api/public/win/WebScreenInfoFactory.h',
        'api/public/win/WebScreenInfoFactory.h',
        'api/src/ApplicationCacheHost.cpp',
        'api/src/AssertMatchingEnums.cpp',
        'api/src/ChromiumBridge.cpp',
        'api/src/ChromiumCurrentTime.cpp',
        'api/src/ChromiumThreading.cpp',
        'api/src/gtk/WebFontInfo.cpp',
        'api/src/gtk/WebFontInfo.h',
        'api/src/gtk/WebInputEventFactory.cpp',
        'api/src/linux/WebFontRendering.cpp',
        'api/src/x11/WebScreenInfoFactory.cpp',
        'api/src/mac/WebInputEventFactory.mm',
        'api/src/mac/WebScreenInfoFactory.mm',
        'api/src/LocalizedStrings.cpp',
        'api/src/MediaPlayerPrivateChromium.cpp',
        'api/src/NotificationPresenterImpl.h',
        'api/src/NotificationPresenterImpl.cpp',
        'api/src/PlatformMessagePortChannel.cpp',
        'api/src/PlatformMessagePortChannel.h',
        'api/src/ResourceHandle.cpp',
        'api/src/SocketStreamHandle.cpp',
        'api/src/StorageAreaProxy.cpp',
        'api/src/StorageAreaProxy.h',
        'api/src/StorageEventDispatcherChromium.cpp',
        'api/src/StorageNamespaceProxy.cpp',
        'api/src/StorageNamespaceProxy.h',
        'api/src/TemporaryGlue.h',
        'api/src/WebBindings.cpp',
        'api/src/WebCache.cpp',
        'api/src/WebColor.cpp',
        'api/src/WebCString.cpp',
        'api/src/WebCursorInfo.cpp',
        'api/src/WebData.cpp',
        'api/src/WebDataSourceImpl.cpp',
        'api/src/WebDataSourceImpl.h',
        'api/src/WebDragData.cpp',
        'api/src/WebForm.cpp',
        'api/src/WebHistoryItem.cpp',
        'api/src/WebHTTPBody.cpp',
        'api/src/WebImageCG.cpp',
        'api/src/WebImageSkia.cpp',
        'api/src/WebInputEvent.cpp',
        'api/src/WebInputEventConversion.cpp',
        'api/src/WebInputEventConversion.h',
        'api/src/WebKit.cpp',
        'api/src/WebMediaPlayerClientImpl.cpp',
        'api/src/WebMediaPlayerClientImpl.h',
        'api/src/WebNode.cpp',
        'api/src/WebNotification.cpp',
        'api/src/WebPluginContainerImpl.h',
        'api/src/WebPluginContainerImpl.cpp',
        'api/src/WebPluginListBuilderImpl.cpp',
        'api/src/WebPluginListBuilderImpl.h',
        'api/src/WebPluginLoadObserver.cpp',
        'api/src/WebPluginLoadObserver.h',
        'api/src/WebRange.cpp',
        'api/src/WebSecurityOrigin.cpp',
        'api/src/WebSettingsImpl.cpp',
        'api/src/WebSettingsImpl.h',
        'api/src/WebStorageAreaImpl.cpp',
        'api/src/WebStorageAreaImpl.h',
        'api/src/WebStorageNamespaceImpl.cpp',
        'api/src/WebStorageNamespaceImpl.h',
        'api/src/WebString.cpp',
        'api/src/WebURL.cpp',
        'api/src/WebURLRequest.cpp',
        'api/src/WebURLRequestPrivate.h',
        'api/src/WebURLResponse.cpp',
        'api/src/WebURLResponsePrivate.h',
        'api/src/WebURLError.cpp',
        'api/src/WrappedResourceRequest.h',
        'api/src/WrappedResourceResponse.h',
        'api/src/win/WebInputEventFactory.cpp',
        'api/src/win/WebScreenInfoFactory.cpp',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
          ],
          'include_dirs': [
            'api/public/x11',
            'api/public/gtk',
            'api/public/linux',
          ],
        }, { # else: OS!="linux" and OS!="freebsd"
          'sources/': [
            ['exclude', '/gtk/'],
            ['exclude', '/x11/'],
            ['exclude', '/linux/'],
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            'api/public/mac',
          ],
          'sources/': [
            ['exclude', 'Skia\\.cpp$'],
          ],
        }, { # else: OS!="mac"
          'sources/': [
            ['exclude', '/mac/'],
            ['exclude', 'CG\\.cpp$'],
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'api/public/win',
          ],
        }, { # else: OS!="win"
          'sources/': [['exclude', '/win/']],
        }],
        ['"ENABLE_3D_CANVAS=1" in feature_defines', {
          # Conditionally compile in GLEW and our GraphicsContext3D implementation.
          'sources+': [
            'api/src/GraphicsContext3D.cpp',
            '../third_party/glew/src/glew.c'
          ],
          'include_dirs+': [
            '../third_party/glew/include'
          ],
          'defines+': [
            'GLEW_STATIC=1',
          ],
          'conditions': [
            ['OS=="win"', {
              'link_settings': {
                'libraries': [
                  '-lopengl32.lib',
                ]
              },
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'msvs_guid': '0B469837-3D46-484A-AFB3-C5A6C68730B9',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'input_path': 'glue/webkit_resources.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_resources.h',
            '<(grit_out_dir)/webkit_resources.pak',
            '<(grit_out_dir)/webkit_resources.rc',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
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
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'msvs_guid': '60B43839-95E6-4526-A661-209F16335E0E',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'input_path': 'glue/webkit_strings.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_strings.h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/webkit_strings_ZZLOCALE.pak\' <(locales))',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
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
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'appcache',
      'type': '<(library)',
      'msvs_guid': '0B945915-31A7-4A07-A5B5-568D737A39B1',
      'dependencies': [
        '../net/net.gyp:net',
        'webkit',
      ],
      'sources': [
        # This list contains all .h and .cc in appcache except for test code.
        'appcache/appcache.cc',
        'appcache/appcache.h',
        'appcache/appcache_backend_impl.cc',
        'appcache/appcache_backend_impl.h',
        'appcache/appcache_entry.h',
        'appcache/appcache_frontend_impl.cc',
        'appcache/appcache_frontend_impl.h',
        'appcache/appcache_group.cc',
        'appcache/appcache_group.h',
        'appcache/appcache_host.cc',
        'appcache/appcache_host.h',
        'appcache/appcache_interceptor.cc',
        'appcache/appcache_interceptor.h',
        'appcache/appcache_interfaces.cc',
        'appcache/appcache_interfaces.h',
        'appcache/appcache_request_handler.cc',
        'appcache/appcache_request_handler.h',
        'appcache/appcache_service.cc',
        'appcache/appcache_service.h',
        'appcache/manifest_parser.cc',
        'appcache/manifest_parser.h',
        'appcache/web_application_cache_host_impl.cc',
        'appcache/web_application_cache_host_impl.h',
      ],
    },
    {
      'target_name': 'database',
      'type': '<(library)',
      'msvs_guid': '1DA00DDD-44E5-4C56-B2CC-414FB0164492',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'sources': [
        'database/vfs_backend.cc',
        'database/vfs_backend.h',
      ],
    },
    {
      'target_name': 'glue',
      'type': '<(library)',
      'msvs_guid': 'C66B126D-0ECE-4CA2-B6DC-FA780AFBBF09',
      'dependencies': [
        '../net/net.gyp:net',
        'inspector_resources',
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        'webkit',
        'webkit_resources',
        'webkit_strings',
      ],
      'actions': [
        {
          'action_name': 'webkit_version',
          'inputs': [
            'build/webkit_version.py',
            '../third_party/WebKit/WebCore/Configurations/Version.xcconfig',
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
        'glue/devtools/devtools_rpc.h',
        'glue/devtools/devtools_rpc_js.h',
        'glue/devtools/bound_object.cc',
        'glue/devtools/bound_object.h',
        'glue/devtools/debugger_agent.h',
        'glue/devtools/debugger_agent_impl.cc',
        'glue/devtools/debugger_agent_impl.h',
        'glue/devtools/debugger_agent_manager.cc',
        'glue/devtools/debugger_agent_manager.h',
        'glue/devtools/tools_agent.h',
        'glue/media/buffered_data_source.cc',
        'glue/media/buffered_data_source.h',
        'glue/media/media_resource_loader_bridge_factory.cc',
        'glue/media/media_resource_loader_bridge_factory.h',
        'glue/media/simple_data_source.cc',
        'glue/media/simple_data_source.h',
        'glue/media/video_renderer_impl.cc',
        'glue/media/video_renderer_impl.h',
        'glue/plugins/mozilla_extensions.cc',
        'glue/plugins/mozilla_extensions.h',
        'glue/plugins/nphostapi.h',
        'glue/plugins/fake_plugin_window_tracker_mac.h',
        'glue/plugins/fake_plugin_window_tracker_mac.cc',
        'glue/plugins/gtk_plugin_container.h',
        'glue/plugins/gtk_plugin_container.cc',
        'glue/plugins/gtk_plugin_container_manager.h',
        'glue/plugins/gtk_plugin_container_manager.cc',
        'glue/plugins/plugin_constants_win.h',
        'glue/plugins/plugin_host.cc',
        'glue/plugins/plugin_host.h',
        'glue/plugins/plugin_instance.cc',
        'glue/plugins/plugin_instance.h',
        'glue/plugins/plugin_lib.cc',
        'glue/plugins/plugin_lib.h',
        'glue/plugins/plugin_lib_linux.cc',
        'glue/plugins/plugin_lib_mac.mm',
        'glue/plugins/plugin_lib_win.cc',
        'glue/plugins/plugin_list.cc',
        'glue/plugins/plugin_list.h',
        'glue/plugins/plugin_list_linux.cc',
        'glue/plugins/plugin_list_mac.mm',
        'glue/plugins/plugin_list_win.cc',
        'glue/plugins/plugin_stream.cc',
        'glue/plugins/plugin_stream.h',
        'glue/plugins/plugin_stream_posix.cc',
        'glue/plugins/plugin_stream_url.cc',
        'glue/plugins/plugin_stream_url.h',
        'glue/plugins/plugin_stream_win.cc',
        'glue/plugins/plugin_string_stream.cc',
        'glue/plugins/plugin_string_stream.h',
        'glue/plugins/plugin_stubs.cc',
        'glue/plugins/webplugin_delegate_impl.cc',
        'glue/plugins/webplugin_delegate_impl.h',
        'glue/plugins/webplugin_delegate_impl_gtk.cc',
        'glue/plugins/webplugin_delegate_impl_mac.mm',
        'glue/plugins/webplugin_delegate_impl_win.cc',
        'glue/alt_error_page_resource_fetcher.cc',
        'glue/alt_error_page_resource_fetcher.h',
        'glue/autofill_form.cc',
        'glue/autofill_form.h',
        'glue/back_forward_list_client_impl.cc',
        'glue/back_forward_list_client_impl.h',
        'glue/chrome_client_impl.cc',
        'glue/chrome_client_impl.h',
        'glue/context_menu.h',
        'glue/context_menu_client_impl.cc',
        'glue/context_menu_client_impl.h',
        'glue/cpp_binding_example.cc',
        'glue/cpp_binding_example.h',
        'glue/cpp_bound_class.cc',
        'glue/cpp_bound_class.h',
        'glue/cpp_variant.cc',
        'glue/cpp_variant.h',
        'glue/dom_operations.cc',
        'glue/dom_operations.h',
        'glue/dom_operations_private.h',
        'glue/dom_serializer.cc',
        'glue/dom_serializer.h',
        'glue/dom_serializer_delegate.h',
        'glue/dragclient_impl.cc',
        'glue/dragclient_impl.h',
        'glue/editor_client_impl.cc',
        'glue/editor_client_impl.h',
        'glue/entity_map.cc',
        'glue/entity_map.h',
        'glue/feed_preview.cc',
        'glue/feed_preview.h',
        'glue/form_data.h',
        'glue/ftp_directory_listing_response_delegate.cc',
        'glue/ftp_directory_listing_response_delegate.h',
        'glue/glue_accessibility_object.cc',
        'glue/glue_accessibility_object.h',
        'glue/glue_serialize.cc',
        'glue/glue_serialize.h',
        'glue/glue_util.cc',
        'glue/glue_util.h',
        'glue/image_decoder.cc',
        'glue/image_decoder.h',
        'glue/image_resource_fetcher.cc',
        'glue/image_resource_fetcher.h',
        'glue/inspector_client_impl.cc',
        'glue/inspector_client_impl.h',
        'glue/multipart_response_delegate.cc',
        'glue/multipart_response_delegate.h',
        'glue/npruntime_util.cc',
        'glue/npruntime_util.h',
        'glue/password_autocomplete_listener.cc',
        'glue/password_autocomplete_listener.h',
        'glue/password_form.h',
        'glue/password_form_dom_manager.cc',
        'glue/password_form_dom_manager.h',
        'glue/resource_fetcher.cc',
        'glue/resource_fetcher.h',
        'glue/resource_loader_bridge.cc',
        'glue/resource_loader_bridge.h',
        'glue/resource_type.h',
        'glue/scoped_clipboard_writer_glue.h',
        'glue/searchable_form_data.cc',
        'glue/searchable_form_data.h',
        'glue/simple_webmimeregistry_impl.cc',
        'glue/simple_webmimeregistry_impl.h',
        'glue/webaccessibility.h',
        'glue/webaccessibilitymanager.h',
        'glue/webaccessibilitymanager_impl.cc',
        'glue/webaccessibilitymanager_impl.h',
        'glue/webclipboard_impl.cc',
        'glue/webclipboard_impl.h',
        'glue/webcursor.cc',
        'glue/webcursor.h',
        'glue/webcursor_gtk.cc',
        'glue/webcursor_gtk_data.h',
        'glue/webcursor_mac.mm',
        'glue/webcursor_win.cc',
        'glue/webdevtoolsagent.h',
        'glue/webdevtoolsagent_delegate.h',
        'glue/webdevtoolsagent_impl.cc',
        'glue/webdevtoolsagent_impl.h',
        'glue/webdevtoolsclient.h',
        'glue/webdevtoolsclient_delegate.h',
        'glue/webdevtoolsclient_impl.cc',
        'glue/webdevtoolsclient_impl.h',
        'glue/webdropdata.cc',
        'glue/webdropdata_win.cc',
        'glue/webdropdata.h',
        'glue/webframe_impl.cc',
        'glue/webframe_impl.h',
        'glue/webframeloaderclient_impl.cc',
        'glue/webframeloaderclient_impl.h',
        'glue/webkit_glue.cc',
        'glue/webkit_glue.h',
        'glue/webkitclient_impl.cc',
        'glue/webkitclient_impl.h',
        'glue/webmediaplayer_impl.h',
        'glue/webmediaplayer_impl.cc',
        'glue/webmenuitem.h',
        'glue/webmenurunner_mac.h',
        'glue/webmenurunner_mac.mm',
        'glue/webplugin.h',
        'glue/webplugin_delegate.h',
        'glue/webplugin_impl.cc',
        'glue/webplugin_impl.h',
        'glue/webplugininfo.h',
        'glue/webpopupmenu_impl.cc',
        'glue/webpopupmenu_impl.h',
        'glue/webpreferences.cc',
        'glue/webpreferences.h',
        'glue/webthemeengine_impl_win.cc',
        'glue/weburlloader_impl.cc',
        'glue/weburlloader_impl.h',
        'glue/webview.h',
        'glue/webview_delegate.h',
        'glue/webview_impl.cc',
        'glue/webview_impl.h',
        'glue/webworker_impl.cc',
        'glue/webworker_impl.h',
        'glue/webworkerclient_impl.cc',
        'glue/webworkerclient_impl.h',
        'glue/window_open_disposition.h',
        'glue/window_open_disposition.cc',

        # These files used to be built in the webcore target, but moved here
        # since part of glue.
        'extensions/v8/gc_extension.cc',
        'extensions/v8/gc_extension.h',
        'extensions/v8/gears_extension.cc',
        'extensions/v8/gears_extension.h',
        'extensions/v8/interval_extension.cc',
        'extensions/v8/interval_extension.h',
        'extensions/v8/playback_extension.cc',
        'extensions/v8/playback_extension.h',
        'extensions/v8/profiler_extension.cc',
        'extensions/v8/profiler_extension.h',
        'extensions/v8/benchmarking_extension.cc',
        'extensions/v8/benchmarking_extension.h',

      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'export_dependent_settings': [
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../base/base.gyp:linux_versioninfo',
          ],
          'export_dependent_settings': [
            # Users of webcursor.h need the GTK include path.
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }, { # else: OS!="linux" and OS!="freebsd"
          'sources/': [['exclude', '_(linux|gtk)(_data)?\\.cc$'],
                       ['exclude', r'/gtk_']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']]
        }, { # else: OS=="mac"
          'sources!': [
            # TODO(port): Unfork webplugin_delegate_impl_mac and this file.
            'glue/plugins/webplugin_delegate_impl.cc',
          ],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            # These files are Windows-only now but may be ported to other
            # platforms.
            'glue/glue_accessibility_object.cc',
            'glue/glue_accessibility_object.h',
            'glue/plugins/mozilla_extensions.cc',
            'glue/webaccessibility.h',
            'glue/webaccessibilitymanager.h',
            'glue/webaccessibilitymanager_impl.cc',
            'glue/webaccessibilitymanager_impl.cc',
            'glue/webaccessibilitymanager_impl.h',
            'glue/webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '../chrome/third_party/wtl/include',
          ],
          'dependencies': [
            '../build/win/system.gyp:cygwin',
            'default_plugin/default_plugin.gyp:default_plugin',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'inspector_resources',
      'type': 'none',
      'msvs_guid': '5330F8EE-00F5-D65C-166E-E3150171055D',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector',
          'files': [
            'glue/devtools/js/base.js',
            'glue/devtools/js/debugger_agent.js',
            'glue/devtools/js/devtools.css',
            'glue/devtools/js/devtools.html',
            'glue/devtools/js/devtools.js',
            'glue/devtools/js/devtools_callback.js',
            'glue/devtools/js/devtools_host_stub.js',
            'glue/devtools/js/heap_profiler_panel.js',
            'glue/devtools/js/inspector_controller.js',
            'glue/devtools/js/inspector_controller_impl.js',
            'glue/devtools/js/profiler_processor.js',
            'glue/devtools/js/tests.js',

            '<@(webinspector_files)',

            '../v8/tools/codemap.js',
            '../v8/tools/consarray.js',
            '../v8/tools/csvparser.js',
            '../v8/tools/logreader.js',
            '../v8/tools/profile.js',
            '../v8/tools/profile_view.js',
            '../v8/tools/splaytree.js',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
          'files': [

            '<@(webinspector_image_files)',

          ],
        },
      ],
    },
  ], # targets
}
