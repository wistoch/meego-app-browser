// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PROFILE_MOCK_H__
#define CHROME_TEST_PROFILE_MOCK_H__

#include "chrome/test/testing_profile.h"

#include "testing/gmock/include/gmock/gmock.h"

class ProfileMock : public TestingProfile {
 public:
  MOCK_METHOD0(GetBookmarkModel, BookmarkModel*());
  MOCK_METHOD1(GetWebDataService, WebDataService*(ServiceAccessType access));
};

#endif  // CHROME_TEST_PROFILE_MOCK_H__
