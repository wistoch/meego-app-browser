// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Simulated delay (in milliseconds) for web data loading.
const float kWebDataLoadDelayMilliseconds = 10.0;

// Mock PersonalDataManager that gives back canned profiles and credit cards
// as well as simulating delayed loading of web data using the
// |PersonalDataManager::Observer| interface.
class PersonalDataManagerMock : public PersonalDataManager {
 public:
  PersonalDataManagerMock()
      : observer_(NULL),
        test_data_is_loaded_(true) {}
  virtual ~PersonalDataManagerMock() {}

  virtual const std::vector<AutoFillProfile*>& web_profiles() {
    return test_profiles_;
  }
  virtual const std::vector<CreditCard*>& credit_cards() {
    return test_credit_cards_;
  }
  virtual bool IsDataLoaded() const { return test_data_is_loaded_; }
  virtual void SetObserver(PersonalDataManager::Observer* observer) {
    DCHECK(observer);
    observer_ = observer;

    // This delay allows the UI loop to run and display intermediate results
    // while the data is loading.  When notified that the data is available the
    // UI updates with the new data.  10ms is a nice short amount of time to
    // let the UI thread update but does not slow down the tests too much.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        new MessageLoop::QuitTask,
        kWebDataLoadDelayMilliseconds);
    MessageLoop::current()->Run();
    observer_->OnPersonalDataLoaded();
  }
  virtual void RemoveObserver(PersonalDataManager::Observer* observer) {
    observer_ = NULL;
  }

  std::vector<AutoFillProfile*> test_profiles_;
  std::vector<CreditCard*> test_credit_cards_;
  PersonalDataManager::Observer* observer_;
  bool test_data_is_loaded_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerMock);
};

// Mock profile that gives back our own mock |PersonalDataManager|.
class ProfileMock : public TestingProfile {
 public:
  ProfileMock() {
    test_manager_.reset(new PersonalDataManagerMock);
  }
  virtual ~ProfileMock() {}

  virtual PersonalDataManager* GetPersonalDataManager() {
    return test_manager_.get();
  }

  scoped_ptr<PersonalDataManagerMock> test_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileMock);
};

// Mock browser that gives back our own |BrowserMock| instance as the profile.
class BrowserMock : public BrowserTestHelper {
 public:
  BrowserMock() {
    test_profile_.reset(new ProfileMock);
  }
  virtual ~BrowserMock() {}

  // Override of |BrowserTestHelper::profile()|.
  virtual TestingProfile* profile() const {
    return test_profile_.get();
  }

  scoped_ptr<ProfileMock> test_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserMock);
};

// Mock observer for the AutoFill settings dialog.
class AutoFillDialogObserverMock : public AutoFillDialogObserver {
 public:
  AutoFillDialogObserverMock()
    : hit_(false) {}
  virtual ~AutoFillDialogObserverMock() {}

  virtual void OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
    hit_ = true;

    std::vector<AutoFillProfile>::iterator i;
    profiles_.clear();
    for (i = profiles->begin(); i != profiles->end(); ++i)
      profiles_.push_back(*i);

    std::vector<CreditCard>::iterator j;
    credit_cards_.clear();
    for (j = credit_cards->begin(); j != credit_cards->end(); ++j)
      credit_cards_.push_back(*j);
  }

  bool hit_;
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> credit_cards_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogObserverMock);
};

// Test fixture for setting up and tearing down our dialog controller under
// test.  Also provides helper methods to access the source profiles and
// credit card information stored in mock |PersonalDataManager|.
class AutoFillDialogControllerTest : public CocoaTest {
 public:
  AutoFillDialogControllerTest()
      : controller_(nil),
        imported_profile_(NULL),
        imported_credit_card_(NULL) {
  }

  void LoadDialog() {
    controller_ = [AutoFillDialogController
        controllerWithObserver:&observer_
                       profile:helper_.profile()
               importedProfile:imported_profile_
            importedCreditCard:imported_credit_card_];
    [controller_ window];
  }

  std::vector<AutoFillProfile*>& profiles() {
    return helper_.test_profile_->test_manager_->test_profiles_;
  }
  std::vector<CreditCard*>& credit_cards() {
    return helper_.test_profile_->test_manager_->test_credit_cards_;
  }

  BrowserMock helper_;
  AutoFillDialogObserverMock observer_;
  AutoFillDialogController* controller_;  // weak reference
  AutoFillProfile* imported_profile_;  // weak reference
  CreditCard* imported_credit_card_;  // weak reference

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogControllerTest);
};

