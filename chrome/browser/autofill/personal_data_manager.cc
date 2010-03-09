// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"

// The minimum number of fields that must contain user data and have known types
// before autofill will attempt to import the data into a profile.
static const int kMinImportSize = 5;

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  // Error from the web database.
  if (!result)
    return;

  DCHECK(pending_profiles_query_ || pending_creditcards_query_);
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT ||
         result->GetType() == AUTOFILL_CREDITCARDS_RESULT);

  switch (result->GetType()) {
    case AUTOFILL_PROFILES_RESULT:
      ReceiveLoadedProfiles(h, result);
      break;
    case AUTOFILL_CREDITCARDS_RESULT:
      ReceiveLoadedCreditCards(h, result);
      break;
    default:
      NOTREACHED();
  }

  // If both requests have responded, then all personal data is loaded.
  if (pending_profiles_query_ == 0 && pending_creditcards_query_ == 0) {
    is_data_loaded_ = true;
    // Copy is needed as observer can unsubscribe itself when notified.
    std::vector<PersonalDataManager::Observer*> temporary_copy;
    temporary_copy.resize(observers_.size());
    std::copy(observers_.begin(), observers_.end(), temporary_copy.begin());
    for (std::vector<PersonalDataManager::Observer*>::iterator
         iter = temporary_copy.begin();
         iter != temporary_copy.end();
         ++iter) {
      (*iter)->OnPersonalDataLoaded();
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager,
// views::ButtonListener implementations
void PersonalDataManager::OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
  // |profiles| may be NULL
  // |credit_cards| may be NULL
  if (profiles)
    SetProfiles(profiles);
  if (credit_cards)
    SetCreditCards(credit_cards);
}

void PersonalDataManager::SetObserver(PersonalDataManager::Observer* observer) {
  for (std::vector<PersonalDataManager::Observer*>::iterator
       iter = observers_.begin();
       iter != observers_.end();
       ++iter) {
    if (*iter == observer) {
      // Already have this observer.
      return;
    }
  }
  observers_.push_back(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManager::Observer* observer) {
  for (std::vector<PersonalDataManager::Observer*>::iterator
       iter = observers_.begin();
       iter != observers_.end();
       ++iter) {
    if (*iter == observer) {
      observers_.erase(iter);
      return;
    }
  }
}

bool PersonalDataManager::ImportFormData(
    const std::vector<FormStructure*>& form_structures,
    AutoFillManager* autofill_manager) {
  InitializeIfNeeded();

  // Parse the form and construct a profile based on the information that is
  // possible to import.
  int importable_fields = 0;
  int importable_credit_card_fields = 0;
  imported_profile_.reset(new AutoFillProfile(
      string16(), CreateNextUniqueID(&unique_profile_ids_)));
  // TODO(jhawkins): Use a hash of the CC# instead of a list of unique IDs?
  imported_credit_card_.reset(new CreditCard(string16(), 0));

  bool billing_address_info = false;
  std::vector<FormStructure*>::const_iterator iter;
  for (iter = form_structures.begin(); iter != form_structures.end(); ++iter) {
    const FormStructure* form = *iter;
    for (size_t i = 0; i < form->field_count(); ++i) {
      const AutoFillField* field = form->field(i);
      string16 value = CollapseWhitespace(field->value(), false);

      // If we don't know the type of the field, or the user hasn't entered any
      // information into the field, then skip it.
      if (!field->IsFieldFillable() || value.empty())
        continue;

      AutoFillType field_type(field->type());
      FieldTypeGroup group(field_type.group());

      if (group == AutoFillType::CREDIT_CARD) {
        // If the user has a password set, we have no way of setting credit
        // card numbers.
        if (!HasPassword()) {
          imported_credit_card_->SetInfo(AutoFillType(field_type.field_type()),
                                         value);
          ++importable_credit_card_fields;
        }
      } else {
        // In the case of a phone number, if the whole phone number was entered
        // into a single field, then parse it and set the sub components.
        if (field_type.subgroup() == AutoFillType::PHONE_WHOLE_NUMBER) {
          string16 number;
          string16 city_code;
          string16 country_code;
          if (group == AutoFillType::PHONE_HOME) {
            PhoneNumber::ParsePhoneNumber(
                value, &number, &city_code, &country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_COUNTRY_CODE), country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_CITY_CODE), city_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_NUMBER), number);
          } else if (group == AutoFillType::PHONE_FAX) {
            PhoneNumber::ParsePhoneNumber(
                value, &number, &city_code, &country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_COUNTRY_CODE), country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_CITY_CODE), city_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_NUMBER), number);
          }
          continue;
        }

        imported_profile_->SetInfo(AutoFillType(field_type.field_type()),
                                   value);

        // If we found any billing address information, then set the profile to
        // use a separate billing address.
        if (group == AutoFillType::ADDRESS_BILLING)
          billing_address_info = true;

        ++importable_fields;
      }
    }
  }

  // If the user did not enter enough information on the page then don't bother
  // importing the data.
  if (importable_fields + importable_credit_card_fields < kMinImportSize)
    return false;

  if (!billing_address_info)
    imported_profile_->set_use_billing_address(false);

  if (importable_fields == 0)
    imported_profile_.reset();

  if (importable_credit_card_fields == 0)
    imported_credit_card_.reset();

  return true;
}

