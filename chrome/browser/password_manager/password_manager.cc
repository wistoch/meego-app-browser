// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormMap;

// static
void PasswordManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled, true);
}

PasswordManager::PasswordManager(Delegate* delegate)
    : login_managers_deleter_(&pending_login_managers_),
      delegate_(delegate),
      observer_(NULL) {
  DCHECK(delegate_);
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
      delegate_->GetProfileForPasswordManager()->GetPrefs(), NULL);
}

PasswordManager::~PasswordManager() {
}

void PasswordManager::ProvisionallySavePassword(PasswordForm form) {
  if (!delegate_->GetProfileForPasswordManager() ||
      delegate_->GetProfileForPasswordManager()->IsOffTheRecord() ||
      !*password_manager_enabled_)
    return;

  // No password to save? Then don't.
  if (form.password_value.empty())
    return;

  LoginManagers::iterator iter;
  PasswordFormManager* manager = NULL;
  for (iter = pending_login_managers_.begin();
       iter != pending_login_managers_.end(); iter++) {
    if ((*iter)->DoesManage(form)) {
      manager = *iter;
      break;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!manager)
    return;

  // If we found a manager but it didn't finish matching yet, the user has
  // tried to submit credentials before we had time to even find matching
  // results for the given form and autofill. If this is the case, we just
  // give up.
  if (!manager->HasCompletedMatching())
    return;

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted())
    return;

  form.ssl_valid = form.origin.SchemeIsSecure() &&
      !delegate_->DidLastPageLoadEncounterSSLErrors();
  form.preferred = true;
  manager->ProvisionallySave(form);
  provisional_save_manager_.reset(manager);
  pending_login_managers_.erase(iter);
  // We don't care about the rest of the forms on the page now that one
  // was selected.
  STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::DidNavigate() {
  // As long as this navigation isn't due to a currently pending
  // password form submit, we're ready to reset and move on.
  if (!provisional_save_manager_.get() && !pending_login_managers_.empty())
    STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::ClearProvisionalSave() {
  provisional_save_manager_.reset();
}

void PasswordManager::DidStopLoading() {
  if (!provisional_save_manager_.get())
    return;

  DCHECK(!delegate_->GetProfileForPasswordManager()->IsOffTheRecord());
  DCHECK(!provisional_save_manager_->IsBlacklisted());

  if (!delegate_->GetProfileForPasswordManager())
    return;
  if (provisional_save_manager_->IsNewLogin()) {
    delegate_->AddSavePasswordInfoBar(provisional_save_manager_.release());
  } else {
    // If the save is not a new username entry, then we just want to save this
    // data (since the user already has related data saved), so don't prompt.
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::PasswordFormsSeen(
    const std::vector<PasswordForm>& forms) {
  if (!delegate_->GetProfileForPasswordManager())
    return;
  if (!*password_manager_enabled_)
    return;

  // Ask the SSLManager for current security.
  bool had_ssl_error = delegate_->DidLastPageLoadEncounterSSLErrors();

  std::vector<PasswordForm>::const_iterator iter;
  for (iter = forms.begin(); iter != forms.end(); iter++) {
    if (provisional_save_manager_.get() &&
        provisional_save_manager_->DoesManage(*iter)) {
      // The form trying to be saved has immediately re-appeared. Assume
      // login failure and abort this save.  Fallback to pending login state
      // since the user may try again.
      pending_login_managers_.push_back(provisional_save_manager_.release());
      // Don't delete the login managers since the user may try again
      // and we want to be able to save in that case.
      break;
    } else {
      bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
      PasswordFormManager* manager =
          new PasswordFormManager(delegate_->GetProfileForPasswordManager(),
                                  this, *iter, ssl_valid);
      pending_login_managers_.push_back(manager);
      manager->FetchMatchingLoginsFromWebDatabase();
    }
  }
}

void PasswordManager::Autofill(
    const PasswordForm& form_for_autofill,
    const PasswordFormMap& best_matches,
    const PasswordForm* const preferred_match) const {
  DCHECK(preferred_match);
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observer_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      bool action_mismatch = form_for_autofill.action.GetWithEmptyPath() !=
                             preferred_match->action.GetWithEmptyPath();
      webkit_glue::PasswordFormDomManager::FillData fill_data;
      webkit_glue::PasswordFormDomManager::InitFillData(form_for_autofill,
                                                        best_matches,
                                                        preferred_match,
                                                        action_mismatch,
                                                        &fill_data);
      delegate_->FillPasswordForm(fill_data);
      return;
    }
    default:
      if (observer_) {
        observer_->OnAutofillDataAvailable(
            UTF16ToWideHack(preferred_match->username_value),
            UTF16ToWideHack(preferred_match->password_value));
      }
  }
}
