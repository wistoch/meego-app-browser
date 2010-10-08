// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <unicode/uidna.h>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"

namespace x509_certificate_model {

std::string ProcessIDN(const std::string& input) {
  // Convert the ASCII input to a string16 for ICU.
  string16 input16;
  input16.reserve(input.length());
  std::copy(input.begin(), input.end(), std::back_inserter(input16));

  string16 output16;
  output16.resize(input.length());

  UErrorCode status = U_ZERO_ERROR;
  int output_chars = uidna_IDNToUnicode(input16.data(), input.length(),
                                        &output16[0], output16.length(),
                                        UIDNA_DEFAULT, NULL, &status);
  if (status == U_ZERO_ERROR) {
    output16.resize(output_chars);
  } else if (status != U_BUFFER_OVERFLOW_ERROR) {
    return input;
  } else {
    output16.resize(output_chars);
    output_chars = uidna_IDNToUnicode(input16.data(), input.length(),
                                      &output16[0], output16.length(),
                                      UIDNA_DEFAULT, NULL, &status);
    if (status != U_ZERO_ERROR)
      return input;
    DCHECK_EQ(static_cast<size_t>(output_chars), output16.length());
    output16.resize(output_chars);  // Just to be safe.
  }

  if (input16 == output16)
    return input;  // Input did not contain any encoded data.

  // Input contained encoded data, return formatted string showing original and
  // decoded forms.
  return l10n_util::GetStringFUTF8(IDS_CERT_INFO_IDN_VALUE_FORMAT,
                                   input16, output16);
}

}  // x509_certificate_model

