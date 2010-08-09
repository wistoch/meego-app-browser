// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/content_settings_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

typedef HostContentSettingsMap::ContentSettingsDetails ContentSettingsDetails;

namespace {

std::string ContentSettingsTypeToGroupName(ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return "cookies";
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return "images";
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return "javascript";
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return "plugins";
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return "popups";
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return "location";
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return "notifications";

    default:
      NOTREACHED();
      return "";
  }
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  if (name == "cookies")
    return CONTENT_SETTINGS_TYPE_COOKIES;
  if (name == "images")
    return CONTENT_SETTINGS_TYPE_IMAGES;
  if (name == "javascript")
    return CONTENT_SETTINGS_TYPE_JAVASCRIPT;
  if (name == "plugins")
    return CONTENT_SETTINGS_TYPE_PLUGINS;
  if (name == "popups")
    return CONTENT_SETTINGS_TYPE_POPUPS;
  if (name == "location")
    return CONTENT_SETTINGS_TYPE_GEOLOCATION;
  if (name == "notifications")
    return CONTENT_SETTINGS_TYPE_NOTIFICATIONS;

  NOTREACHED();
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

std::string ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "allow";
    case CONTENT_SETTING_ASK:
      return "ask";
    case CONTENT_SETTING_BLOCK:
      return "block";

    default:
      NOTREACHED();
      return "";
  }
}

ContentSetting ContentSettingFromString(const std::string& name) {
  if (name == "allow")
    return CONTENT_SETTING_ALLOW;
  if (name == "ask")
    return CONTENT_SETTING_ASK;
  if (name == "block")
    return CONTENT_SETTING_BLOCK;

  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

}  // namespace

