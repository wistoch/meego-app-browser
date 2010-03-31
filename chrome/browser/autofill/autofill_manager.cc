// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_values.h"

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      infobar_(NULL) {
  DCHECK(tab_contents);

  personal_data_ =
      tab_contents_->profile()->GetOriginalProfile()->GetPersonalDataManager();
  DCHECK(personal_data_);
  download_manager_.SetObserver(this);
}

AutoFillManager::~AutoFillManager() {
  // This is NULL in the MockAutoFillManager.
  if (personal_data_)
    personal_data_->RemoveObserver(this);
  download_manager_.SetObserver(NULL);
}

// static
void AutoFillManager::RegisterBrowserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kAutoFillDialogPlacement);
}

// static
void AutoFillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAutoFillInfoBarShown, false);
  prefs->RegisterBooleanPref(prefs::kAutoFillEnabled, true);
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  prefs->RegisterStringPref(prefs::kAutoFillDefaultProfile, std::wstring());
  prefs->RegisterStringPref(prefs::kAutoFillDefaultCreditCard, std::wstring());
}

void AutoFillManager::FormFieldValuesSubmitted(
    const webkit_glue::FormFieldValues& form) {
  if (!IsAutoFillEnabled())
    return;

  if (tab_contents_->profile()->IsOffTheRecord())
    return;

  // Grab a copy of the form data.
  upload_form_structure_.reset(new FormStructure(form));

  if (!upload_form_structure_->IsAutoFillable())
    return;

  // Determine the possible field types and upload the form structure to the
  // PersonalDataManager.
  DeterminePossibleFieldTypes(upload_form_structure_.get());
  HandleSubmit();

  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  bool infobar_shown = prefs->GetBoolean(prefs::kAutoFillInfoBarShown);
  if (!infobar_shown) {
    // Ask the user for permission to save form information.
    infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
  }
}

void AutoFillManager::FormsSeen(
    const std::vector<webkit_glue::FormFieldValues>& forms) {
  if (!IsAutoFillEnabled())
    return;

  for (std::vector<webkit_glue::FormFieldValues>::const_iterator iter =
           forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form_structure = new FormStructure(*iter);
    DeterminePossibleFieldTypes(form_structure);
    form_structures_.push_back(form_structure);
  }

  // Only query the server for form data if the user has profile or
  // credit card data set up.
  if (!personal_data_->profiles().empty() ||
      !personal_data_->credit_cards().empty()) {
    download_manager_.StartQueryRequest(form_structures_);
  }
}

bool AutoFillManager::GetAutoFillSuggestions(
    int query_id, const webkit_glue::FormField& field) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  if (profiles.empty() && credit_cards.empty())
    return false;

  AutoFillFieldType type = UNKNOWN_TYPE;
  for (std::vector<FormStructure*>::iterator form = form_structures_.begin();
       form != form_structures_.end(); ++form) {
    for (std::vector<AutoFillField*>::const_iterator iter = (*form)->begin();
         iter != (*form)->end(); ++iter) {
      // The field list is terminated with a NULL AutoFillField, so don't try to
      // dereference it.
      if (!*iter)
        break;

      AutoFillField* form_field = *iter;
      if (*form_field != field)
        continue;

      if (form_field->possible_types().find(CREDIT_CARD_NAME) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == CREDIT_CARD_NAME) {
        type = CREDIT_CARD_NAME;
        break;
      }

      if (form_field->possible_types().find(CREDIT_CARD_NUMBER) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == CREDIT_CARD_NUMBER) {
        type = CREDIT_CARD_NUMBER;
        break;
      }

      if (form_field->possible_types().find(NAME_FIRST) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == NAME_FIRST) {
        type = NAME_FIRST;
        break;
      }

      if (form_field->possible_types().find(NAME_FULL) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == NAME_FULL) {
        type = NAME_FULL;
        break;
      }
    }
  }

  if (type == UNKNOWN_TYPE)
    return false;

  std::vector<string16> names;
  std::vector<string16> labels;

  // Check for credit card suggestions.
  if (type == CREDIT_CARD_NAME || type == CREDIT_CARD_NUMBER) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      // TODO(jhawkins): What if GetFieldText(...).length() == 0?
      if (StartsWith((*iter)->GetFieldText(AutoFillType(type)),
                                           field.value(), false)) {
        string16 name;
        if (type == CREDIT_CARD_NUMBER) {
          name = (*iter)->ObfuscatedNumber();
        } else {
          name = (*iter)->GetFieldText(AutoFillType(type));
        }
        string16 label = (*iter)->Label();

        names.push_back(name);
        labels.push_back(label);
      }
    }
  } else if (type == NAME_FIRST || type == NAME_FULL) {
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      // TODO(jhawkins): What if GetFieldText(...).length() == 0?
      if (StartsWith((*iter)->GetFieldText(AutoFillType(type)),
                                           field.value(), false)) {
        string16 name = (*iter)->GetFieldText(AutoFillType(type));
        string16 label = (*iter)->Label();

        names.push_back(name);
        labels.push_back(label);
      }
    }
  }

  // No suggestions.
  if (names.empty())
    return false;

  // TODO(jhawkins): If the default profile is in this list, set it as the
  // default suggestion index.
  host->AutoFillSuggestionsReturned(query_id, names, labels, -1);
  return true;
}

