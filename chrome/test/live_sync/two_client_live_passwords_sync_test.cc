// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerifierPasswordStore(), form, expected);
  EXPECT_EQ(1U, expected.size());

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  EXPECT_TRUE(ContainsSamePasswordForms(expected, actual_zero));

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  EXPECT_TRUE(ContainsSamePasswordForms(expected, actual_one));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");

  PasswordForm form_zero;
  form_zero.origin = GURL("http://www.google.com/");
  form_zero.username_value = ASCIIToUTF16("username");
  form_zero.password_value = ASCIIToUTF16("zero");
  AddLogin(GetPasswordStore(0), form_zero);

  PasswordForm form_one;
  form_one.origin = GURL("http://www.google.com/");
  form_one.username_value = ASCIIToUTF16("username");
  form_one.password_value = ASCIIToUTF16("one");
  AddLogin(GetPasswordStore(1), form_one);

  EXPECT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  EXPECT_EQ(1U, actual_zero.size());

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  EXPECT_EQ(1U, actual_one.size());

  EXPECT_TRUE(ContainsSamePasswordForms(actual_zero, actual_one));
}

// Marked as FAILS -- see http://crbug.com/59867.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FAILS_SetPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  GetClient(0)->service()->SetPassphrase(kValidPassphrase);
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  EXPECT_TRUE(GetClient(1)->service()->observed_passphrase_required());
  GetClient(1)->service()->SetPassphrase(kValidPassphrase);
  GetClient(1)->AwaitPassphraseAccepted();
  EXPECT_FALSE(GetClient(1)->service()->observed_passphrase_required());
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       SetPassphraseAndAddPassword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  GetClient(0)->service()->SetPassphrase(kValidPassphrase);

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetPasswordStore(0), form);

  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  EXPECT_TRUE(GetClient(1)->service()->observed_passphrase_required());
  EXPECT_EQ(1, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);

  GetClient(1)->service()->SetPassphrase(kValidPassphrase);
  GetClient(1)->AwaitSyncCycleCompletion("Accept passphrase and decrypt.");
  GetClient(1)->AwaitPassphraseAccepted();
  EXPECT_FALSE(GetClient(1)->service()->observed_passphrase_required());
  EXPECT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);
}
