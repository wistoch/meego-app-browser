// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/json_value_serializer.h"
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
namespace master_preferences {
const wchar_t kAltFirstRunBubble[] = L"oem_bubble";
const wchar_t kAltShortcutText[] = L"alternate_shortcut_text";
const wchar_t kChromeShortcutIconIndex[] = L"chrome_shortcut_icon_index";
const wchar_t kCreateAllShortcuts[] = L"create_all_shortcuts";
const wchar_t kDistroImportBookmarksPref[] = L"import_bookmarks";
const wchar_t kDistroImportHistoryPref[] = L"import_history";
const wchar_t kDistroImportHomePagePref[] = L"import_home_page";
const wchar_t kDistroImportSearchPref[] = L"import_search_engine";
const wchar_t kDistroPingDelay[] = L"ping_delay";
const wchar_t kDistroShowWelcomePage[] = L"show_welcome_page";
const wchar_t kDistroSkipFirstRunPref[] = L"skip_first_run_ui";
const wchar_t kDoNotCreateShortcuts[] = L"do_not_create_shortcuts";
const wchar_t kDoNotLaunchChrome[] = L"do_not_launch_chrome";
const wchar_t kDoNotRegisterForUpdateLaunch[] =
    L"do_not_register_for_update_launch";
const wchar_t kMakeChromeDefault[] = L"make_chrome_default";
const wchar_t kMakeChromeDefaultForUser[] = L"make_chrome_default_for_user";
const wchar_t kRequireEula[] = L"require_eula";
const wchar_t kSystemLevel[] = L"system_level";
const wchar_t kVerboseLogging[] = L"verbose_logging";
const wchar_t kExtensionsBlock[] = L"extensions.settings";
}

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

std::vector<GURL> GetDefaultBookmarks(const DictionaryValue* prefs) {
  return GetNamedList(L"default_bookmarks", prefs);
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
