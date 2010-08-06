// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/menus/menu_model.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "webkit/glue/context_menu.h"

using menus::MenuModel;
using WebKit::WebContextMenuData;

// This test class helps us sidestep platform-specific issues with popping up a
// real context menu, while still running through the actual code in
// RenderViewContextMenu where extension items get added and executed.
class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(TabContents* tab_contents,
                            const ContextMenuParams& params)
      : RenderViewContextMenu(tab_contents, params) {}

  virtual ~TestRenderViewContextMenu() {}

  bool HasExtensionItemWithLabel(const std::string& label) {
    string16 label16 = UTF8ToUTF16(label);
    std::map<int, ExtensionMenuItem::Id>::iterator i;
    for (i = extension_item_map_.begin(); i != extension_item_map_.end(); ++i) {
      const ExtensionMenuItem::Id& id = i->second;
      string16 tmp_label;
      EXPECT_TRUE(GetItemLabel(id, &tmp_label));
      if (tmp_label == label16)
        return true;
    }
    return false;
  }

  // Looks in the menu for an extension item with |id|, and if it is found and
  // has a label, that is put in |result| and we return true. Otherwise returns
  // false.
  bool GetItemLabel(const ExtensionMenuItem::Id& id, string16* result) {
    int command_id = 0;
    if (!FindCommandId(id, &command_id))
      return false;

    MenuModel* model = NULL;
    int index = -1;
    if (!GetMenuModelAndItemIndex(command_id, &model, &index)) {
      return false;
    }
    *result = model->GetLabelAt(index);
    return true;
  }

  // Searches for an menu item with |command_id|. If it's found, the return
  // value is true and the model and index where it appears in that model are
  // returned in |found_model| and |found_index|. Otherwise returns false.
  bool GetMenuModelAndItemIndex(int command_id,
                                MenuModel** found_model,
                                int *found_index) {
    std::vector<MenuModel*> models_to_search;
    models_to_search.push_back(&menu_model_);

    while (!models_to_search.empty()) {
      MenuModel* model = models_to_search.back();
      models_to_search.pop_back();
      for (int i = 0; i < model->GetItemCount(); i++) {
        if (model->GetCommandIdAt(i) == command_id) {
          *found_model = model;
          *found_index = i;
          return true;
        } else if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU) {
          models_to_search.push_back(model->GetSubmenuModelAt(i));
        }
      }
    }

    return false;
  }

 protected:
  // These two functions implement pure virtual methods of
  // RenderViewContextMenu.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator) {
    // None of our commands have accelerators, so always return false.
    return false;
  }
  virtual void PlatformInit() {}


  // Given an extension menu item id, tries to find the corresponding command id
  // in the menu.
  bool FindCommandId(const ExtensionMenuItem::Id& id, int* command_id) {
    std::map<int, ExtensionMenuItem::Id>::const_iterator i;
    for (i = extension_item_map_.begin(); i != extension_item_map_.end(); ++i) {
      if (i->second == id) {
        *command_id = i->first;
        return true;
      }
    }
    return false;
  }
};

class ExtensionContextMenuBrowserTest : public ExtensionBrowserTest {
 public:
  // Helper to load an extension from context_menus/|subdirectory| in the
  // extensions test data dir.
  bool LoadContextMenuExtension(std::string subdirectory) {
    FilePath extension_dir =
        test_data_dir_.AppendASCII("context_menus").AppendASCII(subdirectory);
    return LoadExtension(extension_dir);
  }

  // This creates and returns a test menu for a page with |url|.
  TestRenderViewContextMenu* CreateMenuForURL(const GURL& url) {
    TabContents* tab_contents = browser()->GetSelectedTabContents();
    WebContextMenuData data;
    ContextMenuParams params(data);
    params.page_url = url;
    TestRenderViewContextMenu* menu =
        new TestRenderViewContextMenu(tab_contents, params);
    menu->Init();
    return menu;
  }

  // Shortcut to return the current ExtensionMenuManager.
  ExtensionMenuManager* menu_manager() {
    return browser()->profile()->GetExtensionsService()->menu_manager();
  }

  // Returns a pointer to the currently loaded extension with |name|, or null
  // if not found.
  Extension* GetExtensionNamed(std::string name) {
    const ExtensionList* extensions =
        browser()->profile()->GetExtensionsService()->extensions();
    ExtensionList::const_iterator i;
    for (i = extensions->begin(); i != extensions->end(); ++i) {
      if ((*i)->name() == name) {
        return *i;
      }
    }
    return NULL;
  }

  // This gets all the items that any extension has registered for possible
  // inclusion in context menus.
  ExtensionMenuItem::List GetItems() {
    ExtensionMenuItem::List result;
    std::set<std::string> extension_ids = menu_manager()->ExtensionIds();
    std::set<std::string>::iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); ++i) {
      const ExtensionMenuItem::List* list = menu_manager()->MenuItems(*i);
      result.insert(result.end(), list->begin(), list->end());
    }
    return result;
  }

  // This creates a test menu for a page with |url|, looks for an extension item
  // with the given |label|, and returns true if the item was found.
  bool MenuHasItemWithLabel(const GURL& url, const std::string& label) {
    scoped_ptr<TestRenderViewContextMenu> menu(CreateMenuForURL(url));
    return menu->HasExtensionItemWithLabel(label);
  }
};