TEST_F(AutoFillDialogControllerTest, SaveButtonInformsObserver) {
  LoadDialog();
  [controller_ save:nil];
  ASSERT_TRUE(observer_.hit_);
}

TEST_F(AutoFillDialogControllerTest, CancelButtonDoesNotInformObserver) {
  LoadDialog();
  [controller_ cancel:nil];
  ASSERT_FALSE(observer_.hit_);
}

TEST_F(AutoFillDialogControllerTest, NoEditsGiveBackOriginalProfile) {
  AutoFillProfile profile;
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(observer_.profiles_.size(), profiles().size());

  // Contents should match.
  size_t i = 0;
  size_t count = profiles().size();
  for (i = 0; i < count; i++)
    ASSERT_EQ(observer_.profiles_[i], *profiles()[i]);

  // Contents should not match a different profile.
  AutoFillProfile different_profile;
  different_profile.set_label(ASCIIToUTF16("different"));
  different_profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("joe"));
  for (i = 0; i < count; i++)
    ASSERT_NE(observer_.profiles_[i], different_profile);
}

TEST_F(AutoFillDialogControllerTest, NoEditsGiveBackOriginalCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("myCC"), 345);
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(observer_.credit_cards_.size(), credit_cards().size());

  // Contents should match.  With the exception of the |unique_id|.
  size_t i = 0;
  size_t count = credit_cards().size();
  for (i = 0; i < count; i++) {
    credit_cards()[i]->set_unique_id(observer_.credit_cards_[i].unique_id());
    ASSERT_EQ(observer_.credit_cards_[i], *credit_cards()[i]);
  }

  // Contents should not match a different profile.
  CreditCard different_credit_card(ASCIIToUTF16("different"), 0);
  different_credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_NUMBER), ASCIIToUTF16("1234"));
  for (i = 0; i < count; i++)
    ASSERT_NE(observer_.credit_cards_[i], different_credit_card);
}

TEST_F(AutoFillDialogControllerTest, AutoFillDataMutation) {
  AutoFillProfile profile(ASCIIToUTF16("Home"), 17);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("David"));
  profile.SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16("C"));
  profile.SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16("Holloway"));
  profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
      ASCIIToUTF16("dhollowa@chromium.org"));
  profile.SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16("Google Inc."));
  profile.SetInfo(
      AutoFillType(ADDRESS_HOME_LINE1), ASCIIToUTF16("1122 Mountain View Road"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2), ASCIIToUTF16("Suite #1"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      ASCIIToUTF16("Mountain View"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("94111"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"));
  profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER), ASCIIToUTF16("014155552258"));
  profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER), ASCIIToUTF16("024087172258"));
  profiles().push_back(&profile);

  LoadDialog();

  AutoFillAddressModel* am = [[[controller_ addressFormViewControllers]
      objectAtIndex:0] addressModel];
  EXPECT_TRUE([[am label] isEqualToString:@"Home"]);
  EXPECT_TRUE([[am firstName] isEqualToString:@"David"]);
  EXPECT_TRUE([[am middleName] isEqualToString:@"C"]);
  EXPECT_TRUE([[am lastName] isEqualToString:@"Holloway"]);
  EXPECT_TRUE([[am email] isEqualToString:@"dhollowa@chromium.org"]);
  EXPECT_TRUE([[am companyName] isEqualToString:@"Google Inc."]);
  EXPECT_TRUE([[am addressLine1] isEqualToString:@"1122 Mountain View Road"]);
  EXPECT_TRUE([[am addressLine2] isEqualToString:@"Suite #1"]);
  EXPECT_TRUE([[am addressCity] isEqualToString:@"Mountain View"]);
  EXPECT_TRUE([[am addressState] isEqualToString:@"CA"]);
  EXPECT_TRUE([[am addressZip] isEqualToString:@"94111"]);
  EXPECT_TRUE([[am phoneWholeNumber] isEqualToString:@"014155552258"]);
  EXPECT_TRUE([[am faxWholeNumber] isEqualToString:@"024087172258"]);

  [controller_ save:nil];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.profiles_.size() == 1);

  profiles()[0]->set_unique_id(observer_.profiles_[0].unique_id());
  ASSERT_EQ(observer_.profiles_[0], *profiles()[0]);
}

