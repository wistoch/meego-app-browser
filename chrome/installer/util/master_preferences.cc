// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/util_constants.h"
#include "googleurl/src/gurl.h"


namespace {
const wchar_t* kDistroDict = L"distribution";

bool GetGURLFromValue(const Value* in_value, GURL* out_value) {
  if (!in_value || !out_value)
    return false;
  std::string url;
  in_value->GetAsString(&url);
  GURL gurl(url);
  *out_value = gurl;
  return true;
}

std::vector<GURL> GetNamedList(const wchar_t* name,
                               const DictionaryValue* prefs) {
  std::vector<GURL> list;
  if (!prefs)
    return list;
  ListValue* value_list = NULL;
  if (!prefs->GetList(name, &value_list))
    return list;
  for (size_t i = 0; i < value_list->GetSize(); ++i) {
    Value* entry;
    GURL gurl_entry;
    if (!value_list->Get(i, &entry) || !GetGURLFromValue(entry, &gurl_entry)) {
      NOTREACHED();
      break;
    }
    list.push_back(gurl_entry);
  }
  return list;
}

}

namespace installer_util {

bool GetDistroBooleanPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                bool* value) {
  if (!prefs || !value)
    return false;

  DictionaryValue* distro = NULL;
  if (!prefs->GetDictionary(kDistroDict, &distro) || !distro)
    return false;

  if (!distro->GetBoolean(name, value))
    return false;

  return true;
}

bool GetDistroStringPreference(const DictionaryValue* prefs,
                               const std::wstring& name,
                               std::wstring* value) {
  if (!prefs || !value)
    return false;

  DictionaryValue* distro = NULL;
  if (!prefs->GetDictionary(kDistroDict, &distro) || !distro)
    return false;

  std::wstring str_value;
  if (!distro->GetString(name, &str_value))
    return false;

  if (str_value.empty())
    return false;

  *value = str_value;
  return true;
}

bool GetDistroIntegerPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                int* value) {
  if (!prefs || !value)
    return false;

  DictionaryValue* distro = NULL;
  if (!prefs->GetDictionary(kDistroDict, &distro) || !distro)
    return false;

  if (!distro->GetInteger(name, value))
    return false;

  return true;
}

DictionaryValue* GetInstallPreferences(const CommandLine& cmd_line) {
  DictionaryValue* prefs = NULL;
#if defined(OS_WIN)
  if (cmd_line.HasSwitch(installer_util::switches::kInstallerData)) {
    FilePath prefs_path(
        cmd_line.GetSwitchValue(installer_util::switches::kInstallerData));
    prefs = installer_util::ParseDistributionPreferences(prefs_path);
  }

  if (!prefs)
    prefs = new DictionaryValue();

  if (cmd_line.HasSwitch(installer_util::switches::kCreateAllShortcuts))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kCreateAllShortcuts, true);

  if (cmd_line.HasSwitch(installer_util::switches::kDoNotCreateShortcuts))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kDoNotCreateShortcuts, true);

  if (cmd_line.HasSwitch(installer_util::switches::kMsi))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kMsi, true);

  if (cmd_line.HasSwitch(
        installer_util::switches::kDoNotRegisterForUpdateLaunch))
    installer_util::SetDistroBooleanPreference(
        prefs,
        installer_util::master_preferences::kDoNotRegisterForUpdateLaunch,
        true);

  if (cmd_line.HasSwitch(installer_util::switches::kDoNotLaunchChrome))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kDoNotLaunchChrome, true);

  if (cmd_line.HasSwitch(installer_util::switches::kMakeChromeDefault))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kMakeChromeDefault, true);

  if (cmd_line.HasSwitch(installer_util::switches::kSystemLevel))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kSystemLevel, true);

  if (cmd_line.HasSwitch(installer_util::switches::kVerboseLogging))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kVerboseLogging, true);

  if (cmd_line.HasSwitch(installer_util::switches::kAltDesktopShortcut))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kAltShortcutText, true);
#endif

  return prefs;
}

DictionaryValue* ParseDistributionPreferences(
    const FilePath& master_prefs_path) {
  if (!file_util::PathExists(master_prefs_path)) {
    LOG(WARNING) << "Master preferences file not found: "
                 << master_prefs_path.value();
    return NULL;
  }
  std::string json_data;
  if (!file_util::ReadFileToString(master_prefs_path, &json_data))
    return NULL;

  JSONStringValueSerializer json(json_data);
  scoped_ptr<Value> root(json.Deserialize(NULL));

  if (!root.get())
    return NULL;

  if (!root->IsType(Value::TYPE_DICTIONARY))
    return NULL;

  return static_cast<DictionaryValue*>(root.release());
}

std::vector<GURL> GetFirstRunTabs(const DictionaryValue* prefs) {
  return GetNamedList(L"first_run_tabs", prefs);
}

bool SetDistroBooleanPreference(DictionaryValue* prefs,
                                const std::wstring& name,
                                bool value) {
  if (!prefs || name.empty())
    return false;
  prefs->SetBoolean(std::wstring(kDistroDict) + L"." + name, value);
  return true;
}

bool HasExtensionsBlock(const DictionaryValue* prefs,
                        DictionaryValue** extensions) {
  return (prefs->GetDictionary(master_preferences::kExtensionsBlock,
                               extensions));
}

}  // installer_util
