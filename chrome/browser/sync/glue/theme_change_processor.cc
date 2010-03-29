// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_change_processor.h"

#include "base/logging.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/theme_util.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

namespace {
std::string GetThemeId(Extension* current_theme) {
  if (current_theme) {
    DCHECK(current_theme->IsTheme());
  }
  return current_theme ? current_theme->id() : "default/system";
}
}  // namespace

ThemeChangeProcessor::ThemeChangeProcessor(
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      profile_(NULL) {
  DCHECK(error_handler);
}

ThemeChangeProcessor::~ThemeChangeProcessor() {}

void ThemeChangeProcessor::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(running());
  DCHECK(profile_);
  Extension* extension = Details<Extension>(details).ptr();
  switch (type.value) {
    case NotificationType::BROWSER_THEME_CHANGED:
      // We pay attention to this notification only when it signifies
      // that the user has set the current theme to the system theme or
      // default theme.  If the user set the current theme to a custom
      // theme, the theme isn't actually loaded until after this
      // notification.
      LOG(INFO) << "Got BROWSER_THEME_CHANGED notification for theme "
                << GetThemeId(extension);
      DCHECK_EQ(Source<BrowserThemeProvider>(source).ptr(),
                profile_->GetThemeProvider());
      if (extension != NULL) {
        DCHECK(extension->IsTheme());
        DCHECK_EQ(extension->id(), profile_->GetThemeProvider()->GetThemeID());
        return;
      }
      break;
    case NotificationType::EXTENSION_LOADED:
      // We pay attention to this notification only when it signifies
      // that a theme extension has been loaded because that means that
      // the user set the current theme to a custom theme and it has
      // successfully installed.
      DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
      CHECK(extension);
      if (!extension->IsTheme()) {
        return;
      }
      LOG(INFO) << "Got EXTENSION_LOADED notification for theme "
                << extension->id();
      DCHECK_EQ(extension->id(), profile_->GetThemeProvider()->GetThemeID());
      DCHECK_EQ(extension, profile_->GetTheme());
      break;
    case NotificationType::EXTENSION_UNLOADED:
      // We pay attention to this notification only when it signifies
      // that a theme extension has been unloaded because that means
      // that the user set the current theme to a custom theme and then
      // changed his mind and undid it (reverting to the previous
      // theme).
      DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
      CHECK(extension);
      if (!extension->IsTheme()) {
        return;
      }
      LOG(INFO) << "Got EXTENSION_UNLOADED notification for theme "
                << extension->id();
      extension = profile_->GetTheme();
      break;
    default:
      LOG(DFATAL) << "Unexpected notification received: " << type.value;
      break;
  }

  DCHECK_EQ(extension, profile_->GetTheme());
  if (extension) {
    DCHECK(extension->IsTheme());
  }
  LOG(INFO) << "Theme changed to " << GetThemeId(extension);

  // Here, we know that a theme is being set; the theme is a custom
  // theme iff extension is non-NULL.

  sync_api::WriteTransaction trans(share_handle());
  sync_api::WriteNode node(&trans);
  if (!node.InitByClientTagLookup(syncable::THEMES,
                                  kCurrentThemeClientTag)) {
    LOG(ERROR) << "Could not create node with client tag: "
               << kCurrentThemeClientTag;
    error_handler()->OnUnrecoverableError();
    return;
  }

  sync_pb::ThemeSpecifics old_theme_specifics = node.GetThemeSpecifics();
  // Make sure to base new_theme_specifics on old_theme_specifics so
  // we preserve the state of use_system_theme_by_default.
  sync_pb::ThemeSpecifics new_theme_specifics = old_theme_specifics;
  GetThemeSpecificsFromCurrentTheme(profile_, &new_theme_specifics);
  // Do a write only if something actually changed so as to guard
  // against cycles.
  if (!AreThemeSpecificsEqual(old_theme_specifics, new_theme_specifics)) {
    node.SetThemeSpecifics(new_theme_specifics);
  }
  return;
}

void ThemeChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!running()) {
    return;
  }
  StopObserving();
  ApplyChangesFromSyncModelHelper(trans, changes, change_count);
  StartObserving();
}

void ThemeChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  StartObserving();
}

void ThemeChangeProcessor::StopImpl() {
  StopObserving();
  profile_ = NULL;
}

void ThemeChangeProcessor::ApplyChangesFromSyncModelHelper(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (change_count != 1) {
    LOG(ERROR) << "Unexpected number of theme changes";
    error_handler()->OnUnrecoverableError();
    return;
  }
  const sync_api::SyncManager::ChangeRecord& change = changes[0];
  if (change.action != sync_api::SyncManager::ChangeRecord::ACTION_UPDATE) {
    LOG(ERROR) << "Unexpected change.action " << change.action;
    error_handler()->OnUnrecoverableError();
    return;
  }
  sync_api::ReadNode node(trans);
  if (!node.InitByIdLookup(change.id)) {
    LOG(ERROR) << "Theme node lookup failed";
    error_handler()->OnUnrecoverableError();
    return;
  }
  DCHECK_EQ(node.GetModelType(), syncable::THEMES);
  DCHECK(profile_);
  SetCurrentThemeFromThemeSpecificsIfNecessary(
      node.GetThemeSpecifics(), profile_);
}

void ThemeChangeProcessor::StartObserving() {
  DCHECK(profile_);
  LOG(INFO) << "Observing BROWSER_THEME_CHANGED, EXTENSION_LOADED, "
            << "and EXTENSION_UNLOADED";
  notification_registrar_.Add(
      this, NotificationType::BROWSER_THEME_CHANGED,
      Source<BrowserThemeProvider>(profile_->GetThemeProvider()));
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_LOADED,
      Source<Profile>(profile_));
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_UNLOADED,
      Source<Profile>(profile_));
}

void ThemeChangeProcessor::StopObserving() {
  DCHECK(profile_);
  LOG(INFO) << "Unobserving all notifications";
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
