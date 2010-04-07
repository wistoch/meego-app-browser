// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

class PersonalDataLoadedObserverMock : public PersonalDataManager::Observer {
 public:
  PersonalDataLoadedObserverMock() {}
  virtual ~PersonalDataLoadedObserverMock() {}

  MOCK_METHOD0(OnPersonalDataLoaded, void());
};

class PersonalDataManagerTest : public testing::Test {
 protected:
  PersonalDataManagerTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
  }

  virtual void SetUp() {
    db_thread_.Start();

    profile_.reset(new TestingProfile);
    profile_->CreateWebDataService(false);

    ResetPersonalDataManager();
  }

  virtual void TearDown() {
    if (profile_.get())
      profile_.reset(NULL);

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  void ResetPersonalDataManager() {
    personal_data_.reset(new PersonalDataManager());
    personal_data_->Init(profile_.get());
    personal_data_->SetObserver(&personal_data_observer_);

    // Disable auxiliary profiles for unit testing.  These reach out to system
    // services on the Mac.
    profile_->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  }

  AutoFillProfile* MakeProfile() {
    AutoLock lock(personal_data_->unique_ids_lock_);
    return new AutoFillProfile(string16(),
        personal_data_->CreateNextUniqueID(
        &personal_data_->unique_profile_ids_));
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<PersonalDataManager> personal_data_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
  PersonalDataLoadedObserverMock personal_data_observer_;
};

// TODO(jhawkins): Test SetProfiles w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetProfiles) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  AutoFillProfile profile2(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile2,
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the three test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // profiles, so we have to update the expectations.
  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  // Three operations in one:
  //  - Update profile0
  //  - Remove profile1
  //  - Add profile2
  profile0.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile2);
  personal_data_->SetProfiles(&update);

  // Set the expected unique ID for profile2.
  profile2.set_unique_id(update[1].unique_id());

  // AutoFillProfile IDs are re-used, so the third profile to be added will have
  // a unique ID that matches the old unique ID of the removed profile1, even
  // though that ID has already been used.
  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile2, *results2.at(1));
  EXPECT_EQ(profile2.unique_id(), profile1.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(profile0, *results3.at(0));
  EXPECT_EQ(profile2, *results3.at(1));
}

// TODO(jhawkins): Test SetCreditCards w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetCreditCards) {
  CreditCard creditcard0(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard0,
      "Corporate", "John Dillinger", "Visa",
      "123456789012", "01", "2010", "123", "Chicago", "Indianapolis");

  CreditCard creditcard1(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard1,
      "Personal", "Bonnie Parker", "Mastercard",
      "098765432109", "12", "2012", "987", "Dallas", "");

  CreditCard creditcard2(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard2,
      "Savings", "Clyde Barrow", "American Express",
      "777666888555", "04", "2015", "445", "Home", "Farm");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the three test credit cards to the database.
  std::vector<CreditCard> update;
  update.push_back(creditcard0);
  update.push_back(creditcard1);
  personal_data_->SetCreditCards(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // credit cards, so we have to update the expectations.
  creditcard0.set_unique_id(update[0].unique_id());
  creditcard1.set_unique_id(update[1].unique_id());

  const std::vector<CreditCard*>& results1 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(creditcard0, *results1.at(0));
  EXPECT_EQ(creditcard1, *results1.at(1));

  // Three operations in one:
  //  - Update creditcard0
  //  - Remove creditcard1
  //  - Add creditcard2
  creditcard0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  update.clear();
  update.push_back(creditcard0);
  update.push_back(creditcard2);
  personal_data_->SetCreditCards(&update);

  // Set the expected unique ID for creditcard2.
  creditcard2.set_unique_id(update[1].unique_id());

  // CreditCard IDs are re-used, so the third credit card to be added will have
  // a unique ID that matches the old unique ID of the removed creditcard1, even
  // though that ID has already been used.
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard2, *results2.at(1));
  EXPECT_EQ(creditcard2.unique_id(), creditcard1.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results3 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(creditcard0, *results3.at(0));
  EXPECT_EQ(creditcard2, *results3.at(1));
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  // Add the test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  scoped_ptr<AutoFillProfile> profile2(MakeProfile());
  autofill_unittest::SetProfileInfo(profile2.get(),
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(wds);
  wds->AddAutoFillProfile(*profile2.get());

  personal_data_->Refresh();

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
    OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(3U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile1, *results2.at(1));
  EXPECT_EQ(*profile2.get(), *results2.at(2));

  wds->RemoveAutoFillProfile(profile1.unique_id());
  wds->RemoveAutoFillProfile(profile2->unique_id());

  // Before telling the PDM to refresh, simulate an edit to one of the profiles
  // via a SetProfile update (this would happen if the autofill window was
  // open with a previous snapshot of the profiles, and something [e.g. sync]
  // removed a profile from the browser.  In this edge case, we will end up
  // in a consistent state by dropping the write).
  profile2->SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Jo"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile1);
  update.push_back(*profile2.get());
  personal_data_->SetProfiles(&update);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(profile0, *results2.at(0));
}

TEST_F(PersonalDataManagerTest, DefaultProfiles) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  AutoFillProfile profile2(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile2,
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Check that with no profiles our default profile index is -1.
  EXPECT_EQ(-1, personal_data_->DefaultProfile());

  // Add the three test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // profiles, so we have to update the expectations.
  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  // Three operations in one:
  //  - Update profile0
  //  - Remove profile1
  //  - Add profile2
  profile0.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile2);
  personal_data_->SetProfiles(&update);

  // Set the expected unique ID for profile2.
  profile2.set_unique_id(update[1].unique_id());

  // AutoFillProfile IDs are re-used, so the third profile to be added will have
  // a unique ID that matches the old unique ID of the removed profile1, even
  // though that ID has already been used.
  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile2, *results2.at(1));
  EXPECT_EQ(profile2.unique_id(), profile1.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(profile0, *results3.at(0));
  EXPECT_EQ(profile2, *results3.at(1));

  // Check that with profiles our default profile index is 0 prior to explicitly
  // setting a default.
  EXPECT_EQ(0, personal_data_->DefaultProfile());

  profile_->GetPrefs()->SetString(prefs::kAutoFillDefaultProfile,
                                  UTF8ToWide("Work"));

  // Check that with profiles our default profile index is 1 after setting
  // to label of profile1.
  EXPECT_EQ(1, personal_data_->DefaultProfile());
}

