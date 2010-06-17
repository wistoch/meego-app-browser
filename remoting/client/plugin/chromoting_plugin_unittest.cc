// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "remoting/client/plugin/chromoting_plugin.h"
#include "remoting/client/pepper/fake_browser.h"
#include "testing/gtest/include/gtest/gtest.h"

// Routine to create the PepperPlugin subclass that implements all of the
// plugin-specific functionality.
pepper::PepperPlugin* CreatePlugin(NPNetscapeFuncs* browser_funcs,
                                   NPP instance);

namespace remoting {

class ChromotingPluginTest : public testing::Test {
 protected:

  virtual void SetUp() {
    // Set up the fake browser callback routines.
    fake_browser_ = Singleton<FakeBrowser>::get();
    NPNetscapeFuncs* browser_funcs_ = fake_browser_->GetBrowserFuncs();
    instance_.reset(new NPP_t());

    // Create the ChromotingPlugin for testing.
    pepper::PepperPlugin* pepper_plugin =
        CreatePlugin(browser_funcs_, instance_.get());
    plugin_.reset(
        static_cast<ChromotingPlugin*>(pepper_plugin));
  }

  virtual void TearDown() {
  }

  FakeBrowser* fake_browser_;
  scoped_ptr<NPP_t> instance_;
  scoped_ptr<ChromotingPlugin> plugin_;
};

TEST_F(ChromotingPluginTest, TestCaseSetup) {
  ASSERT_TRUE(plugin_->browser() != NULL);
  ASSERT_TRUE(plugin_->extensions() != NULL);
  ASSERT_TRUE(plugin_->instance() != NULL);

  // Device is not set until New() is called.
  ASSERT_TRUE(plugin_->device() == NULL);
}

#if 0
TODO(ajwong): reenable once we have the threading sorted out.
TEST_F(ChromotingPluginTest, TestNew) {
  NPMIMEType mimetype =
      const_cast<NPMIMEType>("pepper-application/x-chromoting-plugin");
  int16 argc;
  char* argn[4];
  char* argv[4];

  // Test 0 arguments.
  argc = 0;
  ASSERT_EQ(NPERR_GENERIC_ERROR, plugin_->New(mimetype, argc, argn, argv));

  // Test 1 argument (missing "src").
  argc = 1;
  argn[0] = const_cast<char*>("noturl");
  argv[0] = const_cast<char*>("random.value");
  ASSERT_EQ(NPERR_GENERIC_ERROR, plugin_->New(mimetype, argc, argn, argv));

  // Test "src" argument.
  argc = 1;
  argn[0] = const_cast<char*>("src");
  argv[0] = const_cast<char*>("chromotocol://name?user=u&auth=a&jid=j");
  ASSERT_EQ(NPERR_NO_ERROR, plugin_->New(mimetype, argc, argn, argv));

  ASSERT_EQ(NPERR_NO_ERROR, plugin_->Destroy(NULL));
}

TEST_F(ChromotingPluginTest, TestSetWindow) {
  NPWindow* window = fake_browser_->GetWindow();
  NPError result;

  result = plugin_->SetWindow(window);
  ASSERT_EQ(NPERR_NO_ERROR, result);
}
#endif

TEST_F(ChromotingPluginTest, ParseUrl) {
  const char url[] = "chromotocol://hostid?user=auser&auth=someauth&jid=ajid";
  std::string user_id;
  std::string auth_token;
  std::string host_jid;
  ASSERT_TRUE(
      ChromotingPlugin::ParseUrl(url, &user_id, &auth_token, &host_jid));

  EXPECT_EQ("auser", user_id);
  EXPECT_EQ("someauth", auth_token);
  EXPECT_EQ("ajid", host_jid);
}

}  // namespace remoting
