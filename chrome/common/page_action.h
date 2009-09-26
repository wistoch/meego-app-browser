// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_ACTION_H_
#define CHROME_COMMON_PAGE_ACTION_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"

class ContextualAction {
 public:
  ContextualAction();
  virtual ~ContextualAction();

  typedef enum {
    PAGE_ACTION = 0,
    BROWSER_ACTION = 1,
  } ContextualActionType;

  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  ContextualActionType type() const { return type_; }
  void set_type(ContextualActionType type) { type_ = type; }

  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  std::string name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  const std::vector<std::string>& icon_paths() const { return icon_paths_; }
  void AddIconPath(const std::string& icon_path) {
    icon_paths_.push_back(icon_path);
  }

 private:
  // The id for the ContextualAction, for example: "RssPageAction".
  // For BrowserActions this is blank.
  std::string id_;

  // The type of the ContextualAction, either PageAction or BrowserAction.
  ContextualActionType type_;

  // The id for the extension this ContextualAction belongs to (as defined in
  // the extension manifest).
  std::string extension_id_;

  // The name of the ContextualAction.
  std::string name_;

  // The paths to the icons that this PageIcon can show.
  std::vector<std::string> icon_paths_;
};

typedef std::map<std::string, ContextualAction*> ContextualActionMap;

// This class keeps track of what values each tab uses to override the default
// values of the ContextualAction.
class ContextualActionState {
 public:
  ContextualActionState(std::string title, int icon_index)
    : title_(title), icon_index_(icon_index) {
  }

  std::string title() const { return title_; }
  int icon_index() const { return icon_index_; }

 private:
  // The title to use.
  std::string title_;

  // The icon to use.
  int icon_index_;

  DISALLOW_COPY_AND_ASSIGN(ContextualActionState);
};

#endif  // CHROME_COMMON_PAGE_ACTION_H_
