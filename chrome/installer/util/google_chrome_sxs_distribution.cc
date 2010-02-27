// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines implementation of GoogleChromeSxSDistribution.

#include "chrome/installer/util/google_chrome_sxs_distribution.h"

#include "installer_util_strings.h"

namespace {

const wchar_t kChromeSxSGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";

}  // namespace

GoogleChromeSxSDistribution::GoogleChromeSxSDistribution() {
  GoogleChromeDistribution::set_product_guid(kChromeSxSGuid);
}

std::wstring GoogleChromeSxSDistribution::GetAppShortCutName() {
  const std::wstring& shortcut_name =
      installer_util::GetLocalizedString(IDS_SXS_SHORTCUT_NAME_BASE);
  return shortcut_name;
}

std::wstring GoogleChromeSxSDistribution::GetInstallSubDir() {
  return GoogleChromeDistribution::GetInstallSubDir().append(
      installer_util::kSxSSuffix);
}

std::wstring GoogleChromeSxSDistribution::GetUninstallRegPath() {
  return GoogleChromeDistribution::GetUninstallRegPath().append(
      installer_util::kSxSSuffix);
}

bool GoogleChromeSxSDistribution::CanSetAsDefault() {
  return false;
}
