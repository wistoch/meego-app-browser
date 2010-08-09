# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },  # variables
  'targets': [
    # A library for sending and receiving peer-issued notifications.
    #
    # TODO(akalin): Separate out the XMPP stuff from this library into
    # its own library.
    {
      'target_name': 'notifier',
      'type': '<(library)',
      'sources': [
        'notifier/base/chrome_async_socket.cc',
        'notifier/base/chrome_async_socket.h',
        'notifier/base/signal_thread_task.h',
        'notifier/base/ssl_adapter.h',
        'notifier/base/ssl_adapter.cc',
        'notifier/base/static_assert.h',
        'notifier/base/task_pump.cc',
        'notifier/base/task_pump.h',
        'notifier/communicator/connection_options.cc',
        'notifier/communicator/connection_options.h',
        'notifier/communicator/connection_settings.cc',
        'notifier/communicator/connection_settings.h',
        'notifier/communicator/const_communicator.h',
        'notifier/communicator/gaia_token_pre_xmpp_auth.cc',
        'notifier/communicator/gaia_token_pre_xmpp_auth.h',
        'notifier/communicator/login.cc',
        'notifier/communicator/login.h',
        'notifier/communicator/login_connection_state.h',
        'notifier/communicator/login_failure.cc',
        'notifier/communicator/login_failure.h',
        'notifier/communicator/login_settings.cc',
        'notifier/communicator/login_settings.h',
        'notifier/communicator/product_info.cc',
        'notifier/communicator/product_info.h',
        'notifier/communicator/single_login_attempt.cc',
        'notifier/communicator/single_login_attempt.h',
        'notifier/communicator/ssl_socket_adapter.cc',
        'notifier/communicator/ssl_socket_adapter.h',
        'notifier/communicator/xmpp_connection_generator.cc',
        'notifier/communicator/xmpp_connection_generator.h',
        'notifier/communicator/xmpp_socket_adapter.cc',
        'notifier/communicator/xmpp_socket_adapter.h',
        'notifier/listener/listen_task.cc',
        'notifier/listener/listen_task.h',
        'notifier/listener/mediator_thread.h',
        'notifier/listener/mediator_thread_impl.cc',
        'notifier/listener/mediator_thread_impl.h',
        'notifier/listener/mediator_thread_mock.h',
        'notifier/listener/notification_constants.cc',
        'notifier/listener/notification_constants.h',
        'notifier/listener/notification_defines.h',
        'notifier/listener/send_update_task.cc',
        'notifier/listener/send_update_task.h',
        'notifier/base/sigslotrepeater.h',
        'notifier/listener/subscribe_task.cc',
        'notifier/listener/subscribe_task.h',
        'notifier/listener/talk_mediator.h',
        'notifier/listener/talk_mediator_impl.cc',
        'notifier/listener/talk_mediator_impl.h',
        'notifier/listener/xml_element_util.cc',
        'notifier/listener/xml_element_util.h',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
        'kXmppProductName="chromium-sync"',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/expat/expat.gyp:expat',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
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
        'notifier/base/chrome_async_socket_unittest.cc',
        'notifier/listener/talk_mediator_unittest.cc',
        'notifier/listener/send_update_task_unittest.cc',
        'notifier/listener/subscribe_task_unittest.cc',
        'notifier/listener/xml_element_util_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'notifier',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
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
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
