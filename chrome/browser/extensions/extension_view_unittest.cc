// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/extensions/extension_shelf.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/test_extension_loader.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// How long to wait for the extension to put up a javascript alert before giving
// up.
const int kAlertTimeoutMs = 20000;

// The extension we're using as our test case.
const char* kExtensionId = "00123456789abcdef0123456789abcdef0123456";

};  // namespace

// This class starts up an extension process and waits until it tries to put
// up a javascript alert.
class MockExtensionHost : public ExtensionHost {
 public:
  MockExtensionHost(Extension* extension, const GURL& url,
                    SiteInstance* instance)
      : ExtensionHost(extension, instance, url, NULL),
        got_message_(false) {
    CreateRenderView(NULL);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        new MessageLoop::QuitTask, kAlertTimeoutMs);
    ui_test_utils::RunMessageLoop();
  }

  virtual ExtensionFunctionDispatcher* CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host) {
    NOTREACHED();
    return NULL;
  }

  bool got_message() { return got_message_; }
 private:
  virtual void RunJavaScriptMessage(
      const std::wstring& message,
      const std::wstring& default_prompt,
      const GURL& frame_url,
      const int flags,
      IPC::Message* reply_msg,
      bool* did_suppress_message) {
    got_message_ = true;
    MessageLoopForUI::current()->Quit();

    // Call super, otherwise we'll leak reply_msg.
    ExtensionHost::RunJavaScriptMessage(
        message, default_prompt, frame_url, flags,
        reply_msg, did_suppress_message);
  }

  bool got_message_;
};

class ExtensionViewTest : public InProcessBrowserTest {
 public:
  virtual void SetUp() {
    // Initialize the error reporter here, otherwise BrowserMain will create it
    // with the wrong MessageLoop.
    ExtensionErrorReporter::Init(false);

    // Use single-process in an attempt to speed it up and make it less flaky.
    //EnableSingleProcess();

    InProcessBrowserTest::SetUp();
  }
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableExtensions);
  }
};

// Tests that ExtensionView starts an extension process and runs the script
// contained in the extension's "index.html" file.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest, Index) {
  // Get the path to our extension.
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").
      AppendASCII("good").AppendASCII("extension1").AppendASCII("1");
  ASSERT_TRUE(file_util::DirectoryExists(path));  // sanity check

  // Wait for the extension to load and grab a pointer to it.
  TestExtensionLoader loader(browser()->profile());
  Extension* extension = loader.Load(kExtensionId, path);
  ASSERT_TRUE(extension);
  GURL url = Extension::GetResourceURL(extension->url(), "toolstrip1.html");

  // Start the extension process and wait for it to show a javascript alert.
  MockExtensionHost host(extension, url,
      browser()->profile()->GetExtensionProcessManager()->
          GetSiteInstanceForURL(url));
  EXPECT_TRUE(host.got_message());
}

// Tests that the ExtensionShelf initializes properly, notices that
// an extension loaded and has a view available, and then sets that up
// properly.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest, Shelf) {
  // When initialized, there are no extension views and the preferred height
  // should be zero.
  scoped_ptr<ExtensionShelf> shelf(new ExtensionShelf(browser()));
  EXPECT_EQ(shelf->GetChildViewCount(), 0);
  EXPECT_EQ(shelf->GetPreferredSize().height(), 0);

  // Get the path to our extension.
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").
      AppendASCII("good").AppendASCII("extension1").AppendASCII("1");
  ASSERT_TRUE(file_util::DirectoryExists(path));  // sanity check

  // Wait for the extension to load and grab a pointer to it.
  TestExtensionLoader loader(browser()->profile());
  Extension* extension = loader.Load(kExtensionId, path);
  ASSERT_TRUE(extension);
  GURL url = Extension::GetResourceURL(extension->url(), "toolstrip1.html");

  // There should now be two extension views and preferred height of the view
  // should be non-zero.
  EXPECT_EQ(shelf->GetChildViewCount(), 2);
  EXPECT_NE(shelf->GetPreferredSize().height(), 0);
}
