// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_cache.h"

#include <limits>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

// This CloudPolicyCache currently supports two protocols for the interaction
// with DMServer: the old "DevicePolicy" format, which is being used in the
// CrOS Pilot Program and will be deprecated afterwards, and the new
// "CloudPolicy" format, which will be used exclusively after the public launch
// of ChromeOS.

namespace policy {

// Decodes a CloudPolicySettings object into two maps with mandatory and
// recommended settings, respectively. The implementation is generated code
// in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  ConfigurationPolicyProvider::PolicyMapType* mandatory,
                  ConfigurationPolicyProvider::PolicyMapType* recommended);

// Saves policy information to a file.
class PersistPolicyTask : public Task {
 public:
  PersistPolicyTask(const FilePath& path,
                    const em::CloudPolicyResponse* cloud_policy_response,
                    const em::DevicePolicyResponse* device_policy_response,
                    const bool is_unmanaged)
      : path_(path),
        cloud_policy_response_(cloud_policy_response),
        device_policy_response_(device_policy_response),
        is_unmanaged_(is_unmanaged) {}

 private:
  // Task override.
  virtual void Run();

  const FilePath path_;
  scoped_ptr<const em::CloudPolicyResponse> cloud_policy_response_;
  scoped_ptr<const em::DevicePolicyResponse> device_policy_response_;
  const bool is_unmanaged_;
};

void PersistPolicyTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string data;
  em::CachedCloudPolicyResponse cached_policy;
  if (cloud_policy_response_.get()) {
    cached_policy.mutable_cloud_policy()->CopyFrom(*cloud_policy_response_);
  } else if (device_policy_response_.get()) {
    cached_policy.mutable_device_policy()->CopyFrom(*device_policy_response_);
    cached_policy.set_timestamp(base::Time::NowFromSystemTime().ToTimeT());
  }
  if (is_unmanaged_) {
    cached_policy.set_unmanaged(true);
    cached_policy.set_timestamp(base::Time::NowFromSystemTime().ToTimeT());
  }
  if (!cached_policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << path_.value();
    return;
  }
}

CloudPolicyCache::CloudPolicyCache(
    const FilePath& backing_file_path)
    : backing_file_path_(backing_file_path),
      device_policy_(new DictionaryValue),
      fresh_policy_(false),
      is_unmanaged_(false),
      has_device_policy_(false) {
}

CloudPolicyCache::~CloudPolicyCache() {}

void CloudPolicyCache::LoadPolicyFromFile() {
  // TODO(jkummerow): This method is doing file IO during browser startup. In
  // the long run it would be better to delay this until the FILE thread exists.
  if (!file_util::PathExists(backing_file_path_) || fresh_policy_) {
    return;
  }

  // Read the protobuf from the file.
  std::string data;
  if (!file_util::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    return;
  }

  em::CachedCloudPolicyResponse cached_response;
  if (!cached_response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    return;
  }
  base::Time timestamp;
  PolicyMapType mandatory_policy;
  PolicyMapType recommended_policy;
  is_unmanaged_ = cached_response.unmanaged();
  if (is_unmanaged_ || cached_response.has_device_policy())
    timestamp = base::Time::FromTimeT(cached_response.timestamp());
  if (cached_response.has_cloud_policy()) {
    DCHECK(!is_unmanaged_);
    bool ok = DecodePolicyResponse(cached_response.cloud_policy(),
                                   &mandatory_policy,
                                   &recommended_policy,
                                   &timestamp);
    if (!ok) {
      LOG(WARNING) << "Decoding policy data failed.";
      return;
    }
  }
  if (timestamp > base::Time::NowFromSystemTime()) {
    LOG(WARNING) << "Rejected policy data from " << backing_file_path_.value()
                 << ", file is from the future.";
    return;
  }
  // Swap in the new policy information.
  if (is_unmanaged_) {
    base::AutoLock lock(lock_);
    last_policy_refresh_time_ = timestamp;
    return;
  } else if (cached_response.has_cloud_policy()) {
    if (!fresh_policy_) {
      base::AutoLock lock(lock_);
      mandatory_policy_.swap(mandatory_policy);
      recommended_policy_.swap(recommended_policy);
      last_policy_refresh_time_ = timestamp;
      has_device_policy_ = false;
    }
  } else if (cached_response.has_device_policy()) {
    scoped_ptr<DictionaryValue> value(
        DecodeDevicePolicy(cached_response.device_policy()));
    if (!fresh_policy_) {
      base::AutoLock lock(lock_);
      device_policy_.reset(value.release());
      last_policy_refresh_time_ = timestamp;
      has_device_policy_ = true;
    }
  }
}

