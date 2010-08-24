// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/common/policy_constants.h"
#include "chrome/common/notification_service.h"

namespace {

// TODO(avi): Use this mapping to auto-generate MCX manifests and Windows
// ADM/ADMX files. http://crbug.com/49316

struct InternalPolicyValueMapEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const char* name;
};

const InternalPolicyValueMapEntry kPolicyValueMap[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
      Value::TYPE_STRING, policy::key::kHomepageLocation },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
      Value::TYPE_BOOLEAN, policy::key::kHomepageIsNewTabPage },
  { ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::TYPE_INTEGER, policy::key::kProxyServerMode },
  { ConfigurationPolicyStore::kPolicyProxyServer,
      Value::TYPE_STRING, policy::key::kProxyServer },
  { ConfigurationPolicyStore::kPolicyProxyPacUrl,
      Value::TYPE_STRING, policy::key::kProxyPacUrl },
  { ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::TYPE_STRING, policy::key::kProxyBypassList },
  { ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
      Value::TYPE_BOOLEAN, policy::key::kAlternateErrorPagesEnabled },
  { ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
      Value::TYPE_BOOLEAN, policy::key::kSearchSuggestEnabled },
  { ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kDnsPrefetchingEnabled },
  { ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kSafeBrowsingEnabled },
  { ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kMetricsReportingEnabled },
  { ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
      Value::TYPE_BOOLEAN, policy::key::kPasswordManagerEnabled },
  { ConfigurationPolicyStore::kPolicyDisabledPlugins,
      Value::TYPE_STRING, policy::key::kDisabledPluginsList },
  { ConfigurationPolicyStore::kPolicyApplicationLocale,
      Value::TYPE_STRING, policy::key::kApplicationLocaleValue },
  { ConfigurationPolicyStore::kPolicySyncDisabled,
      Value::TYPE_BOOLEAN, policy::key::kSyncDisabled },
  { ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
      Value::TYPE_LIST, policy::key::kExtensionInstallAllowList },
  { ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
      Value::TYPE_LIST, policy::key::kExtensionInstallDenyList },
  { ConfigurationPolicyStore::kPolicyShowHomeButton,
      Value::TYPE_BOOLEAN, policy::key::kShowHomeButton },
};

}  // namespace

/* static */
const ConfigurationPolicyProvider::PolicyValueMap*
    ConfigurationPolicyProvider::PolicyValueMapping() {
  static PolicyValueMap* mapping;
  if (!mapping) {
    mapping = new PolicyValueMap();
    for (size_t i = 0; i < arraysize(kPolicyValueMap); ++i) {
      const InternalPolicyValueMapEntry& internal_entry = kPolicyValueMap[i];
      PolicyValueMapEntry entry;
      entry.policy_type = internal_entry.policy_type;
      entry.value_type = internal_entry.value_type;
      entry.name = std::string(internal_entry.name);
      mapping->push_back(entry);
    }
  }
  return mapping;
}

void ConfigurationPolicyProvider::NotifyStoreOfPolicyChange() {
  NotificationService::current()->Notify(
      NotificationType::POLICY_CHANGED,
      Source<ConfigurationPolicyProvider>(this),
      NotificationService::NoDetails());
}
