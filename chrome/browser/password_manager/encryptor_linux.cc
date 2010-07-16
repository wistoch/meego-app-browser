// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

#include "base/crypto/encryptor.h"
#include "base/crypto/symmetric_key.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"

namespace {

// Salt for Symmetric key derivation.
const char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
const size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
const size_t kEncryptionIterations = 1;

// Size of initialization vector for AES 128-bit.
const size_t kIVBlockSizeAES128 = 16;

// Prefix for cypher text returned by obfuscation version.  We prefix the
// cyphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss.
const char kObfuscationPrefix[] = "v10";

// Generates a newly allocated SymmetricKey object based a hard-coded password.
// Ownership of the key is passed to the caller.  Returns NULL key if a key
// generation error occurs.
base::SymmetricKey* GetEncryptionKey() {
  // We currently "obfuscate" by encrypting and decrypting with hard-coded
  // password.  We need to improve this password situation by moving a secure
  // password into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115
  std::string password = "peanuts";
  std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  scoped_ptr<base::SymmetricKey> encryption_key(
      base::SymmetricKey::DeriveKeyFromPassword(base::SymmetricKey::AES,
                                                password,
                                                salt,
                                                kEncryptionIterations,
                                                kDerivedKeySizeInBits));
  DCHECK(encryption_key.get());

  return encryption_key.release();
}

}  // namespace

bool Encryptor::EncryptString16(const string16& plaintext,
                                std::string* ciphertext) {
  return EncryptString(UTF16ToUTF8(plaintext), ciphertext);
}

bool Encryptor::DecryptString16(const std::string& ciphertext,
                                string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = UTF8ToUTF16(utf8);
  return true;
}

bool Encryptor::EncryptString(const std::string& plaintext,
                              std::string* ciphertext) {
  // This currently "obfuscates" by encrypting with hard-coded password.
  // We need to improve this password situation by moving a secure password
  // into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115

  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  scoped_ptr<base::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  std::string iv(kIVBlockSizeAES128, ' ');
  base::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), base::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cypher text with version information.
  ciphertext->insert(0, kObfuscationPrefix);
  return true;
}

bool Encryptor::DecryptString(const std::string& ciphertext,
                              std::string* plaintext) {
  // This currently "obfuscates" by encrypting with hard-coded password.
  // We need to improve this password situation by moving a secure password
  // into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115

  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  if (ciphertext.find(kObfuscationPrefix) != 0) {
    *plaintext = ciphertext;
    return true;
  }

  // Strip off the versioning prefix before decrypting.
  std::string raw_ciphertext = ciphertext.substr(strlen(kObfuscationPrefix));

  scoped_ptr<base::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  std::string iv(kIVBlockSizeAES128, ' ');
  base::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), base::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(raw_ciphertext, plaintext))
    return false;

  return true;
}