void PersonalDataManager::SetProfiles(std::vector<AutoFillProfile>* profiles) {
  if (profile_->IsOffTheRecord())
    return;

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  // Remove the unique IDs of the new set of profiles from the unique ID set.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (iter->unique_id() != 0)
      unique_profile_ids_.erase(iter->unique_id());
  }

  // Any remaining IDs are not in the new profile list and should be removed
  // from the web database.
  for (std::set<int>::iterator iter = unique_profile_ids_.begin();
       iter != unique_profile_ids_.end(); ++iter) {
    wds->RemoveAutoFillProfile(*iter);
  }

  // Clear the unique IDs.  The set of unique IDs is updated for each profile
  // added to |web_profiles_| below.
  unique_profile_ids_.clear();

  // Update the web database with the existing profiles.  We need to handle
  // these first so that |unique_profile_ids_| is reset with the IDs of the
  // existing profiles; otherwise, new profiles added before older profiles can
  // take their unique ID.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (iter->unique_id() != 0) {
      unique_profile_ids_.insert(iter->unique_id());
      wds->UpdateAutoFillProfile(*iter);
    }
  }

  // Add the new profiles to the web database.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    // The profile was added by the AutoFill dialog, so we need to set the
    // unique ID.  This also means we need to add this profile to the web
    // database.
    if (iter->unique_id() == 0) {
      iter->set_unique_id(CreateNextUniqueID(&unique_profile_ids_));
      wds->AddAutoFillProfile(*iter);
    }
  }

  web_profiles_.reset();
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    web_profiles_.push_back(new AutoFillProfile(*iter));
  }
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (profile_->IsOffTheRecord())
    return;

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  // Remove the unique IDs of the new set of credit cards from the unique ID
  // set.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (iter->unique_id() != 0)
      unique_creditcard_ids_.erase(iter->unique_id());
  }

  // Any remaining IDs are not in the new credit card list and should be removed
  // from the web database.
  for (std::set<int>::iterator iter = unique_creditcard_ids_.begin();
       iter != unique_creditcard_ids_.end(); ++iter) {
    wds->RemoveCreditCard(*iter);
  }

  // Clear the unique IDs.  The set of unique IDs is updated for each credit
  // card added to |credit_cards_| below.
  unique_creditcard_ids_.clear();

  // Update the web database with the existing credit cards.  We need to handle
  // these first so that |unique_creditcard_ids_| is reset with the IDs of the
  // existing credit cards; otherwise, new credit cards added before older
  // credit cards can take their unique ID.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (iter->unique_id() != 0) {
      unique_creditcard_ids_.insert(iter->unique_id());
      wds->UpdateCreditCard(*iter);
    }
  }

  // Add the new credit cards to the web database.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    // The credit card was added by the AutoFill dialog, so we need to set the
    // unique ID.  This also means we need to add this credit card to the web
    // database.
    if (iter->unique_id() == 0) {
      iter->set_unique_id(CreateNextUniqueID(&unique_creditcard_ids_));
      wds->AddCreditCard(*iter);
    }
  }

  credit_cards_.reset();
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    credit_cards_.push_back(new CreditCard(*iter));
  }
}

