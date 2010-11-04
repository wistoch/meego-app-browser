// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_themes_sync_test.h"

#include "base/basictypes.h"
#include "chrome/common/extensions/extension.h"

class SingleClientLiveThemesSyncTest : public LiveThemesSyncTest {
 public:
  SingleClientLiveThemesSyncTest() : LiveThemesSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLiveThemesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

IN_PROC_BROWSER_TEST_F(SingleClientLiveThemesSyncTest, CustomTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  scoped_refptr<Extension> theme = GetTheme(0);
  ASSERT_EQ(NULL, GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(NULL, GetCustomTheme(verifier()));

  SetTheme(GetProfile(0), theme);
  SetTheme(verifier(), theme);
  ASSERT_EQ(theme, GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(theme, GetCustomTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for themes change."));

  ASSERT_EQ(theme, GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(theme, GetCustomTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(SingleClientLiveThemesSyncTest, NativeTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  scoped_refptr<Extension> theme = GetTheme(0);
  SetTheme(GetProfile(0), theme);
  SetTheme(verifier(), theme);
  ASSERT_FALSE(UsingNativeTheme(GetProfile(0)));
  ASSERT_FALSE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for themes change."));

  GetProfile(0)->SetNativeTheme();
  verifier()->SetNativeTheme();
  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for native theme change."));

  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(SingleClientLiveThemesSyncTest, DefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  scoped_refptr<Extension> theme = GetTheme(0);
  SetTheme(GetProfile(0), theme);
  SetTheme(verifier(), theme);
  ASSERT_FALSE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_FALSE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for themes change."));

  GetProfile(0)->ClearTheme();
  verifier()->ClearTheme();
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for cleared theme change."));

  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));
}
