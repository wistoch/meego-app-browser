// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "webkit/glue/form_field_values.h"

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      infobar_(NULL) {
  personal_data_ = tab_contents_->profile()->GetPersonalDataManager();
}

AutoFillManager::~AutoFillManager() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

// static
void AutoFillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAutoFillInfoBarShown, false);
  prefs->RegisterBooleanPref(prefs::kAutoFillEnabled, false);
}

void AutoFillManager::FormFieldValuesSubmitted(
    const webkit_glue::FormFieldValues& form) {
  // TODO(jhawkins): Remove this switch when AutoFill++ is fully implemented.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNewAutoFill))
    return;

  // Grab a copy of the form data.
  upload_form_structure_.reset(new FormStructure(form));

  if (!upload_form_structure_->IsAutoFillable())
    return;

  // Determine the possible field types.
  DeterminePossibleFieldTypes(upload_form_structure_.get());

  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  bool autofill_enabled = prefs->GetBoolean(prefs::kAutoFillEnabled);
  bool infobar_shown = prefs->GetBoolean(prefs::kAutoFillInfoBarShown);
  if (!infobar_shown && personal_data_) {
    // Ask the user for permission to save form information.
    infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
  } else if (autofill_enabled) {
    HandleSubmit();
  }
}

void AutoFillManager::FormsSeen(
    const std::vector<webkit_glue::FormFieldValues>& forms) {
  form_structures_.reset();
  for (std::vector<webkit_glue::FormFieldValues>::const_iterator iter =
           forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form_structure = new FormStructure(*iter);
    DeterminePossibleFieldTypes(form_structure);
    form_structures_.push_back(form_structure);
  }
}

void AutoFillManager::OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
  // Save the personal data.
  personal_data_->SetProfiles(profiles);
  personal_data_->SetCreditCards(credit_cards);

  HandleSubmit();
}

void AutoFillManager::OnPersonalDataLoaded() {
  // We might have been alerted that the PersonalDataManager has loaded, so
  // remove ourselves as observer.
  personal_data_->RemoveObserver(this);

  ShowAutoFillDialog(
      this, personal_data_->profiles(), personal_data_->credit_cards());
}

void AutoFillManager::DeterminePossibleFieldTypes(
    FormStructure* form_structure) {
  // TODO(jhawkins): Update field text.

  form_structure->GetHeuristicAutoFillTypes();

  // OTR: We can't use the PersonalDataManager to help determine field types.
  if (!personal_data_)
    return;

  for (size_t i = 0; i < form_structure->field_count(); i++) {
    const AutoFillField* field = form_structure->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value(), &field_types);
    form_structure->set_possible_types(i, field_types);
  }
}

void AutoFillManager::HandleSubmit() {
  // If there wasn't enough data to import then we don't want to send an upload
  // to the server.
  if (personal_data_ &&
      !personal_data_->ImportFormData(form_structures_.get(), this))
    return;

  UploadFormData();
}

void AutoFillManager::OnInfoBarAccepted() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // If the personal data manager has not loaded the data yet, set ourselves as
  // its observer so that we can listen for the OnPersonalDataLoaded signal.
  if (!personal_data_->IsDataLoaded())
    personal_data_->SetObserver(this);
  else
    OnPersonalDataLoaded();
}

void AutoFillManager::SaveFormData() {
  // TODO(jhawkins): Save the form data to the web database.
}

void AutoFillManager::UploadFormData() {
  std::string xml;
  bool ok = upload_form_structure_->EncodeUploadRequest(false, &xml);
  DCHECK(ok);

  // TODO(jhawkins): Initiate the upload request thread.
}

void AutoFillManager::Reset() {
  upload_form_structure_.reset();
}
