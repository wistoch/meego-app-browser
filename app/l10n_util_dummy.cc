// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "unicode/uscript.h"


namespace l10n_util {

std::wstring GetString(int message_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return UTF16ToWide(rb.GetLocalizedString(message_id));
}

}  // namespace l10n_util
