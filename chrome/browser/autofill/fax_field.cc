// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/fax_field.h"

#include "base/logging.h"
#include "chrome/browser/autofill/autofill_field.h"

// static
FaxField* FaxField::Parse(std::vector<AutoFillField*>::const_iterator* iter) {
  DCHECK(iter);

  FaxField fax_field;
  if (ParseText(iter, ASCIIToUTF16("fax"), &fax_field.number_))
    return new FaxField(fax_field);

  return NULL;
}

bool FaxField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  return Add(field_type_map, number_, AutoFillType(PHONE_FAX_WHOLE_NUMBER));
}

FaxField::FaxField() : number_(NULL) {}

FaxField::FaxField(const FaxField& fax_field) : number_(fax_field.number_) {}
