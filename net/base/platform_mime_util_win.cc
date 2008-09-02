// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/base/platform_mime_util.h"

#include "base/registry.h"
#include "base/string_util.h"

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const std::wstring& ext, std::string* result) const {
  // check windows registry for file extension's mime type (registry key
  // names are not case-sensitive).
  std::wstring value, key = L"." + ext;
  RegKey(HKEY_CLASSES_ROOT, key.c_str()).ReadValue(L"Content Type", &value);
  if (!value.empty()) {
    *result = WideToUTF8(value);
    return true;
  }
  return false;
}

bool PlatformMimeUtil::GetPreferredExtensionForMimeType(
    const std::string& mime_type, std::wstring* ext) const {
  std::wstring key(L"MIME\\Database\\Content Type\\" + UTF8ToWide(mime_type));
  return RegKey(HKEY_CLASSES_ROOT, key.c_str()).ReadValue(L"Extension", ext);
}

}  // namespace net

