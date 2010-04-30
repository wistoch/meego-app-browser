// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "googleurl/src/gurl.h"
#include "net/socket/ssl_test_util.h"

class BookmarkModel;
class BookmarkNode;
class Profile;
namespace net {
class ScopedDefaultHostResolverProc;
}

namespace switches {
extern const wchar_t kSyncUserForTest[];
extern const wchar_t kSyncPasswordForTest[];
}

// TODO(timsteele): This should be moved out of personalization_unit_tests into
// its own project that doesn't get run by default on the standard buildbot
// without a valid sync server set up.
class LiveSyncTest : public InProcessBrowserTest {
 public:
  LiveSyncTest();
  ~LiveSyncTest();

  virtual void SetUp() {
    // At this point, the browser hasn't been launched, and no services are
    // available.  But we can verify our command line parameters and fail
    // early.
    const CommandLine* cl = CommandLine::ForCurrentProcess();
    username_ = WideToUTF8(cl->GetSwitchValue(switches::kSyncUserForTest));
    password_ = WideToUTF8(cl->GetSwitchValue(switches::kSyncPasswordForTest));
    ASSERT_FALSE(username_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncUserForTest;
    ASSERT_FALSE(password_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncPasswordForTest;

    // Unless a sync server was explicitly provided, run a test one locally.
    // TODO(ncarter): It might be better to allow the user to specify a choice
    // of sync server "providers" -- a script that could locate (or allocate)
    // a sync server instance, possibly on some remote host.  The provider
    // would be invoked before each test.
    if (!cl->HasSwitch(switches::kSyncServiceURL))
      SetUpLocalTestServer();

    // Yield control back to the InProcessBrowserTest framework.
    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpLocalTestServer() {
    bool success = server_.Start(net::TestServerLauncher::ProtoHTTP,
        server_.kHostName, server_.kOKHTTPSPort,
        FilePath(), FilePath(), std::wstring());
    ASSERT_TRUE(success);

    CommandLine* cl = CommandLine::ForCurrentProcess();
    cl->AppendSwitchWithValue(switches::kSyncServiceURL,
        StringPrintf("http://%s:%d/chromiumsync", server_.kHostName,
            server_.kOKHTTPSPort));
  }

  // Append command line flag to enable sync.
  virtual void SetUpCommandLine(CommandLine* command_line) {
  }

  // Helper to get a handle on a bookmark in |m| when the url is known to be
  // unique.
  static const BookmarkNode* GetByUniqueURL(BookmarkModel* m, const GURL& url);

  // Helper to ProfileManager::CreateProfile that handles path creation.
  static Profile* MakeProfile(const FilePath::CharType* name);

  // Utility to block (by running the current MessageLoop) until the model has
  // loaded.  Note this is required instead of using m->BlockTillLoaded, as that
  // cannot be called from the main thread (deadlock will occur).
  static void BlockUntilLoaded(BookmarkModel* m);

 protected:
  std::string username_;
  std::string password_;

  virtual void SetUpInProcessBrowserTestFixture();
  virtual void TearDownInProcessBrowserTestFixture();

 private:
  // LiveBookmarksSyncTests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com.  We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  net::TestServerLauncher server_;

  DISALLOW_COPY_AND_ASSIGN(LiveSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
