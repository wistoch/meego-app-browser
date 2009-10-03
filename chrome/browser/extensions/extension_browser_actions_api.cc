// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_actions_api.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/notification_service.h"

namespace extension_browser_actions_api_constants {

const char kSetNameFunction[] = "browserAction.setName";
const char kSetIconFunction[] = "browserAction.setIcon";
const char kSetBadgeTextFunction[] = "browserAction.setBadgeText";
const char kSetBadgeBackgroundColorFunction[] =
    "browserAction.setBadgeBackgroundColor";

}  // namespace extension_browser_actions_api_constants

namespace {
// Errors.
const char kNoBrowserActionError[] =
    "This extension has no browser action specified.";
}

bool BrowserActionSetNameFunction::RunImpl() {
  std::string name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&name));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_title(name);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetIconFunction::RunImpl() {
  int icon_index = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&icon_index));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_icon_index(icon_index);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetBadgeTextFunction::RunImpl() {
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&badge_text));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_badge_text(badge_text);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetBadgeBackgroundColorFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i) {
    EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
  }

  SkColor color = SkColorSetARGB(color_array[0], color_array[1], color_array[2],
                                 color_array[3]);

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_badge_background_color(color);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}
