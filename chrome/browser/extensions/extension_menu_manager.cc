// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_menu_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/json/json_writer.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "webkit/glue/context_menu.h"

ExtensionMenuItem::ExtensionMenuItem(const std::string& extension_id,
                                     std::string title,
                                     bool checked, Type type,
                                     const ContextList& contexts,
                                     const ContextList& enabled_contexts)
    : extension_id_(extension_id),
      title_(title),
      id_(0),
      type_(type),
      checked_(checked),
      contexts_(contexts),
      enabled_contexts_(enabled_contexts),
      parent_id_(0) {}

ExtensionMenuItem::~ExtensionMenuItem() {}

ExtensionMenuItem* ExtensionMenuItem::ChildAt(int index) const {
  if (index < 0 || static_cast<size_t>(index) >= children_.size())
    return NULL;
  return children_[index].get();
}

bool ExtensionMenuItem::RemoveChild(int child_id) {
  ExtensionMenuItem* child = ReleaseChild(child_id, true);
  if (child) {
    delete child;
    return true;
  } else {
    return false;
  }
}

ExtensionMenuItem* ExtensionMenuItem::ReleaseChild(int child_id,
                                                   bool recursive) {
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    ExtensionMenuItem* child = NULL;
    if ((*i)->id() == child_id) {
      child = i->release();
      children_.erase(i);
      return child;
    } else if (recursive) {
      child = (*i)->ReleaseChild(child_id, recursive);
      if (child)
        return child;
    }
  }
  return NULL;
}

std::set<int> ExtensionMenuItem::RemoveAllDescendants() {
  std::set<int> result;
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    ExtensionMenuItem* child = i->get();
    result.insert(child->id());
    std::set<int> removed = child->RemoveAllDescendants();
    result.insert(removed.begin(), removed.end());
  }
  children_.clear();
  return result;
}

string16 ExtensionMenuItem::TitleWithReplacement(
    const string16& selection) const {
  string16 result = UTF8ToUTF16(title_);
  // TODO(asargent) - Change this to properly handle %% escaping so you can
  // put "%s" in titles that won't get substituted.
  ReplaceSubstringsAfterOffset(&result, 0, ASCIIToUTF16("%s"), selection);
  return result;
}

bool ExtensionMenuItem::SetChecked(bool checked) {
  if (type_ != CHECKBOX && type_ != RADIO)
    return false;
  checked_ = checked;
  return true;
}

void ExtensionMenuItem::AddChild(ExtensionMenuItem* item) {
  item->parent_id_ = id_;
  children_.push_back(linked_ptr<ExtensionMenuItem>(item));
}

ExtensionMenuManager::ExtensionMenuManager() : next_item_id_(1) {
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
}

ExtensionMenuManager::~ExtensionMenuManager() {}

std::set<std::string> ExtensionMenuManager::ExtensionIds() {
  std::set<std::string> id_set;
  for (MenuItemMap::const_iterator i = context_items_.begin();
       i != context_items_.end(); ++i) {
    id_set.insert(i->first);
  }
  return id_set;
}

std::vector<const ExtensionMenuItem*> ExtensionMenuManager::MenuItems(
    const std::string& extension_id) {
  std::vector<const ExtensionMenuItem*> result;

  MenuItemMap::iterator i = context_items_.find(extension_id);
  if (i != context_items_.end()) {
    ExtensionMenuItem::List& list = i->second;
    ExtensionMenuItem::List::iterator j;
    for (j = list.begin(); j != list.end(); ++j) {
      result.push_back(j->get());
    }
  }

  return result;
}

int ExtensionMenuManager::AddContextItem(ExtensionMenuItem* item) {
  const std::string& extension_id = item->extension_id();
  // The item must have a non-empty extension id.
  if (extension_id.empty())
    return 0;

  DCHECK_EQ(0, item->id());
  item->set_id(next_item_id_++);

  context_items_[extension_id].push_back(linked_ptr<ExtensionMenuItem>(item));
  items_by_id_[item->id()] = item;

  if (item->type() == ExtensionMenuItem::RADIO && item->checked())
    RadioItemSelected(item);

  return item->id();
}

int ExtensionMenuManager::AddChildItem(int parent_id,
                                       ExtensionMenuItem* child) {
  ExtensionMenuItem* parent = GetItemById(parent_id);
  if (!parent || parent->type() != ExtensionMenuItem::NORMAL ||
      parent->extension_id() != child->extension_id())
    return 0;
  child->set_id(next_item_id_++);
  parent->AddChild(child);
  items_by_id_[child->id()] = child;
  return child->id();
}

