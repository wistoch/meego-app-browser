// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/renderer_host/test_render_view_host.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_factory.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/test/test_notification_tracker.h"
#include "chrome/test/test_tab_contents.h"
#include "chrome/test/testing_profile.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"

// TODO(port): get rid of this section, finish porting.
#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

using base::Time;

namespace {

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest : public RenderViewHostTestHarness {
 public:
  NavigationControllerTest() {}
};

// NavigationControllerHistoryTest ---------------------------------------------

class NavigationControllerHistoryTest : public NavigationControllerTest {
 public:
  NavigationControllerHistoryTest()
      : url0("http://foo1"),
        url1("http://foo1"),
        url2("http://foo1"),
        profile_manager_(NULL) {
  }

  virtual ~NavigationControllerHistoryTest() {
    // Prevent our base class from deleting the profile since profile's
    // lifetime is managed by profile_manager_.
    STLDeleteElements(&windows_);
  }

  // testing::Test overrides.
  virtual void SetUp() {
    NavigationControllerTest::SetUp();

    // Force the session service to be created.
    SessionService* service = new SessionService(profile());
    profile()->set_session_service(service);
    service->SetWindowType(window_id, Browser::TYPE_NORMAL);
    service->SetWindowBounds(window_id, gfx::Rect(0, 1, 2, 3), false);
    service->SetTabIndexInWindow(window_id,
                                 controller()->session_id(), 0);
    controller()->SetWindowID(window_id);

    session_helper_.set_service(service);
  }

  virtual void TearDown() {
    // Release profile's reference to the session service. Otherwise the file
    // will still be open and we won't be able to delete the directory below.
    profile()->set_session_service(NULL);
    session_helper_.set_service(NULL);

    // Make sure we wait for history to shut down before continuing. The task
    // we add will cause our message loop to quit once it is destroyed.
    HistoryService* history =
        profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
    if (history) {
      history->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
      MessageLoop::current()->Run();
    }

    // Do normal cleanup before deleting the profile directory below.
    NavigationControllerTest::TearDown();

    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // Deletes the current profile manager and creates a new one. Indirectly this
  // shuts down the history database and reopens it.
  void ReopenDatabase() {
    session_helper_.set_service(NULL);
    profile()->set_session_service(NULL);

    SessionService* service = new SessionService(profile());
    profile()->set_session_service(service);
    session_helper_.set_service(service);
  }

  void GetLastSession() {
    profile()->GetSessionService()->TabClosed(controller()->window_id(),
                                              controller()->session_id());

    ReopenDatabase();
    Time close_time;

    session_helper_.ReadWindows(&windows_);
  }

  CancelableRequestConsumer consumer;

  // URLs for testing.
  const GURL url0;
  const GURL url1;
  const GURL url2;

  std::vector<SessionWindow*> windows_;

  SessionID window_id;

  SessionServiceTestHelper session_helper_;

 private:
  ProfileManager* profile_manager_;
  std::wstring test_dir_;
  std::wstring profile_path_;
};

void RegisterForAllNavNotifications(TestNotificationTracker* tracker,
                                    NavigationController* controller) {
  tracker->ListenFor(NotificationType::NAV_ENTRY_COMMITTED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NotificationType::NAV_LIST_PRUNED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NotificationType::NAV_ENTRY_CHANGED,
                     Source<NavigationController>(controller));
}

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  EXPECT_TRUE(contents()->is_active());
  EXPECT_TRUE(controller());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->GetLastCommittedEntry());
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller()->GetEntryCount(), 0);
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, LoadURL) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, notifications.size());

  // The load should now be pending.
  EXPECT_EQ(controller()->GetEntryCount(), 0);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), -1);

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0U, notifications.size());

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // Load another...
  controller()->LoadURL(url2, GURL(), PageTransition::TYPED);

  // The load should now be pending.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // We should not have produced a new session history entry.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  controller()->DiscardNonCommittedEntries();
  EXPECT_EQ(0U, notifications.size());

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller()->LoadURL(kExistingURL1, GURL(),
                                  PageTransition::TYPED);
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Do a new navigation without making a pending one.
  const GURL kNewURL("http://see");
  rvh()->SendNavigate(99, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller()->GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a new pending navigation that is
// not the one that just loaded. This will happen if the user types in a URL to
// somewhere slow, and then navigates the current page before the typed URL
// commits.
TEST_F(NavigationControllerTest, LoadURL_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller()->LoadURL(kExistingURL1, GURL(),
                                  PageTransition::TYPED);
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  controller()->LoadURL(kExistingURL2, GURL(),
                                  PageTransition::TYPED);
  EXPECT_EQ(0U, notifications.size());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://see");
  rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller()->GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation. This will happen if the user hits back, but before that commits,
// they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_ExistingPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // First make some history.
  const GURL kExistingURL1("http://eh");
  controller()->LoadURL(kExistingURL1, GURL(),
                                  PageTransition::TYPED);
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://bee");
  controller()->LoadURL(kExistingURL2, GURL(),
                                  PageTransition::TYPED);
  rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  controller()->GoBack();
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(0, controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, controller()->GetLastCommittedEntryIndex());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://see");
  NavigationController::LoadCommittedDetails details;
  rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller()->GetPendingEntryIndex());
  EXPECT_EQ(2, controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller()->GetActiveEntry()->url());
}