bool AutoFillManager::FillAutoFillFormData(int query_id,
                                           const webkit_glue::FormData& form,
                                           const string16& name,
                                           const string16& label) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  if (profiles.empty() && credit_cards.empty())
    return false;

  // Find profile that matches |name| and |label| in question.
  const AutoFillProfile* profile = NULL;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->Label() != label)
      continue;

    if ((*iter)->GetFieldText(AutoFillType(NAME_FIRST)) != name &&
        (*iter)->GetFieldText(AutoFillType(NAME_FULL)) != name)
      continue;

    profile = *iter;
    break;
  }

  // Only look for credit card info if we're not filling profile.
  const CreditCard* credit_card = NULL;
  if (!profile) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->Label() != label)
        continue;

    if ((*iter)->GetFieldText(AutoFillType(CREDIT_CARD_NAME)) != name &&
        (*iter)->ObfuscatedNumber() != name)
      continue;

      credit_card = *iter;
      break;
    }
  }

  if (!profile && !credit_card)
    return false;

  webkit_glue::FormData result = form;
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    const FormStructure* form_structure = *iter;
    if (*form_structure != form)
      continue;

    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      const AutoFillField* field = form_structure->field(i);

      for (size_t j = 0; j < result.fields.size(); ++j) {
        if (field->name() == result.fields[j].name()) {
          AutoFillType autofill_type(field->heuristic_type());
          if (credit_card) {
            result.fields[j].set_value(
                credit_card->GetFieldText(autofill_type));
          } else if (profile) {
            result.fields[j].set_value(
                profile->GetFieldText(autofill_type));
          } else {
            NOTREACHED();
          }
          break;
        }
      }
    }
  }

  host->AutoFillFormDataFilled(query_id, result);
  return true;
}

void AutoFillManager::OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
  // Save the personal data.
  personal_data_->SetProfiles(profiles);
  personal_data_->SetCreditCards(credit_cards);
}

void AutoFillManager::OnPersonalDataLoaded() {
  // We might have been alerted that the PersonalDataManager has loaded, so
  // remove ourselves as observer.
  personal_data_->RemoveObserver(this);

#if !defined(OS_WIN)
#if defined(OS_MACOSX)
  ShowAutoFillDialog(this,
                     personal_data_->web_profiles(),
                     personal_data_->credit_cards(),
                     tab_contents_->profile()->GetOriginalProfile());
#else  // defined(OS_MACOSX)
  ShowAutoFillDialog(NULL, this,
                     tab_contents_->profile()->GetOriginalProfile());
#endif  // defined(OS_MACOSX)
#endif  // !defined(OS_WIN)
}

void AutoFillManager::OnInfoBarClosed() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // Save the imported form data as a profile.
  personal_data_->SaveImportedFormData();
}

void AutoFillManager::OnInfoBarAccepted() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // This is the first time the user is interacting with AutoFill, so set the
  // uploaded form structure as the initial profile in the AutoFillDialog.
  personal_data_->SaveImportedFormData();

