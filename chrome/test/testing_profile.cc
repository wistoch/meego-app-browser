// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_profile.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/dom_ui/ntp_resource_cache.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/database/database_tracker.h"

#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif

using base::Time;
using testing::NiceMock;
using testing::Return;

namespace {

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.

class QuittingHistoryDBTask : public HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    return true;
  }

  virtual void DoneRunOnMainThread() {
    MessageLoop::current()->Quit();
  }

 private:
  ~QuittingHistoryDBTask() {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

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

// This context is used to assist testing the CookieMonster by providing a
// valid CookieStore. This can probably be expanded to test other aspects of
// the context as well.
class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext() {
    cookie_store_ = new net::CookieMonster(NULL, NULL);
  }
};

// Used to return a dummy context (normally the context is on the IO thread).
// The one here can be run on the main test thread. Note that this can lead to
// a leak if your test does not have a ChromeThread::IO in it because
// URLRequestContextGetter is defined as a ReferenceCounted object with a
// special trait that deletes it on the IO thread.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  virtual URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() {
    return ChromeThread::GetMessageLoopProxyForThread(ChromeThread::IO);
  }

 private:
  scoped_refptr<URLRequestContext> context_;
};

class TestExtensionURLRequestContext : public URLRequestContext {
 public:
  TestExtensionURLRequestContext() {
    net::CookieMonster* cookie_monster = new net::CookieMonster(NULL, NULL);
    const char* schemes[] = {chrome::kExtensionScheme};
    cookie_monster->SetCookieableSchemes(schemes, 1);
    cookie_store_ = cookie_monster;
  }
};

class TestExtensionURLRequestContextGetter : public URLRequestContextGetter {
 public:
  virtual URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestExtensionURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() {
    return ChromeThread::GetMessageLoopProxyForThread(ChromeThread::IO);
  }

 private:
  scoped_refptr<URLRequestContext> context_;
};

}  // namespace

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      created_theme_provider_(false),
      has_history_service_(false),
      off_the_record_(false),
      last_session_exited_cleanly_(true) {
  PathService::Get(base::DIR_TEMP, &path_);
  path_ = path_.Append(FILE_PATH_LITERAL("TestingProfilePath"));
  file_util::Delete(path_, true);
  file_util::CreateDirectory(path_);
}

TestingProfile::TestingProfile(int count)
    : start_time_(Time::Now()),
      created_theme_provider_(false),
      has_history_service_(false),
      off_the_record_(false),
      last_session_exited_cleanly_(true) {
  PathService::Get(base::DIR_TEMP, &path_);
  path_ = path_.Append(FILE_PATH_LITERAL("TestingProfilePath"));
  path_ = path_.AppendASCII(IntToString(count));
  file_util::Delete(path_, true);
  file_util::CreateDirectory(path_);
}

TestingProfile::~TestingProfile() {
  NotificationService::current()->Notify(
      NotificationType::PROFILE_DESTROYED,
      Source<Profile>(this),
      NotificationService::NoDetails());
  DestroyHistoryService();
  // FaviconService depends on HistoryServce so destroying it later.
  DestroyFaviconService();
  DestroyWebDataService();
  file_util::Delete(path_, true);
}

void TestingProfile::CreateFaviconService() {
  favicon_service_ = NULL;
  favicon_service_ = new FaviconService(this);
}

void TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  if (history_service_.get())
    history_service_->Cleanup();

  history_service_ = NULL;

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kHistoryFilename);
    file_util::Delete(path, false);
  }
  history_service_ = new HistoryService(this);
  history_service_->Init(GetPath(), bookmark_bar_model_.get(), no_db);
}

void TestingProfile::DestroyFaviconService() {
  if (!favicon_service_.get())
    return;
  favicon_service_ = NULL;
}

void TestingProfile::DestroyHistoryService() {
  if (!history_service_.get())
    return;

  history_service_->NotifyRenderProcessHostDestruction(0);
  history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
  history_service_->Cleanup();
  history_service_ = NULL;

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();
}

