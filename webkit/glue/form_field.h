// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_H_
#define WEBKIT_GLUE_FORM_FIELD_H_

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
 public:
  FormField();
  explicit FormField(WebKit::WebFormControlElement element);
  FormField(const string16& label,
            const string16& name,
            const string16& value,
            const string16& form_control_type,
            int size);

  const string16& label() const { return label_; }
  const string16& name() const { return name_; }
  const string16& value() const { return value_; }
  const string16& form_control_type() const { return form_control_type_; }
  int size() const { return size_; }

  void set_label(const string16& label) { label_ = label; }
  void set_name(const string16& name) { name_ = name; }
  void set_value(const string16& value) { value_ = value; }
  void set_form_control_type(const string16& form_control_type) {
    form_control_type_ = form_control_type;
  }
  void set_size(int size) { size_ = size; }

  bool operator==(const FormField& field) const;
  bool operator!=(const FormField& field) const;

 private:
  string16 label_;
  string16 name_;
  string16 value_;
  string16 form_control_type_;
  int size_;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& profile);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