TEST_F(NavigationControllerTest, Reload) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->Reload(true);
  EXPECT_EQ(0U, notifications.size());

  // The reload is pending.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->Reload(true);
  EXPECT_EQ(0U, notifications.size());

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_TRUE(controller()->CanGoForward());

  rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // The back navigation completed successfully.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_TRUE(controller()->CanGoForward());
}

// Tests what happens when a back navigation produces a new page.
TEST_F(NavigationControllerTest, Back_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_FALSE(controller()->CanGoBack());
  EXPECT_TRUE(controller()->CanGoForward());

  rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // The back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(controller()->GetEntryCount(), 3);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 2);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Receives a back message when there is a new pending navigation entry.
TEST_F(NavigationControllerTest, Back_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL kUrl1("http://foo1");
  const GURL kUrl2("http://foo2");
  const GURL kUrl3("http://foo3");

  // First navigate two places so we have some back history.
  rvh()->SendNavigate(0, kUrl1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  //controller()->LoadURL(kUrl2, PageTransition::TYPED);
  rvh()->SendNavigate(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Now start a new pending navigation and go back before it commits.
  controller()->LoadURL(kUrl3, GURL(), PageTransition::TYPED);
  EXPECT_EQ(-1, controller()->GetPendingEntryIndex());
  EXPECT_EQ(kUrl3, controller()->GetPendingEntry()->url());
  controller()->GoBack();

  // The pending navigation should now be the "back" item and the new one
  // should be gone.
  EXPECT_EQ(0, controller()->GetPendingEntryIndex());
  EXPECT_EQ(kUrl1, controller()->GetPendingEntry()->url());
}

// Receives a back message when there is a different renavigation already
// pending.
TEST_F(NavigationControllerTest, Back_OtherBackPending) {
  const GURL kUrl1("http://foo/1");
  const GURL kUrl2("http://foo/2");
  const GURL kUrl3("http://foo/3");

  // First navigate three places so we have some back history.
  rvh()->SendNavigate(0, kUrl1);
  rvh()->SendNavigate(1, kUrl2);
  rvh()->SendNavigate(2, kUrl3);

  // With nothing pending, say we get a navigation to the second entry.
  rvh()->SendNavigate(1, kUrl2);

  // We know all the entries have the same site instance, so we can just grab
  // a random one for looking up other entries.
  SiteInstance* site_instance =
      controller()->GetLastCommittedEntry()->site_instance();

  // That second URL should be the last committed and it should have gotten the
  // new title.
  EXPECT_EQ(kUrl2, controller()->GetEntryWithPageID(
      TAB_CONTENTS_WEB, site_instance, 1)->url());
  EXPECT_EQ(1, controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(-1, controller()->GetPendingEntryIndex());

  // Now go forward to the last item again and say it was committed.
  controller()->GoForward();
  rvh()->SendNavigate(2, kUrl3);

  // Now start going back one to the second page. It will be pending.
  controller()->GoBack();
  EXPECT_EQ(1, controller()->GetPendingEntryIndex());
  EXPECT_EQ(2, controller()->GetLastCommittedEntryIndex());

  // Not synthesize a totally new back event to the first page. This will not
  // match the pending one.
  rvh()->SendNavigate(0, kUrl1);

  // The navigation should not have affected the pending entry.
  EXPECT_EQ(1, controller()->GetPendingEntryIndex());

  // But the navigated entry should be the last committed.
  EXPECT_EQ(0, controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kUrl1, controller()->GetLastCommittedEntry()->url());
}

// Tests what happens when we navigate forward successfully.
TEST_F(NavigationControllerTest, Forward) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoBack();
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoForward();

  // We should now have a pending navigation to go forward.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // The forward navigation completed successfully.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests what happens when a forward navigation produces a new page.
TEST_F(NavigationControllerTest, Forward_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoBack();
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  controller()->GoForward();
  EXPECT_EQ(0U, notifications.size());

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_TRUE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());

  rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check2AndReset(
      NotificationType::NAV_LIST_PRUNED,
      NotificationType::NAV_ENTRY_COMMITTED));

  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

// Tests navigation via link click within a subframe. A new navigation entry
// should be created.
TEST_F(NavigationControllerTest, NewSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(url1, details.previous_url);
  EXPECT_FALSE(details.is_auto);
  EXPECT_FALSE(details.is_in_page);
  EXPECT_FALSE(details.is_main_frame);

  // The new entry should be appended.
  EXPECT_EQ(2, controller()->GetEntryCount());

  // New entry should refer to the new page, but the old URL (entries only
  // reflect the toplevel URL).
  EXPECT_EQ(url1, details.entry->url());
  EXPECT_EQ(params.page_id, details.entry->page_id());
}

// Some pages create a popup, then write an iframe into it. This causes a
// subframe navigation without having any committed entry. Such navigations
// just get thrown on the ground, but we shouldn't crash.
TEST_F(NavigationControllerTest, SubframeOnEmptyPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // Navigation controller currently has no entries.
  const GURL url("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url;
  params.transition = PageTransition::AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;

  NavigationController::LoadCommittedDetails details;
  EXPECT_FALSE(controller()->RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());
}

// Auto subframes are ones the page loads automatically like ads. They should
// not create new navigation entries.
TEST_F(NavigationControllerTest, AutoSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PageTransition::AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // Navigating should do nothing.
  NavigationController::LoadCommittedDetails details;
  EXPECT_FALSE(controller()->RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());

  // There should still be only one entry.
  EXPECT_EQ(1, controller()->GetEntryCount());
}

// Tests navigation and then going back to a subframe navigation.
TEST_F(NavigationControllerTest, BackSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // Main page.
  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // First manual subframe navigation.
  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // This should generate a new entry.
  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller()->GetEntryCount());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  params.page_id = 2;
  params.url = url3;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller()->GetEntryCount());
  EXPECT_EQ(2, controller()->GetCurrentEntryIndex());

  // Go back one.
  controller()->GoBack();
  params.url = url2;
  params.page_id = 1;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller()->GetEntryCount());
  EXPECT_EQ(1, controller()->GetCurrentEntryIndex());

  // Go back one more.
  controller()->GoBack();
  params.url = url1;
  params.page_id = 0;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller()->GetEntryCount());
  EXPECT_EQ(0, controller()->GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, LinkClick) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller()->GetEntryCount(), 2);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, InPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  // Main page. Note that we need "://" so this URL is treated as "standard"
  // which are the only ones that can have a ref.
  const GURL url1("http:////foo");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));

  // First navigation.
  const GURL url2("http:////foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // This should generate a new entry.
  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(controller()->RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller()->GetEntryCount());

  // Go back one.
  ViewHostMsg_FrameNavigate_Params back_params(params);
  controller()->GoBack();
  back_params.url = url1;
  back_params.page_id = 0;
  EXPECT_TRUE(controller()->RendererDidNavigate(back_params,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller()->GetEntryCount());
  EXPECT_EQ(0, controller()->GetCurrentEntryIndex());
  EXPECT_EQ(back_params.url, controller()->GetActiveEntry()->url());

  // Go forward
  ViewHostMsg_FrameNavigate_Params forward_params(params);
  controller()->GoForward();
  forward_params.url = url2;
  forward_params.page_id = 1;
  EXPECT_TRUE(controller()->RendererDidNavigate(forward_params,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      NotificationType::NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller()->GetEntryCount());
  EXPECT_EQ(1, controller()->GetCurrentEntryIndex());
  EXPECT_EQ(forward_params.url,
            controller()->GetActiveEntry()->url());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  controller()->GoBack();
  EXPECT_TRUE(controller()->RendererDidNavigate(back_params,
                                                          &details));
  controller()->GoForward();
  EXPECT_TRUE(controller()->RendererDidNavigate(forward_params,
                                                          &details));
  EXPECT_EQ(forward_params.url,
            controller()->GetActiveEntry()->url());
}

namespace {

// NotificationObserver implementation used in verifying we've received the
// NotificationType::NAV_LIST_PRUNED method.
class PrunedListener : public NotificationObserver {
 public:
  explicit PrunedListener(NavigationController* controller)
      : notification_count_(0) {
    registrar_.Add(this, NotificationType::NAV_LIST_PRUNED,
                   Source<NavigationController>(controller));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::NAV_LIST_PRUNED) {
      notification_count_++;
      details_ = *(Details<NavigationController::PrunedDetails>(details).ptr());
    }
  }

  // Number of times NAV_LIST_PRUNED has been observed.
  int notification_count_;

  // Details from the last NAV_LIST_PRUNED.
  NavigationController::PrunedDetails details_;

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrunedListener);
};

}

