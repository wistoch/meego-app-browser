// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_model_associator.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/synchronized_preferences.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

PreferenceModelAssociator::PreferenceModelAssociator(
    ProfileSyncService* sync_service,
    UnrecoverableErrorHandler* error_handler)
    : sync_service_(sync_service),
      error_handler_(error_handler),
      preferences_node_id_(sync_api::kInvalidId) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(sync_service_);

  // Add the list of kSynchronizedPreferences to our local
  // synced_preferences set, taking care to filter out any preferences
  // that are not registered.
  PrefService* pref_service = sync_service_->profile()->GetPrefs();
  for (size_t i = 0;
       i < static_cast<size_t>(arraysize(kSynchronizedPreferences)); ++i) {
    if (pref_service->FindPreference(kSynchronizedPreferences[i]))
      synced_preferences_.insert(kSynchronizedPreferences[i]);
  }
}

PreferenceModelAssociator::~PreferenceModelAssociator() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

bool PreferenceModelAssociator::AssociateModels() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* pref_service = sync_service_->profile()->GetPrefs();

  int64 root_id;
  if (!GetSyncIdForTaggedNode(kPreferencesTag, &root_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  sync_api::WriteTransaction trans(
      sync_service()->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByIdLookup(root_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  base::JSONReader reader;
  for (std::set<std::wstring>::iterator it = synced_preferences_.begin();
       it != synced_preferences_.end(); ++it) {
    std::string tag = WideToUTF8(*it);
    const PrefService::Preference* pref =
        pref_service->FindPreference((*it).c_str());
    DCHECK(pref);

    sync_api::WriteNode node(&trans);
    if (node.InitByClientTagLookup(syncable::PREFERENCES, tag)) {
      const sync_pb::PreferenceSpecifics& preference(
          node.GetPreferenceSpecifics());
      DCHECK_EQ(tag, preference.name());

      if (!pref->IsManaged()) {
        scoped_ptr<Value> value(
            reader.JsonToValue(preference.value(), false, false));
        std::wstring pref_name = UTF8ToWide(preference.name());
        if (!value.get()) {
          LOG(ERROR) << "Failed to deserialize preference value: "
                     << reader.GetErrorMessage();
          return false;
        }

        // Merge the server value of this preference with the local value.
        bool node_needs_update = MergePreference(*pref, value.get());

        // Update the local preference based on what we got from the
        // sync server.
        pref_service->Set(pref_name.c_str(), *value);

        // If the merge resulted in an updated value, write it back to
        // the sync node.
        if (node_needs_update && !WritePreferenceToNode(*pref, &node))
          return false;
      }
      Associate(pref, node.GetId());
    } else if (!pref->IsManaged()) {
      // If there is no server value for this preference and it is
      // currently its default value, don't create a new server node.
      if (pref->IsDefaultValue())
        continue;

      sync_api::WriteNode write_node(&trans);
      if (!write_node.InitUniqueByCreation(syncable::PREFERENCES, root, tag)) {
        LOG(ERROR) << "Failed to create preference sync node.";
        return false;
      }

      // Update the sync node with the local value for this preference.
      if (!WritePreferenceToNode(*pref, &write_node))
        return false;

      Associate(pref, write_node.GetId());
    }
  }
  return true;
}

bool PreferenceModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool PreferenceModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  int64 preferences_sync_id;
  if (!GetSyncIdForTaggedNode(kPreferencesTag, &preferences_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(
      sync_service()->backend()->GetUserShareHandle());

  sync_api::ReadNode preferences_node(&trans);
  if (!preferences_node.InitByIdLookup(preferences_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the preferences folder has any
  // children.
  *has_nodes = sync_api::kInvalidId != preferences_node.GetFirstChildId();
  return true;
}

bool PreferenceModelAssociator::ChromeModelHasUserCreatedNodes(
    bool* has_nodes) {
  DCHECK(has_nodes);
  // Assume the preferences model always have user-created nodes.
  *has_nodes = true;
  return true;
}

int64 PreferenceModelAssociator::GetSyncIdFromChromeId(
    const std::wstring preference_name) {
  PreferenceNameToSyncIdMap::const_iterator iter =
      id_map_.find(preference_name);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void PreferenceModelAssociator::Associate(
    const PrefService::Preference* preference, int64 sync_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(preference->name()) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[preference->name()] = sync_id;
  id_map_inverse_[sync_id] = preference->name();
}

void PreferenceModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  SyncIdToPreferenceNameMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second);
  id_map_inverse_.erase(iter);
}

bool PreferenceModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                       int64* sync_id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool PreferenceModelAssociator::MergePreference(
    const PrefService::Preference& local_pref,
    Value* server_value) {

  if (local_pref.name() == prefs::kURLsToRestoreOnStartup ||
      local_pref.name() == prefs::kDesktopNotificationAllowedOrigins ||
      local_pref.name() == prefs::kDesktopNotificationDeniedOrigins) {
    return MergeListValues(*local_pref.GetValue(), server_value);
  }

  if (local_pref.name() == prefs::kContentSettingsPatterns ||
      local_pref.name() == prefs::kGeolocationContentSettings) {
    return MergeDictionaryValues(*local_pref.GetValue(), server_value);
  }
  return false;
}

bool PreferenceModelAssociator::WritePreferenceToNode(
    const PrefService::Preference& pref,
    sync_api::WriteNode* node) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(*pref.GetValue())) {
    LOG(ERROR) << "Failed to serialize preference value.";
    return false;
  }

  sync_pb::PreferenceSpecifics preference;
  preference.set_name(WideToUTF8(pref.name()));
  preference.set_value(serialized);
  node->SetPreferenceSpecifics(preference);
  node->SetTitle(pref.name());
  return true;
}

bool PreferenceModelAssociator::MergeListValues(const Value& from_value,
                                                Value* to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return false;
  bool to_changed = false;

  DCHECK(from_value.GetType() == Value::TYPE_LIST);
  DCHECK(to_value->GetType() == Value::TYPE_LIST);
  const ListValue& from_list_value = static_cast<const ListValue&>(from_value);
  ListValue* to_list_value = static_cast<ListValue*>(to_value);

  for (ListValue::const_iterator i = from_list_value.begin();
       i != from_list_value.end(); ++i) {
    Value* value = (*i)->DeepCopy();
    if (to_list_value->AppendIfNotPresent(value)) {
      to_changed = true;
    } else {
      delete value;
    }
  }
  return to_changed;
}

bool PreferenceModelAssociator::MergeDictionaryValues(const Value& from_value,
                                                      Value* to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return false;
  bool to_changed = false;

  DCHECK(from_value.GetType() == Value::TYPE_DICTIONARY);
  DCHECK(to_value->GetType() == Value::TYPE_DICTIONARY);
  const DictionaryValue& from_dict_value =
      static_cast<const DictionaryValue&>(from_value);
  DictionaryValue* to_dict_value = static_cast<DictionaryValue*>(to_value);

  for (DictionaryValue::key_iterator key = from_dict_value.begin_keys();
       key != from_dict_value.end_keys(); ++key) {
    Value* from_value;
    bool success = from_dict_value.GetWithoutPathExpansion(*key, &from_value);
    DCHECK(success);

    Value* to_value;
    if (to_dict_value->GetWithoutPathExpansion(*key, &to_value)) {
      if (to_value->GetType() == Value::TYPE_DICTIONARY) {
        to_changed = MergeDictionaryValues(*from_value, to_value);
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      to_dict_value->SetWithoutPathExpansion(*key, from_value->DeepCopy());
      to_changed = true;
    }
  }
  return to_changed;
}

}  // namespace browser_sync
