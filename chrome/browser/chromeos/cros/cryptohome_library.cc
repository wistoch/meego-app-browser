// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"

namespace chromeos {

bool CryptohomeLibraryImpl::CheckKey(const std::string& user_email,
                                     const std::string& passhash) {
  return chromeos::CryptohomeCheckKey(user_email.c_str(), passhash.c_str());
}

bool CryptohomeLibraryImpl::Mount(const std::string& user_email,
                                  const std::string& passhash) {
  return chromeos::CryptohomeMount(user_email.c_str(), passhash.c_str());
}

bool CryptohomeLibraryImpl::IsMounted() {
  return chromeos::CryptohomeIsMounted();
}

CryptohomeBlob CryptohomeLibraryImpl::GetSystemSalt() {
  return chromeos::CryptohomeGetSystemSalt();
}

}  // namespace chromeos