void TestingProfile::CreateBookmarkModel(bool delete_file) {
  // Nuke the model first, that way we're sure it's done writing to disk.
  bookmark_bar_model_.reset(NULL);

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kBookmarksFileName);
    file_util::Delete(path, false);
  }
  bookmark_bar_model_.reset(new BookmarkModel(this));
  if (history_service_.get()) {
    history_service_->history_backend_->bookmark_service_ =
        bookmark_bar_model_.get();
    history_service_->history_backend_->expirer_.bookmark_service_ =
        bookmark_bar_model_.get();
  }
  bookmark_bar_model_->Load();
}

void TestingProfile::CreateAutocompleteClassifier() {
  autocomplete_classifier_.reset(new AutocompleteClassifier(this));
}

void TestingProfile::CreateWebDataService(bool delete_file) {
  if (web_data_service_.get())
    web_data_service_->Shutdown();

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kWebDataFilename);
    file_util::Delete(path, false);
  }

  web_data_service_ = new WebDataService;
  if (web_data_service_.get())
    web_data_service_->Init(GetPath());
}

void TestingProfile::BlockUntilBookmarkModelLoaded() {
  DCHECK(bookmark_bar_model_.get());
  if (bookmark_bar_model_->IsLoaded())
    return;
  BookmarkLoadObserver observer;
  bookmark_bar_model_->AddObserver(&observer);
  MessageLoop::current()->Run();
  bookmark_bar_model_->RemoveObserver(&observer);
  DCHECK(bookmark_bar_model_->IsLoaded());
}

void TestingProfile::CreateTemplateURLModel() {
  template_url_model_.reset(new TemplateURLModel(this));
}

void TestingProfile::UseThemeProvider(BrowserThemeProvider* theme_provider) {
  theme_provider->Init(this);
  created_theme_provider_ = true;
  theme_provider_.reset(theme_provider);
}

webkit_database::DatabaseTracker* TestingProfile::GetDatabaseTracker() {
  if (!db_tracker_)
    db_tracker_ = new webkit_database::DatabaseTracker(GetPath());
  return db_tracker_;
}

void TestingProfile::InitThemes() {
  if (!created_theme_provider_) {
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
    theme_provider_.reset(new GtkThemeProvider);
#else
    theme_provider_.reset(new BrowserThemeProvider);
#endif
    theme_provider_->Init(this);
    created_theme_provider_ = true;
  }
}

URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return request_context_.get();
}

void TestingProfile::CreateRequestContext() {
  if (!request_context_)
    request_context_ = new TestURLRequestContextGetter();
}

URLRequestContextGetter* TestingProfile::GetRequestContextForExtensions() {
  if (!extensions_request_context_)
      extensions_request_context_ = new TestExtensionURLRequestContextGetter();
  return extensions_request_context_.get();
}

void TestingProfile::set_session_service(SessionService* session_service) {
  session_service_ = session_service;
}

NTPResourceCache* TestingProfile::GetNTPResourceCache() {
  if (!ntp_resource_cache_.get())
    ntp_resource_cache_.reset(new NTPResourceCache(this));
  return ntp_resource_cache_.get();
}

void TestingProfile::BlockUntilHistoryProcessesPendingRequests() {
  DCHECK(history_service_.get());
  DCHECK(MessageLoop::current());

  CancelableRequestConsumer consumer;
  history_service_->ScheduleDBTask(new QuittingHistoryDBTask(), &consumer);
  MessageLoop::current()->Run();
}

ProfileSyncService* TestingProfile::GetProfileSyncService() {
  if (!profile_sync_service_.get()) {
    // Use a NiceMock here since we are really using the mock as a
    // fake.  Test cases that want to set expectations on a
    // ProfileSyncService should use the ProfileMock and have this
    // method return their own mock instance.
    profile_sync_service_.reset(new NiceMock<ProfileSyncServiceMock>());
  }
  return profile_sync_service_.get();
}

void TestingProfile::DestroyWebDataService() {
  if (!web_data_service_.get())
    return;

  web_data_service_->Shutdown();
}
