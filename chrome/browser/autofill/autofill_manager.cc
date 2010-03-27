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
    std::string request_xml;
    if (form_structure->IsAutoFillable() &&
        form_structure->EncodeUploadRequest(true, true, &request_xml)) {
      download_manager_.StartRequest(request_xml,
                                     form_structure->FormSignature(),
                                     true,
                                     false);
    }
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
  if (profiles.empty())
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
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    string16 name = (*iter)->GetFieldText(AutoFillType(type));
    string16 label = (*iter)->Label();

    // TODO(jhawkins): What if name.length() == 0?
    if (StartsWith(name, field.value(), false)) {
      names.push_back(name);
      labels.push_back(label);
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
  if (profiles.empty())
    return false;

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

  if (!profile)
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
          result.fields[j].set_value(
              profile->GetFieldText(AutoFillType(field->heuristic_type())));
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
    const std::string& form_signature,
    const std::string& heuristic_xml) {
  for (ScopedVector<FormStructure>::iterator it = form_structures_.begin();
      it != form_structures_.end();
      ++it) {
    if ((*it)->FormSignature() == form_signature) {
      // Create a vector of AutoFillFieldTypes,
      // to assign the parsed field types to.
      std::vector<AutoFillFieldType> field_types;
      UploadRequired upload_required = USE_UPLOAD_RATES;

      // Create a parser.
      AutoFillQueryXmlParser parse_handler(&field_types, &upload_required);
      buzz::XmlParser parser(&parse_handler);
      parser.Parse(heuristic_xml.c_str(), heuristic_xml.length(), true);
      if (parse_handler.succeeded()) {
        DCHECK(field_types.size() == (*it)->field_count());
        if (field_types.size() == (*it)->field_count()) {
          for (size_t i = 0; i < (*it)->field_count(); ++i) {
            if (field_types[i] != NO_SERVER_DATA &&
                field_types[i] != UNKNOWN_TYPE) {
             FieldTypeSet types = (*it)->field(i)->possible_types();
             types.insert(field_types[i]);
             (*it)->set_possible_types(i, types);
            }
          }
        }
        return;
      }
    }
  }
}

void AutoFillManager::OnUploadedAutoFillHeuristics(
    const std::string& form_signature) {
}

void AutoFillManager::OnHeuristicsRequestError(
    const std::string& form_signature, int http_error) {
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
  std::string xml;
  bool ok = upload_form_structure_->EncodeUploadRequest(false, false, &xml);
  DCHECK(ok);

  // TODO(georgey): enable upload request when we make sure that our data is in
  // line with toolbar data:
  // download_manager_.StartRequest(xml,
  //                                upload_form_structure_->FormSignature(),
  //                                false,
  //                                form_is_autofilled);
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
