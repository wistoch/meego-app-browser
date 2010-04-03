// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_
#define CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_

#include <string>
#include <vector>

#include "base/scoped_vector.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "googleurl/src/gurl.h"

namespace buzz {
  class XmlElement;
}  // namespace buzz

namespace webkit_glue {
struct FormData;
}  // namespace webkit_glue

enum RequestMethod {
  GET,
  POST
};

enum UploadRequired {
  UPLOAD_NOT_REQUIRED,
  UPLOAD_REQUIRED,
  USE_UPLOAD_RATES
};

// FormStructure stores a single HTML form together with the values entered
// in the fields along with additional information needed by AutoFill.
class FormStructure {
 public:
  explicit FormStructure(const webkit_glue::FormData& form);

  // Encodes the XML upload request from this FormStructure.
  bool EncodeUploadRequest(bool auto_fill_used,
                           std::string* encoded_xml) const;

  // Encodes the XML query request for the set of forms.
  // All fields are returned in one XML. For example, there are three forms,
  // with 2, 4, and 3 fields. The returned XML would have type info for 9
  // fields, first two of which would be for the first form, next 4 for the
  // second, and the rest is for the third.
  static bool EncodeQueryRequest(const ScopedVector<FormStructure>& forms,
                                 std::string* encoded_xml);

  // Runs several heuristics against the form fields to determine their possible
  // types.
  void GetHeuristicAutoFillTypes();

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  std::string FormSignature() const;

  // Runs a quick heuristic to rule out pages but obviously not auto-fillable,
  // like google/yahoo/msn search, etc.
  bool IsAutoFillable() const;

  // Sets the possible types for the field at |index|.
  void set_possible_types(int index, const FieldTypeSet& types);

  const AutoFillField* field(int index) const;
  size_t field_count() const;

  // Used for iterating over the fields.
  std::vector<AutoFillField*>::const_iterator begin() const {
    return fields_.begin();
  }
  std::vector<AutoFillField*>::const_iterator end() const {
    return fields_.end();
  }

  bool operator==(const webkit_glue::FormData& form) const;
  bool operator!=(const webkit_glue::FormData& form) const;

 private:
  enum EncodeRequestType {
    QUERY,
    UPLOAD,
  };

  // Associates the field with the heuristic type for each of the field views.
  void GetHeuristicFieldInfo(FieldTypeMap* field_types_map);

  // Adds form info to |encompassing_xml_element|. |request_type| indicates if
  // it is a query or upload.
  bool EncodeFormRequest(EncodeRequestType request_type,
                         buzz::XmlElement* encompassing_xml_element) const;

  // The name of the form.
  // TODO(jhawkins): string16
  std::string form_name_;

  // The source URL.
  GURL source_url_;

  // The target URL.
  GURL target_url_;

  bool has_credit_card_field_;
  bool has_autofillable_field_;
  bool has_password_fields_;

  // A vector of all the input fields in the form.  The vector is terminated by
  // a NULL entry.
  ScopedVector<AutoFillField> fields_;

  // The names of the form input elements, that are part of the form signature.
  // The string starts with "&" and the names are also separated by the "&"
  // character. E.g.: "&form_input1_name&form_input2_name&...&form_inputN_name"
  std::string form_signature_field_names_;

  // GET or POST.
  RequestMethod method_;
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_
