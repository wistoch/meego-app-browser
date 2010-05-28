// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_test.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

namespace switches {
const wchar_t kSyncUserForTest[] = L"sync-user-for-test";
const wchar_t kSyncPasswordForTest[] = L"sync-password-for-test";
}

// BookmarkLoadObserver is used when blocking until the BookmarkModel
// finishes loading. As soon as the BookmarkModel finishes loading the message
// loop is quit.
class BookmarkLoadObserver : public BookmarkModelObserver {
 public:
  BookmarkLoadObserver() {}
  virtual void Loaded(BookmarkModel* model) {
    MessageLoop::current()->Quit();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadObserver);
};

LiveSyncTest::LiveSyncTest() : started_local_test_server_(false) {
}

LiveSyncTest::~LiveSyncTest() {
}

// static
void LiveSyncTest::BlockUntilLoaded(BookmarkModel* m) {
  if (m->IsLoaded())
    return;
  BookmarkLoadObserver observer;
  m->AddObserver(&observer);
  ui_test_utils::RunMessageLoop();
  m->RemoveObserver(&observer);
  ASSERT_TRUE(m->IsLoaded());
}

// static
const BookmarkNode* LiveSyncTest::GetByUniqueURL(BookmarkModel* m,
                                                 const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  m->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  return nodes[0];
}

// static
Profile* LiveSyncTest::MakeProfile(const FilePath::CharType* name) {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);

  return ProfileManager::CreateProfile(path.Append(name));
}

void LiveSyncTest::SetUpInProcessBrowserTestFixture() {
  // We don't take a reference to |resolver|, but mock_host_resolver_override_
  // does, so effectively assumes ownership.
  net::RuleBasedHostResolverProc* resolver =
      new net::RuleBasedHostResolverProc(host_resolver());
  resolver->AllowDirectLookup("*.google.com");
  // Allow direct lookup of thawte.com.  On Linux, we use Chromium's nss
  // implementation which uses ocsp.thawte.com for certificate verification.
  // Without this, running the test case on Linux causes an error as we make an
  // external DNS lookup of "ocsp.thawte.com".
  resolver->AllowDirectLookup("*.thawte.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver));
}

void LiveSyncTest::TearDownInProcessBrowserTestFixture() {
  mock_host_resolver_override_.reset();
}