// Tests that we limit the number of navigation entries created correctly.
TEST_F(NavigationControllerTest, EnforceMaxNavigationCount) {
  size_t original_count = NavigationController::max_entry_count();
  const int kMaxEntryCount = 5;

  NavigationController::set_max_entry_count(kMaxEntryCount);

  int url_index;
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    GURL url(StringPrintf("http://www.a.com/%d", url_index));
    controller()->LoadURL(url, GURL(), PageTransition::TYPED);
    rvh()->SendNavigate(url_index, url);
  }

  EXPECT_EQ(controller()->GetEntryCount(), kMaxEntryCount);

  // Created a PrunedListener to observe prune notifications.
  PrunedListener listener(controller());

  // Navigate some more.
  GURL url(StringPrintf("http://www.a.com/%d", url_index));
  controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(url_index, url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1, listener.notification_count_);
  EXPECT_TRUE(listener.details_.from_front);
  EXPECT_EQ(1, listener.details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller()->GetEntryAtIndex(0)->url(),
            GURL("http:////www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    url = GURL(StringPrintf("http:////www.a.com/%d", url_index));
    controller()->LoadURL(url, GURL(), PageTransition::TYPED);
      rvh()->SendNavigate(url_index, url);
    url_index++;
  }
  EXPECT_EQ(controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller()->GetEntryAtIndex(0)->url(),
            GURL("http:////www.a.com/4"));

  NavigationController::set_max_entry_count(original_count);
}