TEST_F(AutoFillDialogControllerTest, CreditCardDataMutation) {
  CreditCard credit_card(ASCIIToUTF16("myCC"), 345);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("DCH"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_NUMBER), ASCIIToUTF16("1234 5678 9101 1121"));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), ASCIIToUTF16("2012"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_VERIFICATION_CODE), ASCIIToUTF16("222"));
  credit_cards().push_back(&credit_card);

  LoadDialog();

  AutoFillCreditCardModel* cm = [[[controller_ creditCardFormViewControllers]
      objectAtIndex:0] creditCardModel];
  EXPECT_TRUE([[cm label] isEqualToString:@"myCC"]);
  EXPECT_TRUE([[cm nameOnCard] isEqualToString:@"DCH"]);
  EXPECT_TRUE([[cm creditCardNumber] isEqualToString:@"1234 5678 9101 1121"]);
  EXPECT_TRUE([[cm expirationMonth] isEqualToString:@"01"]);
  EXPECT_TRUE([[cm expirationYear] isEqualToString:@"2012"]);
  EXPECT_TRUE([[cm cvcCode] isEqualToString:@"222"]);

  [controller_ save:nil];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.credit_cards_.size() == 1);

  credit_cards()[0]->set_unique_id(observer_.credit_cards_[0].unique_id());
  ASSERT_EQ(observer_.credit_cards_[0], *credit_cards()[0]);
}

TEST_F(AutoFillDialogControllerTest, TwoProfiles) {
  AutoFillProfile profile1(ASCIIToUTF16("One"), 1);
  profile1.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile1);
  AutoFillProfile profile2(ASCIIToUTF16("Two"), 2);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = profiles().size(); i < count; i++) {
    profiles()[i]->set_unique_id(observer_.profiles_[i].unique_id());
    ASSERT_EQ(observer_.profiles_[i], *profiles()[i]);
  }
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCards) {
  CreditCard credit_card1(ASCIIToUTF16("Visa"), 1);
  credit_card1.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card1);
  CreditCard credit_card2(ASCIIToUTF16("Mastercard"), 2);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = credit_cards().size(); i < count; i++) {
    credit_cards()[i]->set_unique_id(observer_.credit_cards_[i].unique_id());
    ASSERT_EQ(observer_.credit_cards_[i], *credit_cards()[i]);
  }
}

TEST_F(AutoFillDialogControllerTest, AddNewProfile) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ addNewAddress:nil];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 2.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 2UL);

  // New address should match.
  AutoFillProfile new_profile(ASCIIToUTF16("New address"), 0);
  ASSERT_EQ(observer_.profiles_[1], new_profile);
}

TEST_F(AutoFillDialogControllerTest, AddNewCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ addNewCreditCard:nil];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 2.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 2UL);

  // New address should match.
  CreditCard new_credit_card(ASCIIToUTF16("New credit card"), 0);
  ASSERT_EQ(observer_.credit_cards_[1], new_credit_card);
}