TEST_F(PersonalDataManagerTest, DefaultCreditCards) {
  CreditCard creditcard0(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard0,
      "Corporate", "John Dillinger", "Visa",
      "123456789012", "01", "2010", "123", "Chicago", "Indianapolis");

  CreditCard creditcard1(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard1,
      "Personal", "Bonnie Parker", "Mastercard",
      "098765432109", "12", "2012", "987", "Dallas", "");

  CreditCard creditcard2(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard2,
      "Savings", "Clyde Barrow", "American Express",
      "777666888555", "04", "2015", "445", "Home", "Farm");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Check that with no credit cards our default credit card index is -1.
  EXPECT_EQ(-1, personal_data_->DefaultCreditCard());

  // Add the three test credit cards to the database.
  std::vector<CreditCard> update;
  update.push_back(creditcard0);
  update.push_back(creditcard1);
  personal_data_->SetCreditCards(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // credit cards, so we have to update the expectations.
  creditcard0.set_unique_id(update[0].unique_id());
  creditcard1.set_unique_id(update[1].unique_id());

  const std::vector<CreditCard*>& results1 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(creditcard0, *results1.at(0));
  EXPECT_EQ(creditcard1, *results1.at(1));

  // Three operations in one:
  //  - Update creditcard0
  //  - Remove creditcard1
  //  - Add creditcard2
  creditcard0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  update.clear();
  update.push_back(creditcard0);
  update.push_back(creditcard2);
  personal_data_->SetCreditCards(&update);

  // Set the expected unique ID for creditcard2.
  creditcard2.set_unique_id(update[1].unique_id());

  // CreditCard IDs are re-used, so the third credit card to be added will have
  // a unique ID that matches the old unique ID of the removed creditcard1, even
  // though that ID has already been used.
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard2, *results2.at(1));
  EXPECT_EQ(creditcard2.unique_id(), creditcard1.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results3 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(creditcard0, *results3.at(0));
  EXPECT_EQ(creditcard2, *results3.at(1));

  // Check that with credit cards our default credit card index is 0 prior to
  // explicitly setting a default.
  EXPECT_EQ(0, personal_data_->DefaultCreditCard());

  profile_->GetPrefs()->SetString(prefs::kAutoFillDefaultCreditCard,
                                  UTF8ToWide("Savings"));

  // Check that with credit cards our default credit card index is 1 after
  // setting to label of creditcard1.
  EXPECT_EQ(1, personal_data_->DefaultCreditCard());
}