bool CloudPolicyCache::SetPolicy(const em::CloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_unmanaged_ = false;
  base::Time timestamp;
  PolicyMapType mandatory_policy;
  PolicyMapType recommended_policy;
  bool ok = DecodePolicyResponse(policy, &mandatory_policy, &recommended_policy,
                                 &timestamp);
  if (!ok) {
    // TODO(jkummerow): Signal error to PolicyProvider.
    return false;
  }
  const bool new_policy_differs =
      !Equals(mandatory_policy, mandatory_policy_) ||
      !Equals(recommended_policy, recommended_policy_);
  {
    base::AutoLock lock(lock_);
    mandatory_policy_.swap(mandatory_policy);
    recommended_policy_.swap(recommended_policy);
    fresh_policy_ = true;
    last_policy_refresh_time_ = timestamp;
    has_device_policy_ = false;
  }

  if (timestamp > base::Time::NowFromSystemTime() +
                  base::TimeDelta::FromMinutes(1)) {
    LOG(WARNING) << "Server returned policy with timestamp from the future, "
                    "not persisting to disk.";
  } else {
    em::CloudPolicyResponse* policy_copy = new em::CloudPolicyResponse;
    policy_copy->CopyFrom(policy);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        new PersistPolicyTask(backing_file_path_, policy_copy, NULL, false));
  }
  return new_policy_differs;
}

bool CloudPolicyCache::SetDevicePolicy(const em::DevicePolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_unmanaged_ = false;
  DictionaryValue* value = DecodeDevicePolicy(policy);
  const bool new_policy_differs = !(value->Equals(device_policy_.get()));
  base::Time now(base::Time::NowFromSystemTime());
  {
    base::AutoLock lock(lock_);
    device_policy_.reset(value);
    fresh_policy_ = true;
    last_policy_refresh_time_ = now;
    has_device_policy_ = true;
  }

  em::DevicePolicyResponse* policy_copy = new em::DevicePolicyResponse;
  policy_copy->CopyFrom(policy);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      new PersistPolicyTask(backing_file_path_, NULL, policy_copy, false));
  return new_policy_differs;
}

DictionaryValue* CloudPolicyCache::GetDevicePolicy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  return device_policy_->DeepCopy();
}

const CloudPolicyCache::PolicyMapType*
    CloudPolicyCache::GetMandatoryPolicy() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return &mandatory_policy_;
}

const CloudPolicyCache::PolicyMapType*
    CloudPolicyCache::GetRecommendedPolicy() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return &recommended_policy_;
}

void CloudPolicyCache::SetUnmanaged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_unmanaged_ = true;
  {
    base::AutoLock lock(lock_);
    mandatory_policy_.clear();
    recommended_policy_.clear();
    device_policy_.reset(new DictionaryValue);
    last_policy_refresh_time_ = base::Time::NowFromSystemTime();
  }
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      new PersistPolicyTask(backing_file_path_, NULL, NULL, true));
}

// static
bool CloudPolicyCache::DecodePolicyResponse(
    const em::CloudPolicyResponse& policy_response,
    PolicyMapType* mandatory,
    PolicyMapType* recommended,
    base::Time* timestamp) {
  std::string data = policy_response.signed_response();

  if (!VerifySignature(policy_response.signature(), data,
                       policy_response.certificate_chain())) {
    LOG(WARNING) << "Failed to verify signature.";
    return false;
  }

  em::SignedCloudPolicyResponse response;
  if (!response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse SignedCloudPolicyResponse protobuf.";
    return false;
  }

  // TODO(jkummerow): Verify response.device_token(). Needs final specification
  // which token we're actually sending / expecting to get back.

  // TODO(jkummerow): Store response.device_name(), if we decide to transfer
  // it from the server to the client.

  DCHECK(timestamp);
  *timestamp = base::Time::FromTimeT(response.timestamp());
  DecodePolicy(response.settings(), mandatory, recommended);
  return true;
}

