// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PageAction) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("page_action")) << message_;

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ASSERT_EQ(1u, service->extensions()->size());
  Extension* extension = service->extensions()->at(0);
  ASSERT_TRUE(extension);

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  const ExtensionActionState* action_state =
      browser()->GetSelectedTabContents()->GetPageActionState(
          extension->page_action());
  ASSERT_TRUE(action_state);
  EXPECT_EQ("Modified", action_state->title());
  EXPECT_EQ("badge", action_state->badge_text());
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action_state->badge_background_color());

  {
    // Simulate the page action being clicked.
    ResultCatcher catcher;
    int tab_id = ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        browser()->profile(), extension->id(), "", tab_id, "", 0);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update2.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  action_state = browser()->GetSelectedTabContents()->GetPageActionState(
      extension->page_action());
  EXPECT_TRUE(action_state->icon());
}
