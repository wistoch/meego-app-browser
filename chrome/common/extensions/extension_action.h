// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkColor.h"

class ExtensionAction {
 public:
  ExtensionAction();
  virtual ~ExtensionAction();

  typedef enum {
    PAGE_ACTION = 0,
    BROWSER_ACTION = 1,
  } ExtensionActionType;

  int command_id() const { return command_id_; }

  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  ExtensionActionType type() const { return type_; }
  void set_type(ExtensionActionType type) { type_ = type; }

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

  const GURL& popup_url() const { return popup_url_; }
  void set_popup_url(const GURL& url) { popup_url_ = url; }

  const int popup_height() const { return popup_height_; }
  void set_popup_height(int height) { popup_height_ = height; }

 private:
  static int next_command_id_;

  // The id for the ExtensionAction, for example: "RssPageAction".
  // For BrowserActions this is blank.
  std::string id_;

  // The type of the ExtensionAction, either PageAction or BrowserAction.
  ExtensionActionType type_;

  // The id for the extension this ExtensionAction belongs to (as defined in
  // the extension manifest).
  std::string extension_id_;

  // The name of the ExtensionAction.
  std::string name_;

  // The paths to the icons that this PageIcon can show.
  std::vector<std::string> icon_paths_;

  // An integer for use with the browser's command system. These should always
  // be in the range [IDC_BROWSER_ACTION_FIRST, IDC_BROWSER_ACTION_LAST].
  int command_id_;

  // If the action has a popup, it has a URL and a height.
  GURL popup_url_;
  int popup_height_;
};

typedef std::map<std::string, ExtensionAction*> ExtensionActionMap;

// This class keeps track of what values each tab uses to override the default
// values of the ExtensionAction.
class ExtensionActionState {
 public:
  ExtensionActionState(std::string title, int icon_index)
    : title_(title), icon_index_(icon_index),
      badge_background_color_(new SkColor(SkColorSetARGB(255, 218, 0, 24))) {
  }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const std::string& badge_text() const { return badge_text_; }
  void set_badge_text(const std::string& badge_text) {
    badge_text_ = badge_text;
  }

  SkColor* badge_background_color() const {
    return badge_background_color_.get();
  }
  void set_badge_background_color(const SkColor& badge_background_color) {
    badge_background_color_.reset(new SkColor(badge_background_color));
  }

  int icon_index() const { return icon_index_; }
  void set_icon_index(int icon_index) { icon_index_ = icon_index; }

 private:
  // The title to use.
  std::string title_;

  // The icon to use.
  int icon_index_;

  // The badge text.
  std::string badge_text_;

  // The background color for the badge.
  scoped_ptr<SkColor> badge_background_color_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionState);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
