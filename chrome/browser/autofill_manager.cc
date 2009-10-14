// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_manager.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "webkit/glue/autofill_form.h"

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
static const int kMaxAutofillMenuItems = 6;

// static
void AutofillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kFormAutofillEnabled, true);
}

AutofillManager::AutofillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      pending_query_handle_(0),
      request_id_(0) {
  form_autofill_enabled_.Init(prefs::kFormAutofillEnabled,
      profile()->GetPrefs(), NULL);
}

AutofillManager::~AutofillManager() {
  CancelPendingQuery();
}

void AutofillManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    SendSuggestions(NULL);
    WebDataService* web_data_service =
        profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(pending_query_handle_);
  }
  pending_query_handle_ = 0;
}

Profile* AutofillManager::profile() {
  return tab_contents_->profile();
}

void AutofillManager::AutofillFormSubmitted(
    const webkit_glue::AutofillForm& form) {
  StoreFormEntriesInWebDatabase(form);
}

bool AutofillManager::GetAutofillSuggestions(int request_id,
                                             const std::wstring& name,
                                             const std::wstring& prefix) {
  if (!*form_autofill_enabled_)
    return false;

  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return false;
  }

  CancelPendingQuery();

  request_id_ = request_id;

  pending_query_handle_ = web_data_service->GetFormValuesForElementName(
      name, prefix, kMaxAutofillMenuItems, this);
  return true;
}

void AutofillManager::RemoveAutofillEntry(const std::wstring& name,
                                          const std::wstring& value) {
  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  web_data_service->RemoveFormValueForElementName(name, value);
}

void AutofillManager::OnWebDataServiceRequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (*form_autofill_enabled_) {
    DCHECK(result);
    SendSuggestions(result);
  } else {
    SendSuggestions(NULL);
  }
}

void AutofillManager::StoreFormEntriesInWebDatabase(
    const webkit_glue::AutofillForm& form) {
  if (!*form_autofill_enabled_)
    return;

  if (profile()->IsOffTheRecord())
    return;

  profile()->GetWebDataService(Profile::EXPLICIT_ACCESS)->
      AddAutofillFormElements(form.elements);
}

void AutofillManager::SendSuggestions(const WDTypedResult* result) {
  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return;
  if (result) {
    DCHECK(result->GetType() == AUTOFILL_VALUE_RESULT);
    const WDResult<std::vector<std::wstring> >* autofill_result =
        static_cast<const WDResult<std::vector<std::wstring> >*>(result);
    host->AutofillSuggestionsReturned(
        request_id_, autofill_result->GetValue(), -1);
  } else {
    host->AutofillSuggestionsReturned(
        request_id_, std::vector<std::wstring>(), -1);
  }
}
