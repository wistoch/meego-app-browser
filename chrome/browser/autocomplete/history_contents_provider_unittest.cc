// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/history_contents_provider.h"
#include "chrome/browser/history/history.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestEntry {
  const char* url;
  const wchar_t* title;
  const wchar_t* body;
} test_entries[] = {
  {"http://www.google.com/1", L"PAGEONE 1",   L"FOO some body text"},
  {"http://www.google.com/2", L"PAGEONE 2",   L"FOO some more blah blah"},
  {"http://www.google.com/3", L"PAGETHREE 3", L"BAR some hello world for you"},
};

// For comparing TestEntry.url with wide strings generated by the autocomplete
// code
bool UrlIs(const char* url, const std::wstring& str) {
  return WideToUTF8(str) == std::string(url);
}

class HistoryContentsProviderTest : public testing::Test,
                                    public ACProviderListener {
 public:

  void RunQuery(const AutocompleteInput& input,
                bool minimal_changes,
                bool synchronous_only) {
    provider_->Start(input, minimal_changes, synchronous_only);

    // When we're waiting for asynchronous messages, we have to spin the message
    // loop. This will be exited in the OnProviderUpdate function when complete.
    if (!synchronous_only)
      MessageLoop::current()->Run();
  }

  const ACMatches& matches() const { return provider_->matches(); }

 private:
  // testing::Test
  virtual void SetUp() {
    PathService::Get(base::DIR_TEMP, &history_dir_);
    file_util::AppendToPath(&history_dir_, L"HistoryContentProviderTest");
    file_util::Delete(history_dir_, true);  // Normally won't exist.
    file_util::CreateDirectoryW(history_dir_);

    history_service_ = new HistoryService;
    history_service_->Init(history_dir_, NULL);

    // Populate history.
    for (int i = 0; i < arraysize(test_entries); i++) {
      // We need the ID scope and page ID so that the visit tracker can find it.
      // We just use the index for the page ID below.
      const void* id_scope = reinterpret_cast<void*>(1);
      GURL url(test_entries[i].url);

      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      Time t = Time::Now() - TimeDelta::FromDays(arraysize(test_entries) + i);

      history_service_->AddPage(url, t, id_scope, i, GURL(),
          PageTransition::LINK, HistoryService::RedirectList());
      history_service_->SetPageTitle(url, test_entries[i].title);
      history_service_->SetPageContents(url, test_entries[i].body);
    }

    provider_ = new HistoryContentsProvider(this, history_service_);
  }

  virtual void TearDown() {
    history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
    history_service_->Cleanup();
    provider_ = NULL;
    history_service_ = NULL;

    // Wait for history thread to complete (the QuitTask will cause it to exit
    // on destruction). Note: if this never terminates, somebody is probably
    // leaking a reference to the history backend, so it never calls our
    // destroy task.
    MessageLoop::current()->Run();

    file_util::Delete(history_dir_, true);
  }

  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches) {
    // When we quit, the test will get back control.
    MessageLoop::current()->Quit();
  }

  MessageLoopForUI message_loop_;
  
  std::wstring history_dir_;

  scoped_refptr<HistoryContentsProvider> provider_;
  scoped_refptr<HistoryService> history_service_;
};

}  // namespace

TEST_F(HistoryContentsProviderTest, Body) {
  AutocompleteInput input(L"FOO", std::wstring(), true);
  RunQuery(input, false, false);

  // The results should be the first two pages, in decreasing order.
  const ACMatches& m = matches();
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(UrlIs(test_entries[1].url, m[0].destination_url));
  EXPECT_STREQ(test_entries[1].title, m[0].description.c_str());
  EXPECT_TRUE(UrlIs(test_entries[0].url, m[1].destination_url));
  EXPECT_STREQ(test_entries[0].title, m[1].description.c_str());
}

TEST_F(HistoryContentsProviderTest, Title) {
  AutocompleteInput input(L"PAGEONE", std::wstring(), true);
  RunQuery(input, false, false);

  // The results should be the first two pages.
  const ACMatches& m = matches();
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(UrlIs(test_entries[1].url, m[0].destination_url));
  EXPECT_STREQ(test_entries[1].title, m[0].description.c_str());
  EXPECT_TRUE(UrlIs(test_entries[0].url, m[1].destination_url));
  EXPECT_STREQ(test_entries[0].title, m[1].description.c_str());
}

// The "minimal changes" flag should mean that we don't re-query the DB.
TEST_F(HistoryContentsProviderTest, MinimalChanges) {
  AutocompleteInput input(L"PAGEONE", std::wstring(), true);

  // A minimal changes request when there have been no real queries should
  // give us no results.
  RunQuery(input, true, true);
  const ACMatches& m1 = matches();
  EXPECT_EQ(0, m1.size());

  // Now do a "regular" query to get the results.
  RunQuery(input, false, false);
  const ACMatches& m2 = matches();
  EXPECT_EQ(2, m2.size());

  // Now do a minimal one where we want synchronous results, and the results
  // should still be there.
  RunQuery(input, true, true);
  const ACMatches& m3 = matches();
  EXPECT_EQ(2, m3.size());
}
