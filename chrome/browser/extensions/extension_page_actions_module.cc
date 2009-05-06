// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

bool EnablePageActionFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);

  std::string page_action_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(0, &page_action_id));
  DictionaryValue* action;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &action));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(action->GetInteger(L"tabId", &tab_id));
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(action->GetString(L"url", &url));

  // Find the TabContents that contains this tab id.
  TabContents* contents = NULL;
  ExtensionTabUtil::GetTabById(tab_id, profile(), NULL, NULL, &contents, NULL);
  if (!contents)
    return false;

  // Make sure the URL hasn't changed.
  // TODO(finnur): Add an error message here when there is a way to.
  if (url != contents->controller().GetActiveEntry()->url().spec())
    return false;

  // Find our extension.
  Extension* extension = NULL;
  ExtensionsService* service = profile()->GetExtensionsService();
  if (service)
    extension = service->GetExtensionByID(extension_id());
  else
    NOTREACHED();
  if (!extension)
    return false;

  const PageAction* page_action = extension->GetPageAction(page_action_id);
  if (!page_action)
    return false;

  // Set visible and broadcast notifications that the UI should be updated.
  contents->EnablePageAction(page_action);
  contents->NotifyNavigationStateChanged(TabContents::INVALIDATE_PAGE_ACTIONS);

  return true;
}
