// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
}

class ProfileSyncFactoryMock : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryMock(
      browser_sync::AssociatorInterface* bookmark_model_associator,
      browser_sync::ChangeProcessor* bookmark_change_processor);

  MOCK_METHOD0(CreateProfileSyncService,
               ProfileSyncService*(void));
  MOCK_METHOD1(CreateBookmarkComponents,
               BookmarkComponents(ProfileSyncService* profile_sync_service));

 private:
  BookmarkComponents MakeBookmarkComponents();

  scoped_ptr<browser_sync::AssociatorInterface> bookmark_model_associator_;
  scoped_ptr<browser_sync::ChangeProcessor> bookmark_change_processor_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__
