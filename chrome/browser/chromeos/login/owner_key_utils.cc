// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_key_utils.h"

#include <keyhi.h>     // SECKEY_CreateSubjectPublicKeyInfo()
#include <pk11pub.h>
#include <prerror.h>   // PR_GetError()
#include <secder.h>    // DER_Encode()
#include <secmod.h>

#include <limits>

#include "base/crypto/rsa_private_key.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"

using base::RSAPrivateKey;

namespace chromeos {

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtils

// static
OwnerKeyUtils::Factory* OwnerKeyUtils::factory_ = NULL;

OwnerKeyUtils::OwnerKeyUtils() {}

OwnerKeyUtils::~OwnerKeyUtils() {}

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtilsImpl

class OwnerKeyUtilsImpl : public OwnerKeyUtils {
 public:
  OwnerKeyUtilsImpl();

  RSAPrivateKey* GenerateKeyPair();

  bool ExportPublicKeyViaDbus(RSAPrivateKey* pair,
                              LoginLibrary::Delegate<bool>* d);

  bool ExportPublicKeyToFile(RSAPrivateKey* pair, const FilePath& key_file);

  bool ImportPublicKey(const FilePath& key_file,
                       std::vector<uint8>* output);

  RSAPrivateKey* FindPrivateKey(const std::vector<uint8>& key);

  FilePath GetOwnerKeyFilePath();

 protected:
  virtual ~OwnerKeyUtilsImpl();

 private:
  // The file outside the owner's encrypted home directory where her
  // key will live.
  static const char kOwnerKeyFile[];

  // Key generation parameters.
  static const uint16 kKeySizeInBits;

  DISALLOW_COPY_AND_ASSIGN(OwnerKeyUtilsImpl);
};

// Defined here, instead of up above, because we need OwnerKeyUtilsImpl.
OwnerKeyUtils* OwnerKeyUtils::Create() {
  if (!factory_)
    return new OwnerKeyUtilsImpl();
  else
    return factory_->CreateOwnerKeyUtils();
}

// static
const char OwnerKeyUtilsImpl::kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

// We're generating and using 2048-bit RSA keys.
// static
const uint16 OwnerKeyUtilsImpl::kKeySizeInBits = 2048;

OwnerKeyUtilsImpl::OwnerKeyUtilsImpl() {
  // Ensure NSS is initialized.
  base::EnsureNSSInit();
}

OwnerKeyUtilsImpl::~OwnerKeyUtilsImpl() {}

RSAPrivateKey* OwnerKeyUtilsImpl::GenerateKeyPair() {
  return RSAPrivateKey::CreateSensitive(kKeySizeInBits);
}

bool OwnerKeyUtilsImpl::ExportPublicKeyViaDbus(
    RSAPrivateKey* pair,
    LoginLibrary::Delegate<bool>* d) {
  DCHECK(pair);
  bool ok = false;

  std::vector<uint8> to_export;
  if (!pair->ExportPublicKey(&to_export)) {
    LOG(ERROR) << "Formatting key for export via dbus failed!";
    return false;
  }

  if (CrosLibrary::Get()->EnsureLoaded())
    ok = CrosLibrary::Get()->GetLoginLibrary()->SetOwnerKey(to_export, d);

  return ok;
}

bool OwnerKeyUtilsImpl::ExportPublicKeyToFile(RSAPrivateKey* pair,
                                              const FilePath& key_file) {
  DCHECK(pair);
  bool ok = false;
  int safe_file_size = 0;

  std::vector<uint8> to_export;
  if (!pair->ExportPublicKey(&to_export)) {
    LOG(ERROR) << "Formatting key for export failed!";
    return false;
  }

  if (to_export.size() > static_cast<uint>(INT_MAX)) {
    LOG(ERROR) << "key is too big! " << to_export.size();
  } else {
    safe_file_size = static_cast<int>(to_export.size());

    ok = (safe_file_size ==
          file_util::WriteFile(key_file,
                               reinterpret_cast<char*>(&to_export.front()),
                               safe_file_size));
  }
  return ok;
}

bool OwnerKeyUtilsImpl::ImportPublicKey(const FilePath& key_file,
                                        std::vector<uint8>* output) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!file_util::GetFileSize(key_file, &file_size)) {
    LOG(ERROR) << "Could not get size of " << key_file.value();
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << key_file.value() << "is "
               << file_size << "bytes!!!  Too big!";
    return false;
  }
  int32 safe_file_size = static_cast<int32>(file_size);

  output->resize(safe_file_size);
  // Get the key data off of disk
  int data_read = file_util::ReadFile(key_file,
                                      reinterpret_cast<char*>(&(output->at(0))),
                                      safe_file_size);
  return data_read == safe_file_size;
}

RSAPrivateKey* OwnerKeyUtilsImpl::FindPrivateKey(
    const std::vector<uint8>& key) {
  return RSAPrivateKey::FindFromPublicKeyInfo(key);
}

FilePath OwnerKeyUtilsImpl::GetOwnerKeyFilePath() {
  return FilePath(OwnerKeyUtilsImpl::kOwnerKeyFile);
}

}  // namespace chromeos
