// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_

#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::SyncBackendHostMock;
using browser_sync::TestIdFactory;
using sync_api::UserShare;
using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::DirectoryManager;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::ModelType;
using syncable::MutableEntry;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_VERSION;
using syncable::SPECIFICS;
using syncable::ScopedDirLookup;
using syncable::UNIQUE_SERVER_TAG;
using syncable::UNITTEST;
using syncable::WriteTransaction;

class AbstractProfileSyncServiceTest : public testing::Test {
 protected:
  AbstractProfileSyncServiceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {}

  bool CreateRoot(ModelType model_type) {
    UserShare* user_share = service_->backend()->GetUserShareHandle();
    DirectoryManager* dir_manager = user_share->dir_manager.get();

    ScopedDirLookup dir(dir_manager, user_share->authenticated_name);
    if (!dir.good())
      return false;

    std::string tag_name;
    switch (model_type) {
      case syncable::AUTOFILL:
        tag_name = browser_sync::kAutofillTag;
        break;
      case syncable::PREFERENCES:
        tag_name = browser_sync::kPreferencesTag;
        break;
      default:
        return false;
    }

    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry node(&wtrans,
                      CREATE,
                      wtrans.root_id(),
                      tag_name);
    node.Put(UNIQUE_SERVER_TAG, tag_name);
    node.Put(IS_DIR, true);
    node.Put(SERVER_IS_DIR, false);
    node.Put(IS_UNSYNCED, false);
    node.Put(IS_UNAPPLIED_UPDATE, false);
    node.Put(SERVER_VERSION, 20);
    node.Put(BASE_VERSION, 20);
    node.Put(IS_DEL, false);
    node.Put(ID, ids_.MakeServer(tag_name));
    sync_pb::EntitySpecifics specifics;
    syncable::AddDefaultExtensionValue(model_type, &specifics);
    node.Put(SPECIFICS, specifics);

    return true;
  }

  friend class CreateRootTask;

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ProfileSyncFactoryMock factory_;
  ProfileSyncServiceObserverMock observer_;
  SyncBackendHostMock backend_;
  scoped_ptr<TestProfileSyncService> service_;
  TestIdFactory ids_;
};

class CreateRootTask : public Task {
 public:
  explicit CreateRootTask(AbstractProfileSyncServiceTest* test,
                          ModelType model_type)
      : test_(test), model_type_(model_type), success_(false) {
  }

  virtual void Run() {
    success_ = test_->CreateRoot(model_type_);
  }

  bool success() { return success_; }

 private:
  AbstractProfileSyncServiceTest* test_;
  ModelType model_type_;
  bool success_;
};

#endif  // CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