// Tests that we can do a restore and navigate to the restored entries and
// everything is updated properly. This can be tricky since there is no
// SiteInstance for the entries created initially.
TEST_F(NavigationControllerTest, RestoreNavigate) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<TabNavigation> navigations;
  navigations.push_back(TabNavigation(0, url, GURL(),
                                      ASCIIToUTF16("Title"), "state",
                                      PageTransition::LINK));
  NavigationController* our_controller =
      new NavigationController(profile(), navigations, 0);
  our_controller->GoToIndex(0);

  // We should now have one entry, and it should be "pending".
  EXPECT_EQ(1, our_controller->GetEntryCount());
  EXPECT_EQ(our_controller->GetEntryAtIndex(0),
            our_controller->GetPendingEntry());
  EXPECT_EQ(0, our_controller->GetEntryAtIndex(0)->page_id());

  // Say we navigated to that entry.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  NavigationController::LoadCommittedDetails details;
  our_controller->RendererDidNavigate(params, &details);

  // There should be no longer any pending entry and one committed one. This
  // means that we were able to locate the entry, assign its site instance, and
  // commit it properly.
  EXPECT_EQ(1, our_controller->GetEntryCount());
  EXPECT_EQ(0, our_controller->GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller->GetPendingEntry());
  EXPECT_EQ(url,
            our_controller->GetLastCommittedEntry()->site_instance()->site());

  // Clean up the navigation controller.
  our_controller->Destroy();
}