#if defined(OS_WIN)
  ShowAutoFillDialog(tab_contents_->GetContentNativeView(), this,
                     tab_contents_->profile()->GetOriginalProfile());
#else
  // If the personal data manager has not loaded the data yet, set ourselves as
  // its observer so that we can listen for the OnPersonalDataLoaded signal.
  if (!personal_data_->IsDataLoaded())
    personal_data_->SetObserver(this);
  else
    OnPersonalDataLoaded();
#endif
}

void AutoFillManager::OnInfoBarCancelled() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, false);
}

void AutoFillManager::Reset() {
  upload_form_structure_.reset();
  form_structures_.reset();
}

void AutoFillManager::OnLoadedAutoFillHeuristics(
    const std::vector<std::string>& form_signatures,
    const std::string& heuristic_xml) {
  // Create a vector of AutoFillFieldTypes,
  // to assign the parsed field types to.
  std::vector<AutoFillFieldType> field_types;
  UploadRequired upload_required = USE_UPLOAD_RATES;

  // Create a parser.
  AutoFillQueryXmlParser parse_handler(&field_types, &upload_required);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(heuristic_xml.c_str(), heuristic_xml.length(), true);
  if (!parse_handler.succeeded()) {
    return;
  }

  // For multiple forms requested, returned field types are in one array.
  // |field_shift| indicates start of the fields for current form.
  size_t field_shift = 0;
  // form_signatures should mirror form_structures_ unless new request is
  // initiated. So if there is a discrepancy we just ignore data and return.
  ScopedVector<FormStructure>::iterator it_forms;
  std::vector<std::string>::const_iterator it_signatures;
  for (it_forms = form_structures_.begin(),
       it_signatures = form_signatures.begin();
       it_forms != form_structures_.end() &&
       it_signatures != form_signatures.end() &&
       (*it_forms)->FormSignature() == *it_signatures;
       ++it_forms, ++it_signatures) {
    DCHECK(field_types.size() - field_shift >= (*it_forms)->field_count());
    for (size_t i = 0; i < (*it_forms)->field_count(); ++i) {
      if (field_types[i + field_shift] != NO_SERVER_DATA &&
          field_types[i + field_shift] != UNKNOWN_TYPE) {
        FieldTypeSet types = (*it_forms)->field(i)->possible_types();
        types.insert(field_types[i + field_shift]);
        (*it_forms)->set_possible_types(i, types);
      }
    }
    field_shift += (*it_forms)->field_count();
  }
}

void AutoFillManager::OnUploadedAutoFillHeuristics(
    const std::string& form_signature) {
}

void AutoFillManager::OnHeuristicsRequestError(
    const std::string& form_signature,
    AutoFillDownloadManager::AutoFillRequestType request_type,
    int http_error) {
}

void AutoFillManager::DeterminePossibleFieldTypes(
    FormStructure* form_structure) {
  // TODO(jhawkins): Update field text.

  form_structure->GetHeuristicAutoFillTypes();

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
  // TODO(jhawkins): Import form data from |form_structures_|.  That will
  // require querying the FormManager for updated field values.
  std::vector<FormStructure*> import;
  import.push_back(upload_form_structure_.get());
  if (!personal_data_->ImportFormData(import, this))
    return;

  UploadFormData();
}

void AutoFillManager::UploadFormData() {
  // TODO(georgey): enable upload request when we make sure that our data is in
  // line with toolbar data:
  // download_manager_.StartUploadRequest(upload_form_structure_,
  //                                      form_is_autofilled);
}

bool AutoFillManager::IsAutoFillEnabled() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();

  // Migrate obsolete AutoFill pref.
  if (prefs->HasPrefPath(prefs::kFormAutofillEnabled)) {
    bool enabled = prefs->GetBoolean(prefs::kFormAutofillEnabled);
    prefs->ClearPref(prefs::kFormAutofillEnabled);
    prefs->SetBoolean(prefs::kAutoFillEnabled, enabled);
    return enabled;
  }

  return prefs->GetBoolean(prefs::kAutoFillEnabled);
}

AutoFillManager::AutoFillManager()
    : tab_contents_(NULL),
      personal_data_(NULL) {
}
