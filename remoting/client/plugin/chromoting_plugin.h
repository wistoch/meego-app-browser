// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): We need to come up with a better description of the
// responsibilities for each thread.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_

#include <string>

#include "base/at_exit.h"
#include "base/scoped_ptr.h"
#include "remoting/client/host_connection.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/cpp/instance.h"
#include "third_party/ppapi/cpp/device_context_2d.h"

class MessageLoop;

namespace base {
class Thread;
}  // namespace base

namespace pp {
class Module;
}  // namespace pp

namespace remoting {

class ChromotingClient;
class HostConnection;
class JingleThread;
class PepperView;

class ChromotingPlugin : public pp::Instance {
 public:
  // The mimetype for which this plugin is registered.
  static const char *kMimeType;

  ChromotingPlugin(PP_Instance instance);
  virtual ~ChromotingPlugin();

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual bool HandleEvent(const PP_Event& event);
  virtual void ViewChanged(const PP_Rect& position, const PP_Rect& clip);

  virtual bool CurrentlyOnPluginThread() const;

 private:
  FRIEND_TEST(ChromotingPluginTest, ParseUrl);
  FRIEND_TEST(ChromotingPluginTest, TestCaseSetup);

  static bool ParseUrl(const std::string& url,
                       std::string* user_id,
                       std::string* auth_token,
                       std::string* host_jid);

  // Since we're an internal plugin, we can just grab the message loop during
  // init to figure out which thread we're on.  This should only be used to
  // sanity check which thread we're executing on. Do not post task here!
  // Instead, use PPB_Core:CallOnMainThread() in the pepper api.
  //
  // TODO(ajwong): Think if there is a better way to safeguard this.
  MessageLoop* pepper_main_loop_dont_post_to_me_;

  scoped_ptr<base::Thread> main_thread_;
  scoped_ptr<JingleThread> network_thread_;

  scoped_ptr<HostConnection> host_connection_;
  scoped_ptr<PepperView> view_;
  scoped_ptr<ChromotingClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingPlugin);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