// Make sure that the page type and stuff is correct after an interstitial.
TEST_F(NavigationControllerTest, Interstitial) {
  // First navigate somewhere normal.
  const GURL url1("http://foo");
  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("http://bar");
  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  controller()->GetPendingEntry()->set_page_type(
      NavigationEntry::INTERSTITIAL_PAGE);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  rvh()->SendNavigate(1, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, controller()->GetLastCommittedEntry()->url());
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE,
            controller()->GetLastCommittedEntry()->page_type());
}

TEST_F(NavigationControllerTest, RemoveEntry) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");
  const GURL url5("http://foo5");
  const GURL pending_url("http://pending");
  const GURL default_url("http://default");

  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url1);
  controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(1, url2);
  controller()->LoadURL(url3, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(2, url3);
  controller()->LoadURL(url4, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(3, url4);
  controller()->LoadURL(url5, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(4, url5);

  // Remove the last entry.
  controller()->RemoveEntryAtIndex(
      controller()->GetEntryCount() - 1, default_url);
  EXPECT_EQ(4, controller()->GetEntryCount());
  EXPECT_EQ(3, controller()->GetLastCommittedEntryIndex());
  NavigationEntry* pending_entry = controller()->GetPendingEntry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == url4);

  // Add a pending entry.
  controller()->LoadURL(pending_url, GURL(), PageTransition::TYPED);
  // Now remove the last entry.
  controller()->RemoveEntryAtIndex(
      controller()->GetEntryCount() - 1, default_url);
  // The pending entry should have been discarded and the last committed entry
  // removed.
  EXPECT_EQ(3, controller()->GetEntryCount());
  EXPECT_EQ(2, controller()->GetLastCommittedEntryIndex());
  pending_entry = controller()->GetPendingEntry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == url3);

  // Remove an entry which is not the last committed one.
  controller()->RemoveEntryAtIndex(0, default_url);
  EXPECT_EQ(2, controller()->GetEntryCount());
  EXPECT_EQ(1, controller()->GetLastCommittedEntryIndex());
  // No navigation should have been initiated since we did not remove the
  // current entry.
  EXPECT_FALSE(controller()->GetPendingEntry());

  // Remove the 2 remaining entries.
  controller()->RemoveEntryAtIndex(1, default_url);
  controller()->RemoveEntryAtIndex(0, default_url);

  // This should have created a pending default entry.
  EXPECT_EQ(0, controller()->GetEntryCount());
  EXPECT_EQ(-1, controller()->GetLastCommittedEntryIndex());
  pending_entry = controller()->GetPendingEntry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == default_url);
}

