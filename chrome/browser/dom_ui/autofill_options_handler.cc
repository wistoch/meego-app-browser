// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_options_handler.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

AutoFillOptionsHandler::AutoFillOptionsHandler()
    : personal_data_(NULL) {
}

AutoFillOptionsHandler::~AutoFillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsUIHandler implementation:
void AutoFillOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("autoFillOptionsTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_TITLE));
  localized_strings->SetString("autoFillEnabled",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOFILL_ENABLE));
  localized_strings->SetString("addressesHeader",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESSES_GROUP_NAME));
  localized_strings->SetString("creditCardsHeader",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME));
  localized_strings->SetString("addAddressButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_ADDRESS_BUTTON));
  localized_strings->SetString("addCreditCardButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));
  localized_strings->SetString("editButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_BUTTON));
  localized_strings->SetString("deleteButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_BUTTON));
  localized_strings->SetString("helpButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_HELP_LABEL));
  localized_strings->SetString("addAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_ADDRESS_CAPTION));
  localized_strings->SetString("addCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_CREDITCARD_CAPTION));

  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);
}

void AutoFillOptionsHandler::Initialize() {
  personal_data_ =
      dom_ui_->GetProfile()->GetOriginalProfile()->GetPersonalDataManager();
  personal_data_->SetObserver(this);

  LoadAutoFillData();
}

void AutoFillOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "addAddress",
      NewCallback(this, &AutoFillOptionsHandler::AddAddress));

  dom_ui_->RegisterMessageCallback(
      "removeAddress",
      NewCallback(this, &AutoFillOptionsHandler::RemoveAddress));

  dom_ui_->RegisterMessageCallback(
      "removeCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::RemoveCreditCard));
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutoFillOptionsHandler::OnPersonalDataLoaded() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::OnPersonalDataChanged() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::SetAddressOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autoFillEditAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("fullNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FULL_NAME));
  localized_strings->SetString("companyNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COMPANY_NAME));
  localized_strings->SetString("addrLine1Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1));
  localized_strings->SetString("addrLine2Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2));
  localized_strings->SetString("cityLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CITY));
  localized_strings->SetString("stateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_STATE));
  localized_strings->SetString("zipCodeLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ZIP_CODE));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("phoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PHONE));
  localized_strings->SetString("faxLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FAX));
  localized_strings->SetString("emailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EMAIL));
  localized_strings->SetString("autoFillEditAddressApplyButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("autoFillEditAddressCancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
}

void AutoFillOptionsHandler::SetCreditCardOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autoFillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  localized_strings->SetString("billingAddressLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_BILLING_ADDRESS));
  localized_strings->SetString("chooseExistingAddress",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE));
}

void AutoFillOptionsHandler::LoadAutoFillData() {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue addresses;
  for (std::vector<AutoFillProfile*>::const_iterator i =
           personal_data_->profiles().begin();
       i != personal_data_->profiles().end(); ++i) {
    DictionaryValue* address = new DictionaryValue();
    address->SetString("label", (*i)->PreviewSummary());
    address->SetInteger("unique_id", (*i)->unique_id());
    addresses.Append(address);
  }

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.updateAddresses",
                                  addresses);

  ListValue credit_cards;
  for (std::vector<CreditCard*>::const_iterator i =
           personal_data_->credit_cards().begin();
       i != personal_data_->credit_cards().end(); ++i) {
    DictionaryValue* credit_card = new DictionaryValue();
    credit_card->SetString("label", (*i)->PreviewSummary());
    credit_card->SetInteger("unique_id", (*i)->unique_id());
    credit_cards.Append(credit_card);
  }

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.updateCreditCards",
                                  credit_cards);
}

void AutoFillOptionsHandler::AddAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  AutoFillProfile profile;
  string16 value;
  if (args->GetString(0, &value))
    profile.SetInfo(AutoFillType(NAME_FULL), value);
  if (args->GetString(1, &value))
    profile.SetInfo(AutoFillType(COMPANY_NAME), value);
  if (args->GetString(2, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1), value);
  if (args->GetString(3, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2), value);
  if (args->GetString(4, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY), value);
  if (args->GetString(5, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), value);
  if (args->GetString(6, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), value);
  if (args->GetString(7, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), value);
  if (args->GetString(8, &value))
    profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER), value);
  if (args->GetString(9, &value))
    profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER), value);
  if (args->GetString(10, &value))
    profile.SetInfo(AutoFillType(EMAIL_ADDRESS), value);

  personal_data_->AddProfile(profile);
}

void AutoFillOptionsHandler::RemoveAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  int unique_id = 0;
  if (!ExtractIntegerValue(args, &unique_id)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveProfile(unique_id);
}

void AutoFillOptionsHandler::RemoveCreditCard(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  int unique_id = 0;
  if (!ExtractIntegerValue(args, &unique_id)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveCreditCard(unique_id);
}