void PersonalDataManager::GetPossibleFieldTypes(const string16& text,
                                                FieldTypeSet* possible_types) {
  InitializeIfNeeded();

  string16 clean_info = StringToLowerASCII(CollapseWhitespace(text, false));

  if (clean_info.empty()) {
    possible_types->insert(EMPTY_TYPE);
    return;
  }

  for (ScopedVector<AutoFillProfile>::iterator iter = web_profiles_.begin();
       iter != web_profiles_.end(); ++iter) {
    const FormGroup* profile = *iter;
    if (!profile) {
      DLOG(ERROR) << "NULL information in profiles list";
      continue;
    }
    profile->GetPossibleFieldTypes(clean_info, possible_types);
  }

  for (ScopedVector<CreditCard>::iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    const FormGroup* credit_card = *iter;
    if (!credit_card) {
      DLOG(ERROR) << "NULL information in credit cards list";
      continue;
    }
    credit_card->GetPossibleFieldTypes(clean_info, possible_types);
  }

  if (possible_types->size() == 0)
    possible_types->insert(UNKNOWN_TYPE);
}

bool PersonalDataManager::HasPassword() {
  InitializeIfNeeded();
  return !password_hash_.empty();
}

const std::vector<AutoFillProfile*>& PersonalDataManager::profiles() {
  bool auxiliary_profiles_enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled);
  if (auxiliary_profiles_enabled) {
    profiles_.clear();

    // Populates |auxiliary_profiles_|.
    LoadAuxiliaryProfiles();

    profiles_.insert(profiles_.end(),
        web_profiles_.begin(), web_profiles_.end());
    profiles_.insert(profiles_.end(),
        auxiliary_profiles_.begin(), auxiliary_profiles_.end());
    return profiles_;
  } else {
    return web_profiles_.get();
  }
}

const std::vector<AutoFillProfile*>& PersonalDataManager::web_profiles() {
  return web_profiles_.get();
}

PersonalDataManager::PersonalDataManager(Profile* profile)
    : profile_(profile),
      is_initialized_(false),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_creditcards_query_(0) {
  LoadProfiles();
  LoadCreditCards();
}

void PersonalDataManager::InitializeIfNeeded() {
  if (is_initialized_)
    return;

  is_initialized_ = true;
  // TODO(jhawkins): Load data.
}

int PersonalDataManager::CreateNextUniqueID(std::set<int>* unique_ids) {
  // Profile IDs MUST start at 1 to allow 0 as an error value when reading
  // the ID from the WebDB (see LoadData()).
  int id = 1;
  while (unique_ids->count(id) != 0)
    ++id;
  unique_ids->insert(id);
  return id;
}

void PersonalDataManager::LoadProfiles() {
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);

  pending_profiles_query_ = web_data_service->GetAutoFillProfiles(this);
}

// Win and Linux implementations do nothing.  Mac implementation fills in the
// contents of |auxiliary_profiles_|.
#if !defined(OS_MACOSX)
void PersonalDataManager::LoadAuxiliaryProfiles() {
}
#endif

void PersonalDataManager::LoadCreditCards() {
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);

  pending_creditcards_query_ = web_data_service->GetCreditCards(this);
}

void PersonalDataManager::ReceiveLoadedProfiles(WebDataService::Handle h,
                                                const WDTypedResult* result) {
  DCHECK_EQ(pending_profiles_query_, h);
  pending_profiles_query_ = 0;

  unique_profile_ids_.clear();
  web_profiles_.reset();

  const WDResult<std::vector<AutoFillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutoFillProfile*> >*>(result);

  std::vector<AutoFillProfile*> profiles = r->GetValue();
  for (std::vector<AutoFillProfile*>::iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    unique_profile_ids_.insert((*iter)->unique_id());
    web_profiles_.push_back(*iter);
  }
}

void PersonalDataManager::ReceiveLoadedCreditCards(
    WebDataService::Handle h, const WDTypedResult* result) {
  DCHECK_EQ(pending_creditcards_query_, h);
  pending_creditcards_query_ = 0;

  unique_creditcard_ids_.clear();
  credit_cards_.reset();

  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  for (std::vector<CreditCard*>::iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    unique_creditcard_ids_.insert((*iter)->unique_id());
    credit_cards_.push_back(*iter);
  }
}

void PersonalDataManager::CancelPendingQuery(WebDataService::Handle* handle) {
  if (*handle) {
    WebDataService* web_data_service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(*handle);
  }
  *handle = 0;
}
