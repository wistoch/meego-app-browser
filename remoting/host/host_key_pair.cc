// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_key_pair.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/crypto/rsa_private_key.h"
#include "base/crypto/signature_creator.h"
#include "base/logging.h"
#include "base/task.h"
#include "remoting/host/host_config.h"

namespace remoting {

HostKeyPair::HostKeyPair() { }

HostKeyPair::~HostKeyPair() { }

void HostKeyPair::Generate() {
  key_.reset(base::RSAPrivateKey::Create(2048));
}

bool HostKeyPair::LoadFromString(const std::string& key_base64) {
  std::string key_str;
  if (!base::Base64Decode(key_base64, &key_str)) {
    LOG(ERROR) << "Failed to decode private key.";
    return false;
  }

  std::vector<uint8> key_buf(key_str.begin(), key_str.end());
  key_.reset(base::RSAPrivateKey::CreateFromPrivateKeyInfo(key_buf));
  if (key_.get() == NULL) {
    LOG(ERROR) << "Invalid private key.";
    return false;
  }

  return true;
}

bool HostKeyPair::Load(HostConfig* host_config) {
  std::string key_base64;
  if (!host_config->GetString(kPrivateKeyConfigPath, &key_base64)) {
    LOG(ERROR) << "Private key wasn't found in the config file.";
    return false;
  }
  return LoadFromString(key_base64);
}

void HostKeyPair::Save(MutableHostConfig* host_config) {
  // Check that the key initialized.
  DCHECK(key_.get() != NULL);

  host_config->Update(
      NewRunnableMethod(this, &HostKeyPair::DoSave, host_config));
}

void HostKeyPair::DoSave(MutableHostConfig* host_config) const {
  std::vector<uint8> key_buf;
  key_->ExportPrivateKey(&key_buf);
  std::string key_str(key_buf.begin(), key_buf.end());
  std::string key_base64;
  base::Base64Encode(key_str, &key_base64);
  host_config->SetString(kPrivateKeyConfigPath, key_base64);
}

std::string HostKeyPair::GetPublicKey() const {
  std::vector<uint8> public_key;
  key_->ExportPublicKey(&public_key);
  std::string public_key_str(public_key.begin(), public_key.end());
  std::string public_key_base64;
  base::Base64Encode(public_key_str, &public_key_base64);
  return public_key_base64;
}

std::string HostKeyPair::GetSignature(const std::string& message) const {
  scoped_ptr<base::SignatureCreator> signature_creator(
      base::SignatureCreator::Create(key_.get()));
  signature_creator->Update(reinterpret_cast<const uint8*>(message.c_str()),
                            message.length());
  std::vector<uint8> signature_buf;
  signature_creator->Final(&signature_buf);
  std::string signature_str(signature_buf.begin(), signature_buf.end());
  std::string signature_base64;
  base::Base64Encode(signature_str, &signature_base64);
  return signature_base64;
}

}  // namespace remoting
