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
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BrowserAction) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("browser_action")) << message_;

  // Test that there is a browser action in the toolbar.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ExtensionAction* action = extension->browser_action();
  ASSERT_EQ("Modified", action->GetTitle(ExtensionAction::kDefaultTabId));
  ASSERT_EQ("badge", action->GetBadgeText(ExtensionAction::kDefaultTabId));
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Simulate the browser action being clicked.
  ui_test_utils::NavigateToURL(browser(),
      GURL("http://localhost:1337/files/extensions/test_file.txt"));

  int window_id = ExtensionTabUtil::GetWindowId(browser());
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      browser()->profile(), action->extension_id(), browser());

  // Verify the command worked.
  TabContents* tab = browser()->GetSelectedTabContents();
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result);
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action_no_icon")) << message_;

  // Test that there is a browser action in the toolbar and that it has no icon.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());
  ASSERT_TRUE(browser_actions->GetBrowserActionViewAt(0)->button()->icon()
                  .empty());

  // Tell the extension to update the icon using setIcon({imageData:...}).
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ASSERT_FALSE(browser_actions->GetBrowserActionViewAt(0)->button()->icon()
                   .empty());

  // Tell the extension to update using setIcon({path:...});
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ASSERT_FALSE(browser_actions->GetBrowserActionViewAt(0)->button()->icon()
                   .empty());

  // TODO(aa): Would be nice here to actually compare that the pixels change.
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabSpecificBrowserActionState) {
  ASSERT_TRUE(RunExtensionTest("browser_action_tab_specific_state"))
      << message_;

  // Test that there is a browser action in the toolbar and that it has no icon.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());
  ASSERT_FALSE(browser_actions->GetBrowserActionViewAt(0)->button()->icon()
                   .empty());

  // Execute the action, its title should change
  std::wstring text;
  ResultCatcher catcher;
  browser_actions->TestExecuteBrowserAction(0);
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(
      browser_actions->GetBrowserActionViewAt(0)->button()->GetTooltipText(
          0, 0, &text));
  ASSERT_EQ(L"Showing icon 2", text);

  // open a new tab, the title should go back
  browser()->NewTab();
  ASSERT_TRUE(
      browser_actions->GetBrowserActionViewAt(0)->button()->GetTooltipText(
          0, 0, &text));
  ASSERT_EQ(L"hi!", text);

  // go back to first tab, changed title should reappear
  browser()->SelectTabContentsAt(0, true);
  ASSERT_TRUE(
      browser_actions->GetBrowserActionViewAt(0)->button()->GetTooltipText(
          0, 0, &text));
  ASSERT_EQ(L"Showing icon 2", text);

  // reload that tab, default title should come back
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ASSERT_TRUE(
      browser_actions->GetBrowserActionViewAt(0)->button()->GetTooltipText(
          0, 0, &text));
  ASSERT_EQ(L"hi!", text);
}

// This test is disabled. See bug 25746
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_BrowserActionPopup) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("popup")));

  ResultCatcher catcher;
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();

  // This value is in api_test/popup/popup.html.
  const int growFactor = 500;
  ASSERT_GT(ExtensionPopup::kMinHeight + growFactor * 2,
            ExtensionPopup::kMaxHeight);
  ASSERT_GT(ExtensionPopup::kMinWidth + growFactor * 2,
            ExtensionPopup::kMaxWidth);

  // Our initial expected size.
  int width = ExtensionPopup::kMinWidth;
  int height = ExtensionPopup::kMinHeight;

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.  The first one tries to be 0x0, so it should be the min values.
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  gfx::Rect bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);

  // Do it again, and verify the new bigger size (the popup grows each time it's
  // opened).
  width = growFactor;
  height = growFactor;
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);

  // One more time, but this time it should be constrained by the max values.
  width = ExtensionPopup::kMaxWidth;
  height = ExtensionPopup::kMaxHeight;
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);
}