TEST_F(AutoFillDialogControllerTest, DeleteProfile) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  LoadDialog();
  EXPECT_EQ([[[controller_ addressFormViewControllers] lastObject]
              retainCount], 1UL);
  [controller_ deleteAddress:[[controller_ addressFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, DeleteCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  LoadDialog();
  EXPECT_EQ([[[controller_ creditCardFormViewControllers] lastObject]
              retainCount], 1UL);
  [controller_ deleteCreditCard:[[controller_ creditCardFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, TwoProfilesDeleteOne) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  AutoFillProfile profile2(ASCIIToUTF16("Two"), 2);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);
  LoadDialog();
  [controller_ deleteAddress:[[controller_ addressFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 1UL);

  // First address should match.
  profiles()[0]->set_unique_id(observer_.profiles_[0].unique_id());
  ASSERT_EQ(observer_.profiles_[0], profile);
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCardsDeleteOne) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  CreditCard credit_card2(ASCIIToUTF16("Mastercard"), 2);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);
  LoadDialog();
  [controller_ deleteCreditCard:[[controller_ creditCardFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 1UL);

  // First credit card should match.
  credit_cards()[0]->set_unique_id(observer_.credit_cards_[0].unique_id());
  ASSERT_EQ(observer_.credit_cards_[0], credit_card);
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesFalse) {
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_FALSE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesTrue) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, true);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesChanged) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();
  [controller_ setAuxiliaryEnabled:YES];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, DefaultsChangingLogic) {
  // Two profiles, two credit cards.
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  AutoFillProfile profile2(ASCIIToUTF16("Two"), 2);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  CreditCard credit_card2(ASCIIToUTF16("Mastercard"), 2);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);

  // Invalid defaults for each.
  helper_.profile()->GetPrefs()->SetString(
      prefs::kAutoFillDefaultProfile, L"xxxx");
  helper_.profile()->GetPrefs()->SetString(
      prefs::kAutoFillDefaultCreditCard, L"yyyy");

  // Start 'em up.
  LoadDialog();

  // With invalid default values, the first item should be the default.
  EXPECT_TRUE([[controller_ defaultAddressLabel] isEqualToString:@"One"]);
  EXPECT_TRUE([[controller_ defaultCreditCardLabel] isEqualToString:@"Visa"]);

  // Explicitly set the second to be default and make sure it sticks.
  [controller_ setDefaultAddressLabel:@"Two"];
  [controller_ setDefaultCreditCardLabel:@"Mastercard"];
  ASSERT_TRUE([[controller_ defaultAddressLabel] isEqualToString:@"Two"]);
  ASSERT_TRUE([[controller_ defaultCreditCardLabel]
      isEqualToString:@"Mastercard"]);

  // Deselect the second and the first should become default.
  [controller_ setDefaultAddressLabel:nil];
  [controller_ setDefaultCreditCardLabel:nil];
  ASSERT_TRUE([[controller_ defaultAddressLabel] isEqualToString:@"One"]);
  ASSERT_TRUE([[controller_ defaultCreditCardLabel] isEqualToString:@"Visa"]);

  // Deselect the first and the second should be come default.
  [controller_ setDefaultAddressLabel:nil];
  [controller_ setDefaultCreditCardLabel:nil];
  ASSERT_TRUE([[controller_ defaultAddressLabel] isEqualToString:@"Two"]);
  ASSERT_TRUE([[controller_ defaultCreditCardLabel]
      isEqualToString:@"Mastercard"]);

  // Delete the second and the first should end up as default.
  [controller_ deleteAddress:[[controller_ addressFormViewControllers]
      lastObject]];
  [controller_ deleteCreditCard:[[controller_ creditCardFormViewControllers]
      lastObject]];
  ASSERT_TRUE([[controller_ defaultAddressLabel] isEqualToString:@"One"]);
  ASSERT_TRUE([[controller_ defaultCreditCardLabel] isEqualToString:@"Visa"]);

  // Save and that should end up in the prefs.
  [controller_ save:nil];
  ASSERT_TRUE(observer_.hit_);

  ASSERT_EQ(L"One", helper_.profile()->GetPrefs()->
      GetString(prefs::kAutoFillDefaultProfile));
  ASSERT_EQ(L"Visa", helper_.profile()->GetPrefs()->
      GetString(prefs::kAutoFillDefaultCreditCard));
}

TEST_F(AutoFillDialogControllerTest, WaitForDataToLoad) {
  AutoFillProfile profile(ASCIIToUTF16("Home"), 0);
  profiles().push_back(&profile);
  CreditCard credit_card(ASCIIToUTF16("Visa"), 0);
  credit_cards().push_back(&credit_card);
  helper_.test_profile_->test_manager_->test_data_is_loaded_ = false;
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.credit_cards_.size(), credit_cards().size());

  // Contents should match.
  size_t i = 0;
  size_t count = profiles().size();
  for (i = 0; i < count; i++)
    ASSERT_EQ(observer_.profiles_[i], *profiles()[i]);
  count = credit_cards().size();
  for (i = 0; i < count; i++) {
    ASSERT_EQ(observer_.credit_cards_[i], *credit_cards()[i]);
  }
}

TEST_F(AutoFillDialogControllerTest, ImportedParameters) {
  AutoFillProfile profile(ASCIIToUTF16("Home"), 0);
  imported_profile_ = &profile;
  CreditCard credit_card(ASCIIToUTF16("Mastercard"), 0);
  imported_credit_card_ = &credit_card;

  // Note: when the |imported_*| parameters are supplied the dialog should
  // ignore any profile and credit card information in the
  // |PersonalDataManager|.
  AutoFillProfile profile_ignored(ASCIIToUTF16("Work"), 0);
  profiles().push_back(&profile_ignored);
  CreditCard credit_card_ignored(ASCIIToUTF16("Visa"), 0);
  credit_cards().push_back(&credit_card_ignored);

  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(1UL, observer_.profiles_.size());
  ASSERT_EQ(1UL, observer_.credit_cards_.size());

  // Contents should match.
  ASSERT_EQ(observer_.profiles_[0], profile);
  ASSERT_EQ(observer_.credit_cards_[0], credit_card);
}

}  // namespace