// static
bool CloudPolicyCache::VerifySignature(
    const std::string& signature,
    const std::string& data,
    const RepeatedPtrField<std::string>& certificate_chain) {
  // TODO(jkummerow): Implement this. Non-trivial because we want to do it
  // for all platforms -> it's enough work to deserve its own CL.
  // Don't forget to also verify the hostname of the server against the cert.
  return true;
}

// static
bool CloudPolicyCache::MapEntryEquals(const PolicyMapType::value_type& a,
                                      const PolicyMapType::value_type& b) {
  return a.first == b.first && Value::Equals(a.second, b.second);
}

// static
bool CloudPolicyCache::Equals(const PolicyMapType& a, const PolicyMapType& b) {
  return a.size() == b.size() &&
      std::equal(a.begin(), a.end(), b.begin(), MapEntryEquals);
}

// static
Value* CloudPolicyCache::DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

// static
Value* CloudPolicyCache::DecodeValue(const em::GenericValue& value) {
  if (!value.has_value_type())
    return NULL;

  switch (value.value_type()) {
    case em::GenericValue::VALUE_TYPE_BOOL:
      if (value.has_bool_value())
        return Value::CreateBooleanValue(value.bool_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_INT64:
      if (value.has_int64_value())
        return DecodeIntegerValue(value.int64_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_STRING:
      if (value.has_string_value())
        return Value::CreateStringValue(value.string_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_DOUBLE:
      if (value.has_double_value())
        return Value::CreateDoubleValue(value.double_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_BYTES:
      if (value.has_bytes_value()) {
        std::string bytes = value.bytes_value();
        return BinaryValue::CreateWithCopiedBuffer(bytes.c_str(), bytes.size());
      }
      return NULL;
    case em::GenericValue::VALUE_TYPE_BOOL_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<bool>::const_iterator i;
      for (i = value.bool_array().begin(); i != value.bool_array().end(); ++i)
        list->Append(Value::CreateBooleanValue(*i));
      return list;
    }
    case em::GenericValue::VALUE_TYPE_INT64_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<google::protobuf::int64>::const_iterator i;
      for (i = value.int64_array().begin();
           i != value.int64_array().end(); ++i) {
        Value* int_value = DecodeIntegerValue(*i);
        if (int_value)
          list->Append(int_value);
      }
      return list;
    }
    case em::GenericValue::VALUE_TYPE_STRING_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedPtrField<std::string>::const_iterator i;
      for (i = value.string_array().begin();
           i != value.string_array().end(); ++i)
        list->Append(Value::CreateStringValue(*i));
      return list;
    }
    case em::GenericValue::VALUE_TYPE_DOUBLE_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<double>::const_iterator i;
      for (i = value.double_array().begin();
           i != value.double_array().end(); ++i)
        list->Append(Value::CreateDoubleValue(*i));
      return list;
    }
    default:
      NOTREACHED() << "Unhandled value type";
  }

  return NULL;
}

// static
DictionaryValue* CloudPolicyCache::DecodeDevicePolicy(
    const em::DevicePolicyResponse& policy) {
  DictionaryValue* result = new DictionaryValue;
  RepeatedPtrField<em::DevicePolicySetting>::const_iterator setting;
  for (setting = policy.setting().begin();
       setting != policy.setting().end();
       ++setting) {
    // Wrong policy key? Skip.
    if (setting->policy_key().compare(kChromeDevicePolicySettingKey) != 0)
      continue;

    // No policy value? Skip.
    if (!setting->has_policy_value())
      continue;

    // Iterate through all the name-value pairs wrapped in |setting|.
    const em::GenericSetting& policy_value(setting->policy_value());
    RepeatedPtrField<em::GenericNamedValue>::const_iterator named_value;
    for (named_value = policy_value.named_value().begin();
         named_value != policy_value.named_value().end();
         ++named_value) {
      if (named_value->has_value()) {
        Value* decoded_value =
            CloudPolicyCache::DecodeValue(named_value->value());
        if (decoded_value)
          result->Set(named_value->name(), decoded_value);
      }
    }
  }
  return result;
}

}  // namespace policy