ContentSettingsHandler::ContentSettingsHandler() {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("content_exceptions",
      l10n_util::GetStringUTF16(IDS_COOKIES_EXCEPTIONS_BUTTON));
  localized_strings->SetString("contentSettingsPage",
      l10n_util::GetStringUTF16(IDS_CONTENT_SETTINGS_TITLE));
  localized_strings->SetString("allowException",
      l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON));
  localized_strings->SetString("blockException",
      l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON));
  localized_strings->SetString("addExceptionRow",
      l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ADD_BUTTON));
  localized_strings->SetString("removeExceptionRow",
      l10n_util::GetStringUTF16(IDS_EXCEPTIONS_REMOVE_BUTTON));
  localized_strings->SetString("editExceptionRow",
      l10n_util::GetStringUTF16(IDS_EXCEPTIONS_EDIT_BUTTON));

  // Cookies filter.
  localized_strings->SetString("cookies_tab_label",
      l10n_util::GetStringUTF16(IDS_COOKIES_TAB_LABEL));
  localized_strings->SetString("cookies_modify",
      l10n_util::GetStringUTF16(IDS_MODIFY_COOKIE_STORING_LABEL));
  localized_strings->SetString("cookies_allow",
      l10n_util::GetStringUTF16(IDS_COOKIES_ALLOW_RADIO));
  localized_strings->SetString("cookies_block",
      l10n_util::GetStringUTF16(IDS_COOKIES_BLOCK_RADIO));
  localized_strings->SetString("cookies_block_3rd_party",
      l10n_util::GetStringUTF16(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX));
  localized_strings->SetString("cookies_clear_on_exit",
      l10n_util::GetStringUTF16(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX));
  localized_strings->SetString("cookies_show_cookies",
      l10n_util::GetStringUTF16(IDS_COOKIES_SHOW_COOKIES_BUTTON));
  localized_strings->SetString("flash_storage_settings",
      l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_SETTINGS));
  localized_strings->SetString("flash_storage_url",
      l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_URL));

  // Image filter.
  localized_strings->SetString("images_tab_label",
      l10n_util::GetStringUTF16(IDS_IMAGES_TAB_LABEL));
  localized_strings->SetString("images_setting",
      l10n_util::GetStringUTF16(IDS_IMAGES_SETTING_LABEL));
  localized_strings->SetString("images_allow",
      l10n_util::GetStringUTF16(IDS_IMAGES_LOAD_RADIO));
  localized_strings->SetString("images_block",
      l10n_util::GetStringUTF16(IDS_IMAGES_NOLOAD_RADIO));

  // JavaScript filter.
  localized_strings->SetString("javascript_tab_label",
      l10n_util::GetStringUTF16(IDS_JAVASCRIPT_TAB_LABEL));
  localized_strings->SetString("javascript_setting",
      l10n_util::GetStringUTF16(IDS_JS_SETTING_LABEL));
  localized_strings->SetString("javascript_allow",
      l10n_util::GetStringUTF16(IDS_JS_ALLOW_RADIO));
  localized_strings->SetString("javascript_block",
      l10n_util::GetStringUTF16(IDS_JS_DONOTALLOW_RADIO));

  // Plug-ins filter.
  localized_strings->SetString("plugins_tab_label",
      l10n_util::GetStringUTF16(IDS_PLUGIN_TAB_LABEL));
  localized_strings->SetString("plugins_setting",
      l10n_util::GetStringUTF16(IDS_PLUGIN_SETTING_LABEL));
  localized_strings->SetString("plugins_allow",
      l10n_util::GetStringUTF16(IDS_PLUGIN_LOAD_RADIO));
  localized_strings->SetString("plugins_block",
      l10n_util::GetStringUTF16(IDS_PLUGIN_NOLOAD_RADIO));
  localized_strings->SetString("disable_individual_plugins",
      l10n_util::GetStringUTF16(IDS_PLUGIN_SELECTIVE_DISABLE));
  localized_strings->SetString("chrome_plugin_url",
      chrome::kChromeUIPluginsURL);

  // Pop-ups filter.
  localized_strings->SetString("popups_tab_label",
      l10n_util::GetStringUTF16(IDS_POPUP_TAB_LABEL));
  localized_strings->SetString("popups_setting",
      l10n_util::GetStringUTF16(IDS_POPUP_SETTING_LABEL));
  localized_strings->SetString("popups_allow",
      l10n_util::GetStringUTF16(IDS_POPUP_ALLOW_RADIO));
  localized_strings->SetString("popups_block",
      l10n_util::GetStringUTF16(IDS_POPUP_BLOCK_RADIO));

  // Location filter.
  localized_strings->SetString("location_tab_label",
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_TAB_LABEL));
  localized_strings->SetString("location_setting",
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_SETTING_LABEL));
  localized_strings->SetString("location_allow",
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_ALLOW_RADIO));
  localized_strings->SetString("location_ask",
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_ASK_RADIO));
  localized_strings->SetString("location_block",
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_BLOCK_RADIO));

  // Notifications filter.
  localized_strings->SetString("notifications_tab_label",
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_TAB_LABEL));
  localized_strings->SetString("notifications_setting",
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_SETTING_LABEL));
  localized_strings->SetString("notifications_allow",
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_ALLOW_RADIO));
  localized_strings->SetString("notifications_ask",
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_ASK_RADIO));
  localized_strings->SetString("notifications_block",
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_BLOCK_RADIO));
}

void ContentSettingsHandler::Initialize() {
  // We send a list of the <input> IDs that should be checked.
  DictionaryValue filter_settings;

  const HostContentSettingsMap* settings_map =
      dom_ui_->GetProfile()->GetHostContentSettingsMap();
  for (int i = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    ContentSetting default_setting = settings_map->
        GetDefaultContentSetting(type);

    filter_settings.SetString(ContentSettingsTypeToGroupName(type),
                              ContentSettingToString(default_setting));
  }

  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.setInitialContentFilterSettingsValue", filter_settings);

  scoped_ptr<Value> bool_value(Value::CreateBooleanValue(
      settings_map->BlockThirdPartyCookies()));
  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.setBlockThirdPartyCookies", *bool_value.get());

  UpdateImagesExceptionsViewFromModel();
  notification_registrar_.Add(
      this, NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<const HostContentSettingsMap>(settings_map));
}

// TODO(estade): generalize this function to work on all content settings types
// rather than just images.
void ContentSettingsHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type != NotificationType::CONTENT_SETTINGS_CHANGED)
    return OptionsPageUIHandler::Observe(type, source, details);

  const ContentSettingsDetails* settings_details =
      static_cast<Details<const ContentSettingsDetails> >(details).ptr();

  if (settings_details->type() == CONTENT_SETTINGS_TYPE_IMAGES ||
      settings_details->update_all_types()) {
    // TODO(estade): we pretend update_all() is always true.
    UpdateImagesExceptionsViewFromModel();
  }
}