// Tests the transient entry, making sure it goes away with all navigations.
TEST_F(NavigationControllerTest, TransientEntry) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, controller());

  const GURL url0("http://foo0");
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");
  const GURL transient_url("http://transient");

  controller()->LoadURL(url0, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url0);
  controller()->LoadURL(url1, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(1, url1);

  notifications.Reset();

  // Adding a transient with no pending entry.
  NavigationEntry* transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);

  // We should not have received any notifications.
  EXPECT_EQ(0U, notifications.size());

  // Check our state.
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  EXPECT_EQ(controller()->GetEntryCount(), 3);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller()->GetLastCommittedEntry());
  EXPECT_FALSE(controller()->GetPendingEntry());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);

  // Navigate.
  controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(2, url2);

  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url2, controller()->GetActiveEntry()->url());
  EXPECT_EQ(controller()->GetEntryCount(), 3);

  // Add a transient again, then navigate with no pending entry this time.
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  rvh()->SendNavigate(3, url3);
  // Transient entry should be gone.
  EXPECT_EQ(url3, controller()->GetActiveEntry()->url());
  EXPECT_EQ(controller()->GetEntryCount(), 4);

  // Initiate a navigation, add a transient then commit navigation.
  controller()->LoadURL(url4, GURL(), PageTransition::TYPED);
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  rvh()->SendNavigate(4, url4);
  EXPECT_EQ(url4, controller()->GetActiveEntry()->url());
  EXPECT_EQ(controller()->GetEntryCount(), 5);

  // Add a transient and go back.  This should simply remove the transient.
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  EXPECT_TRUE(controller()->CanGoBack());
  EXPECT_FALSE(controller()->CanGoForward());
  controller()->GoBack();
  // Transient entry should be gone.
  EXPECT_EQ(url4, controller()->GetActiveEntry()->url());
  EXPECT_EQ(controller()->GetEntryCount(), 5);
  rvh()->SendNavigate(3, url3);

  // Add a transient and go to an entry before the current one.
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  controller()->GoToIndex(1);
  // The navigation should have been initiated, transient entry should be gone.
  EXPECT_EQ(url1, controller()->GetActiveEntry()->url());
  rvh()->SendNavigate(1, url1);

  // Add a transient and go to an entry after the current one.
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  controller()->GoToIndex(3);
  // The navigation should have been initiated, transient entry should be gone.
  // Because of the transient entry that is removed, going to index 3 makes us
  // land on url2.
  EXPECT_EQ(url2, controller()->GetActiveEntry()->url());
  rvh()->SendNavigate(2, url2);

  // Add a transient and go forward.
  transient_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  transient_entry->set_url(transient_url);
  controller()->AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller()->GetActiveEntry()->url());
  EXPECT_TRUE(controller()->CanGoForward());
  controller()->GoForward();
  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url3, controller()->GetActiveEntry()->url());
  rvh()->SendNavigate(3, url3);

  // Ensure the URLS are correct.
  EXPECT_EQ(controller()->GetEntryCount(), 5);
  EXPECT_EQ(controller()->GetEntryAtIndex(0)->url(), url0);
  EXPECT_EQ(controller()->GetEntryAtIndex(1)->url(), url1);
  EXPECT_EQ(controller()->GetEntryAtIndex(2)->url(), url2);
  EXPECT_EQ(controller()->GetEntryAtIndex(3)->url(), url3);
  EXPECT_EQ(controller()->GetEntryAtIndex(4)->url(), url4);
}

