// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/glue/form_field_values.h"

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      infobar_(NULL) {
  personal_data_ = tab_contents_->profile()->GetPersonalDataManager();
}

AutoFillManager::~AutoFillManager() {
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

  // TODO(jhawkins): Determine possible field types.

  if (!personal_data_->ImportFormData(form_structures_, this))
    return;

  // Ask the user for permission to save form information.
  infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
}

void AutoFillManager::SaveFormData() {
  UploadFormData();

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