// TODO(estade): generalize this function to work on all content settings types
// rather than just images.
void ContentSettingsHandler::UpdateImagesExceptionsViewFromModel() {
  HostContentSettingsMap::SettingsForOneType entries;
  const HostContentSettingsMap* settings_map =
      dom_ui_->GetProfile()->GetHostContentSettingsMap();
  settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES, "", &entries);

  ListValue exceptions;
  for (size_t i = 0; i < entries.size(); ++i) {
    ListValue* exception = new ListValue();
    exception->Append(new StringValue(entries[i].first.AsString()));
    exception->Append(
        new StringValue(ContentSettingToString(entries[i].second)));
    exceptions.Append(exception);
  }

  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.setImagesExceptions", exceptions);
}

void ContentSettingsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("setContentFilter",
      NewCallback(this,
                  &ContentSettingsHandler::SetContentFilter));
  dom_ui_->RegisterMessageCallback("setAllowThirdPartyCookies",
      NewCallback(this,
                  &ContentSettingsHandler::SetAllowThirdPartyCookies));
  dom_ui_->RegisterMessageCallback("removeImageExceptions",
      NewCallback(this,
                  &ContentSettingsHandler::RemoveExceptions));
  dom_ui_->RegisterMessageCallback("setImageException",
      NewCallback(this,
                  &ContentSettingsHandler::SetException));
  dom_ui_->RegisterMessageCallback("checkExceptionPatternValidity",
      NewCallback(this,
                  &ContentSettingsHandler::CheckExceptionPatternValidity));
}

void ContentSettingsHandler::SetContentFilter(const Value* value) {
  const ListValue* list_value = static_cast<const ListValue*>(value);
  DCHECK_EQ(2U, list_value->GetSize());
  std::string group, setting;
  if (!(list_value->GetString(0, &group) &&
        list_value->GetString(1, &setting))) {
    NOTREACHED();
    return;
  }

  dom_ui_->GetProfile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      ContentSettingsTypeFromGroupName(group),
      ContentSettingFromString(setting));
}

void ContentSettingsHandler::SetAllowThirdPartyCookies(const Value* value) {
  std::wstring allow = ExtractStringValue(value);

  dom_ui_->GetProfile()->GetHostContentSettingsMap()->SetBlockThirdPartyCookies(
      allow == L"true");
}

// TODO(estade): generalize this function to work on all content settings types
// rather than just images.
void ContentSettingsHandler::RemoveExceptions(const Value* value) {
  const ListValue* list_value = static_cast<const ListValue*>(value);

  HostContentSettingsMap* settings_map =
      dom_ui_->GetProfile()->GetHostContentSettingsMap();
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    std::string pattern;
    bool rv = list_value->GetString(i, &pattern);
    DCHECK(rv);
    settings_map->SetContentSetting(HostContentSettingsMap::Pattern(pattern),
                                    CONTENT_SETTINGS_TYPE_IMAGES,
                                    "",
                                    CONTENT_SETTING_DEFAULT);
  }
}

// TODO(estade): generalize this function to work on all content settings types
// rather than just images.
void ContentSettingsHandler::SetException(const Value* value) {
  const ListValue* list_value = static_cast<const ListValue*>(value);
  std::string pattern;
  bool rv = list_value->GetString(0, &pattern);
  DCHECK(rv);
  std::string setting;
  rv = list_value->GetString(1, &setting);
  DCHECK(rv);

  HostContentSettingsMap* settings_map =
      dom_ui_->GetProfile()->GetHostContentSettingsMap();
  settings_map->SetContentSetting(HostContentSettingsMap::Pattern(pattern),
                                  CONTENT_SETTINGS_TYPE_IMAGES,
                                  "",
                                  ContentSettingFromString(setting));
}

// TODO(estade): generalize this function to work on all content settings types
// rather than just images.
void ContentSettingsHandler::CheckExceptionPatternValidity(const Value* value) {
  std::string pattern_string = WideToUTF8(ExtractStringValue(value));
  HostContentSettingsMap::Pattern pattern(pattern_string);

  scoped_ptr<Value> pattern_value(Value::CreateStringValue(
      pattern_string));
  scoped_ptr<Value> valid_value(Value::CreateBooleanValue(
      pattern.IsValid()));

  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.patternValidityCheckComplete", *pattern_value.get(),
                                                       *valid_value.get());
}