bool ExtensionMenuManager::DescendantOf(ExtensionMenuItem* item,
                                        int ancestor_id) {
  DCHECK_GT(ancestor_id, 0);

  // Work our way up the tree until we find the ancestor or 0.
  int id = item->parent_id();
  while (id > 0) {
    DCHECK(id != item->id());  // Catch circular graphs.
    if (id == ancestor_id)
      return true;
    ExtensionMenuItem* next = GetItemById(id);
    if (!next) {
      NOTREACHED();
      return false;
    }
    id = next->parent_id();
  }
  return false;
}

bool ExtensionMenuManager::ChangeParent(int child_id, int parent_id) {
  ExtensionMenuItem* child = GetItemById(child_id);
  ExtensionMenuItem* new_parent = GetItemById(parent_id);
  if (child_id == parent_id || !child || (!new_parent && parent_id > 0) ||
      (new_parent && (DescendantOf(new_parent, child_id) ||
                      child->extension_id() != new_parent->extension_id())))
    return false;

  int old_parent_id = child->parent_id();
  if (old_parent_id > 0) {
    ExtensionMenuItem* old_parent = GetItemById(old_parent_id);
    if (!old_parent) {
      NOTREACHED();
      return false;
    }
    ExtensionMenuItem* taken =
      old_parent->ReleaseChild(child_id, false /* non-recursive search*/);
    DCHECK(taken == child);
  } else {
    // This is a top-level item, so we need to pull it out of our list of
    // top-level items.
    DCHECK_EQ(0, old_parent_id);
    MenuItemMap::iterator i = context_items_.find(child->extension_id());
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    ExtensionMenuItem::List& list = i->second;
    ExtensionMenuItem::List::iterator j = std::find(list.begin(), list.end(),
                                                    child);
    if (j == list.end()) {
      NOTREACHED();
      return false;
    }
    j->release();
    list.erase(j);
  }

  if (new_parent) {
    new_parent->AddChild(child);
  } else {
    context_items_[child->extension_id()].push_back(
        linked_ptr<ExtensionMenuItem>(child));
    child->parent_id_ = 0;
  }
  return true;
}

bool ExtensionMenuManager::RemoveContextMenuItem(int id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  std::string extension_id = GetItemById(id)->extension_id();
  MenuItemMap::iterator i = context_items_.find(extension_id);
  if (i == context_items_.end()) {
    NOTREACHED();
    return false;
  }

  ExtensionMenuItem::List& list = i->second;
  ExtensionMenuItem::List::iterator j;
  for (j = list.begin(); j < list.end(); ++j) {
    // See if the current item is a match, or if one of its children was.
    if ((*j)->id() == id) {
      list.erase(j);
      items_by_id_.erase(id);
      return true;
    } else if ((*j)->RemoveChild(id)) {
      items_by_id_.erase(id);
      return true;
    }
  }
  NOTREACHED();  // The check at the very top should prevent getting here.
  return false;
}

void ExtensionMenuManager::RemoveAllContextItems(std::string extension_id) {
  ExtensionMenuItem::List::iterator i;
  for (i = context_items_[extension_id].begin();
       i != context_items_[extension_id].end(); ++i) {
    ExtensionMenuItem* item = i->get();
    items_by_id_.erase(item->id());

    // Remove descendants from this item and erase them from the lookup cache.
    std::set<int> removed_ids = item->RemoveAllDescendants();
    for (std::set<int>::const_iterator j = removed_ids.begin();
        j != removed_ids.end(); ++j) {
      items_by_id_.erase(*j);
    }
  }
  context_items_.erase(extension_id);
}

ExtensionMenuItem* ExtensionMenuManager::GetItemById(int id) const {
  std::map<int, ExtensionMenuItem*>::const_iterator i = items_by_id_.find(id);
  if (i != items_by_id_.end())
    return i->second;
  else
    return NULL;
}

void ExtensionMenuManager::RadioItemSelected(ExtensionMenuItem* item) {
  // If this is a child item, we need to get a handle to the list from its
  // parent. Otherwise get a handle to the top-level list.
  ExtensionMenuItem::List* list = NULL;
  if (item->parent_id()) {
    ExtensionMenuItem* parent = GetItemById(item->parent_id());
    if (!parent) {
      NOTREACHED();
      return;
    }
    list = parent->children();
  } else {
    if (context_items_.find(item->extension_id()) == context_items_.end()) {
      NOTREACHED();
      return;
    }
    list = &context_items_[item->extension_id()];
  }

  // Find where |item| is in the list.
  ExtensionMenuItem::List::iterator item_location;
  for (item_location = list->begin(); item_location != list->end();
       ++item_location) {
    if (item_location->get() == item)
      break;
  }
  if (item_location == list->end()) {
    NOTREACHED();  // We should have found the item.
    return;
  }

  // Iterate backwards from |item| and uncheck any adjacent radio items.
  ExtensionMenuItem::List::iterator i;
  if (item_location != list->begin()) {
    i = item_location;
    do {
      --i;
      if ((*i)->type() != ExtensionMenuItem::RADIO)
        break;
      (*i)->SetChecked(false);
    } while (i != list->begin());
  }

  // Now iterate forwards from |item| and uncheck any adjacent radio items.
  for (i = item_location + 1; i != list->end(); ++i) {
    if ((*i)->type() != ExtensionMenuItem::RADIO)
      break;
    (*i)->SetChecked(false);
  }
}

