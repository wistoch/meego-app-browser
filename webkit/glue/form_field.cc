// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

namespace webkit_glue {

FormField::FormField() {
}

FormField::FormField(const string16& name,
                     const string16& value)
  : name_(name),
    value_(value) {
}

}  // namespace webkit_glue