// Tests that IsInPageNavigation returns appropriate results.  Prevents
// regression for bug 1126349.
TEST_F(NavigationControllerTest, IsInPageNavigation) {
  // Navigate to URL with no refs.
  const GURL url("http://www.google.com/home.html");
  rvh()->SendNavigate(0, url);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller()->IsURLInPageNavigation(url));
  const GURL other_url("http://www.google.com/add.html");
  EXPECT_FALSE(controller()->IsURLInPageNavigation(other_url));
  const GURL url_with_ref("http://www.google.com/home.html#my_ref");
  EXPECT_TRUE(controller()->IsURLInPageNavigation(url_with_ref));

  // Navigate to URL with refs.
  rvh()->SendNavigate(1, url_with_ref);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller()->IsURLInPageNavigation(url_with_ref));
  EXPECT_FALSE(controller()->IsURLInPageNavigation(url));
  EXPECT_FALSE(controller()->IsURLInPageNavigation(other_url));
  const GURL other_url_with_ref("http://www.google.com/home.html#my_other_ref");
  EXPECT_TRUE(controller()->IsURLInPageNavigation(
      other_url_with_ref));
}

// Some pages can have subframes with the same base URL (minus the reference) as
// the main page. Even though this is hard, it can happen, and we don't want
// these subframe navigations to affect the toplevel document. They should
// instead be ignored.  http://crbug.com/5585
TEST_F(NavigationControllerTest, SameSubframe) {
  // Navigate the main frame.
  const GURL url("http://www.google.com/");
  rvh()->SendNavigate(0, url);

  // We should be at the first navigation entry.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);

  // Navigate a subframe that would normally count as in-page.
  const GURL subframe("http://www.google.com/#");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = subframe;
  params.transition = PageTransition::AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  NavigationController::LoadCommittedDetails details;
  EXPECT_FALSE(controller()->RendererDidNavigate(params, &details));

  // Nothing should have changed.
  EXPECT_EQ(controller()->GetEntryCount(), 1);
  EXPECT_EQ(controller()->GetLastCommittedEntryIndex(), 0);
}

// A basic test case. Navigates to a single url, and make sure the history
// db matches.
TEST_F(NavigationControllerHistoryTest, Basic) {
  controller()->LoadURL(url0, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(0, url0);

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 1);
  session_helper_.AssertTabEquals(0, 0, 1, *(windows_[0]->tabs[0]));
  TabNavigation nav1(0, url0, GURL(), string16(),
                     webkit_glue::CreateHistoryStateForURL(url0),
                     PageTransition::LINK);
  session_helper_.AssertNavigationEquals(nav1, windows_[0]->tabs[0]->navigations[0]);
}

/* TODO(brettw) These test pass on my local machine but fail on the buildbot
   cleaning up the directory after they run. This should be fixed.

// Navigates to three urls, then goes back and make sure the history database
// is in sync.
TEST_F(NavigationControllerHistoryTest, NavigationThenBack) {
  rvh()->SendNavigate(0, url0);
  rvh()->SendNavigate(1, url1);
  rvh()->SendNavigate(2, url2);

  controller()->GoBack();
  rvh()->SendNavigate(1, url1);

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 3);
  session_helper_.AssertTabEquals(0, 1, 3, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, GURL(), string16(),
                    webkit_glue::CreateHistoryStateForURL(url0),
                    PageTransition::LINK);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[0]);
  nav.set_url(url1);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[1]);
  nav.set_url(url2);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[2]);
}

// Navigates to three urls, then goes back twice, then loads a new url.
TEST_F(NavigationControllerHistoryTest, NavigationPruning) {
  rvh()->SendNavigate(0, url0);
  rvh()->SendNavigate(1, url1);
  rvh()->SendNavigate(2, url2);

  controller()->GoBack();
  rvh()->SendNavigate(1, url1);

  controller()->GoBack();
  rvh()->SendNavigate(0, url0);

  rvh()->SendNavigate(3, url2);

  // Now have url0, and url2.

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 2);
  session_helper_.AssertTabEquals(0, 1, 2, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, GURL(), string16(),
                    webkit_glue::CreateHistoryStateForURL(url0),
                    PageTransition::LINK);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[0]);
  nav.set_url(url2);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[1]);
}
*/