// Tests adding a simple context menu item.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, Simple) {
  ExtensionTestMessageListener listener1("created item");
  ExtensionTestMessageListener listener2("onclick fired");
  ASSERT_TRUE(LoadContextMenuExtension("simple"));

  // Wait for the extension to tell us it's created an item.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  GURL page_url("http://www.google.com");

  // Create and build our test context menu.
  scoped_ptr<TestRenderViewContextMenu> menu(CreateMenuForURL(page_url));

  // Look for the extension item in the menu, and execute it.
  int command_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id);

  // Wait for the extension's script to tell us its onclick fired.
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that setting "documentUrlPatterns" for an item properly restricts
// those items to matching pages.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, Patterns) {
  ExtensionTestMessageListener listener("created items");

  ASSERT_TRUE(LoadContextMenuExtension("patterns"));

  // Wait for the js test code to create its two items with patterns.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Check that a document url that should match the items' patterns appears.
  GURL google_url("http://www.google.com");
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, std::string("test_item1")));
  ASSERT_TRUE(MenuHasItemWithLabel(google_url, std::string("test_item2")));

  // Now check with a non-matching url.
  GURL test_url("http://www.test.com");
  ASSERT_FALSE(MenuHasItemWithLabel(test_url, std::string("test_item1")));
  ASSERT_FALSE(MenuHasItemWithLabel(test_url, std::string("test_item2")));
}

// Tests registering an item with a very long title that should get truncated in
// the actual menu displayed.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, LongTitle) {
  ExtensionTestMessageListener listener("created");

  // Load the extension and wait until it's created a menu item.
  ASSERT_TRUE(LoadContextMenuExtension("long_title"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Make sure we have an item registered with a long title.
  size_t limit = RenderViewContextMenu::kMaxExtensionItemTitleLength;
  ExtensionMenuItem::List items = GetItems();
  ASSERT_EQ(1u, items.size());
  ExtensionMenuItem* item = items.at(0);
  ASSERT_GT(item->title().size(), limit);

  // Create a context menu, then find the item's label. It should be properly
  // truncated.
  GURL url("http://foo.com/");
  scoped_ptr<TestRenderViewContextMenu> menu(CreateMenuForURL(url));

  string16 label;
  ASSERT_TRUE(menu->GetItemLabel(item->id(), &label));
  ASSERT_TRUE(label.size() <= limit);
}

// Checks that in |menu|, the item at |index| has type |expected_type| and a
// label of |expected_label|.
static void ExpectLabelAndType(const char* expected_label,
                               MenuModel::ItemType expected_type,
                               const MenuModel& menu,
                               int index) {
  EXPECT_EQ(expected_type, menu.GetTypeAt(index));
  EXPECT_EQ(UTF8ToUTF16(expected_label), menu.GetLabelAt(index));
}

// In the separators test we build a submenu with items and separators in two
// different ways - this is used to verify the results in both cases.
static void VerifyMenuForSeparatorsTest(const MenuModel& menu) {
  // We expect to see the following items in the menu:
  //  radio1
  //  radio2
  //  --separator-- (automatically added)
  //  normal1
  //  --separator--
  //  normal2
  //  --separator--
  //  radio3
  //  radio4
  //  --separator--
  //  normal3

  int index = 0;
  ASSERT_EQ(11, menu.GetItemCount());
  ExpectLabelAndType("radio1", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio2", MenuModel::TYPE_RADIO, menu, index++);
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
  ExpectLabelAndType("normal1", MenuModel::TYPE_COMMAND, menu, index++);
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
  ExpectLabelAndType("normal2", MenuModel::TYPE_COMMAND, menu, index++);
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
  ExpectLabelAndType("radio3", MenuModel::TYPE_RADIO, menu, index++);
  ExpectLabelAndType("radio4", MenuModel::TYPE_RADIO, menu, index++);
  EXPECT_EQ(MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(index++));
  ExpectLabelAndType("normal3", MenuModel::TYPE_COMMAND, menu, index++);
}

// Tests a number of cases for auto-generated and explicitly added separators.
IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, Separators) {
  // Load the extension.
  ASSERT_TRUE(LoadContextMenuExtension("separators"));
  Extension* extension = GetExtensionNamed("Separators Test");
  ASSERT_TRUE(extension != NULL);

  // Navigate to test1.html inside the extension, which should create a bunch
  // of items at the top-level (but they'll get pushed into an auto-generated
  // parent).
  ExtensionTestMessageListener listener1("test1 create finished");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("test1.html")));
  listener1.WaitUntilSatisfied();

  GURL url("http://www.google.com/");
  scoped_ptr<TestRenderViewContextMenu> menu(CreateMenuForURL(url));

  // The top-level item should be an "automagic parent" with the extension's
  // name.
  MenuModel* model = NULL;
  int index = 0;
  string16 label;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST, &model, &index));
  EXPECT_EQ(UTF8ToUTF16(extension->name()), model->GetLabelAt(index));
  ASSERT_EQ(MenuModel::TYPE_SUBMENU, model->GetTypeAt(index));

  // Get the submenu and verify the items there.
  MenuModel* submenu = model->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu != NULL);
  VerifyMenuForSeparatorsTest(*submenu);

  // Now run our second test - navigate to test2.html which creates an explicit
  // parent node and populates that with the same items as in test1.
  ExtensionTestMessageListener listener2("test2 create finished");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("test2.html")));
  listener2.WaitUntilSatisfied();
  menu.reset(CreateMenuForURL(url));
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(
      IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST, &model, &index));
  EXPECT_EQ(UTF8ToUTF16("parent"), model->GetLabelAt(index));
  submenu = model->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu != NULL);
  VerifyMenuForSeparatorsTest(*submenu);
}
