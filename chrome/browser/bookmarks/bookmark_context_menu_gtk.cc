// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_context_menu.h"

#include "chrome/common/l10n_util.h"

void BookmarkContextMenu::PopupAsContext(guint32 event_time) {
  menu_->PopupAsContext(event_time);
}

void BookmarkContextMenu::CreateMenuObject() {
  menu_.reset(new MenuGtk(this, false));
}

void BookmarkContextMenu::AppendItem(int id) {
  menu_->AppendMenuItemWithLabel(
      id,
      MenuGtk::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(id)));
}

void BookmarkContextMenu::AppendItem(int id, int localization_id) {
  menu_->AppendMenuItemWithLabel(
      id,
      MenuGtk::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(localization_id)));
}

void BookmarkContextMenu::AppendSeparator() {
  menu_->AppendSeparator();
}

void BookmarkContextMenu::AppendCheckboxItem(int id) {
  // TODO(erg): Add support for checkbox items to gtk.
  menu_->AppendMenuItemWithLabel(
      id,
      MenuGtk::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(id)));
}