static void AddURLProperty(DictionaryValue* dictionary,
                           const std::wstring& key, const GURL& url) {
  if (!url.is_empty())
    dictionary->SetString(key, url.possibly_invalid_spec());
}

void ExtensionMenuManager::GetItemAndIndex(int id, ExtensionMenuItem** item,
                                           size_t* index) {
  for (MenuItemMap::const_iterator i = context_items_.begin();
       i != context_items_.end(); ++i) {
    const ExtensionMenuItem::List& list = i->second;
    for (size_t tmp_index = 0; tmp_index < list.size(); tmp_index++) {
      if (list[tmp_index]->id() == id) {
        if (item)
          *item = list[tmp_index].get();
        if (index)
          *index = tmp_index;
        return;
      }
    }
  }
  if (item)
    *item = NULL;
  if (index)
    *index = 0;
}


void ExtensionMenuManager::ExecuteCommand(Profile* profile,
                                          TabContents* tab_contents,
                                          const ContextMenuParams& params,
                                          int menuItemId) {
  ExtensionMessageService* service = profile->GetExtensionMessageService();
  if (!service)
    return;

  ExtensionMenuItem* item = GetItemById(menuItemId);
  if (!item)
    return;

  if (item->type() == ExtensionMenuItem::RADIO)
    RadioItemSelected(item);

  ListValue args;

  DictionaryValue* properties = new DictionaryValue();
  properties->SetInteger(L"menuItemId", item->id());
  if (item->parent_id())
    properties->SetInteger(L"parentMenuItemId", item->parent_id());

  switch (params.media_type) {
    case WebKit::WebContextMenuData::MediaTypeImage:
      properties->SetString(L"mediaType", "IMAGE");
      break;
    case WebKit::WebContextMenuData::MediaTypeVideo:
      properties->SetString(L"mediaType", "VIDEO");
      break;
    case WebKit::WebContextMenuData::MediaTypeAudio:
      properties->SetString(L"mediaType", "AUDIO");
      break;
    default:  {}  // Do nothing.
  }

  AddURLProperty(properties, L"linkUrl", params.unfiltered_link_url);
  AddURLProperty(properties, L"srcUrl", params.src_url);
  AddURLProperty(properties, L"mainFrameUrl", params.page_url);
  AddURLProperty(properties, L"frameUrl", params.frame_url);

  if (params.selection_text.length() > 0)
    properties->SetString(L"selectionText", params.selection_text);

  properties->SetBoolean(L"editable", params.is_editable);

  args.Append(properties);

  // Add the tab info to the argument list.
  if (tab_contents) {
    args.Append(ExtensionTabUtil::CreateTabValue(tab_contents));
  } else {
    args.Append(new DictionaryValue());
  }

  if (item->type() == ExtensionMenuItem::CHECKBOX ||
      item->type() == ExtensionMenuItem::RADIO) {
    bool was_checked = item->checked();
    properties->SetBoolean(L"wasChecked", was_checked);

    // RADIO items always get set to true when you click on them, but CHECKBOX
    // items get their state toggled.
    bool checked =
        (item->type() == ExtensionMenuItem::RADIO) ? true : !was_checked;

    item->SetChecked(checked);
    properties->SetBoolean(L"checked", item->checked());
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  std::string event_name = "contextMenu/" + item->extension_id();
  service->DispatchEventToRenderers(event_name, json_args,
                                    profile->IsOffTheRecord(), GURL());
}

void ExtensionMenuManager::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Remove menu items for disabled/uninstalled extensions.
  if (type != NotificationType::EXTENSION_UNLOADED) {
    NOTREACHED();
    return;
  }
  Extension* extension = Details<Extension>(details).ptr();
  MenuItemMap::iterator i = context_items_.find(extension->id());
  if (i != context_items_.end()) {
    const ExtensionMenuItem::List& list = i->second;
    ExtensionMenuItem::List::const_iterator j;
    for (j = list.begin(); j != list.end(); ++j)
      items_by_id_.erase((*j)->id());
    context_items_.erase(i);
  }
}
