// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/menus/button_menu_item_model.h"

#include "app/l10n_util.h"

namespace menus {

ButtonMenuItemModel::ButtonMenuItemModel(
    int string_id,
    ButtonMenuItemModel::Delegate* delegate)
    : item_label_(l10n_util::GetStringUTF16(string_id)),
      delegate_(delegate) {
}

void ButtonMenuItemModel::AddItemWithStringId(int command_id, int string_id) {
  Item item = { command_id, TYPE_BUTTON, l10n_util::GetStringUTF16(string_id),
                -1, -1};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddItemWithStringIdAndGroup(
    int command_id, int string_id, int group) {
  Item item = { command_id, TYPE_BUTTON, l10n_util::GetStringUTF16(string_id),
                -1, group };
  items_.push_back(item);
}

void ButtonMenuItemModel::AddItemWithImage(int command_id,
                                           int icon_idr) {
  Item item = { command_id, TYPE_BUTTON, string16(), icon_idr, -1 };
  items_.push_back(item);
}

void ButtonMenuItemModel::AddButtonLabel(int command_id, int string_id) {
  Item item = { command_id, TYPE_BUTTON_LABEL,
                l10n_util::GetStringUTF16(string_id), -1, -1 };
  items_.push_back(item);
}

void ButtonMenuItemModel::AddSpace() {
  Item item = { 0, TYPE_SPACE, string16(), -1, -1 };
  items_.push_back(item);
}

int ButtonMenuItemModel::GetItemCount() const {
  return static_cast<int>(items_.size());
}

ButtonMenuItemModel::ButtonType ButtonMenuItemModel::GetTypeAt(
    int index) const {
  return items_[index].type;
}

int ButtonMenuItemModel::GetCommandIdAt(int index) const {
  return items_[index].command_id;
}

bool ButtonMenuItemModel::IsLabelDynamicAt(int index) const {
  if (delegate_)
    return delegate_->IsLabelForCommandIdDynamic(GetCommandIdAt(index));
  return false;
}

string16 ButtonMenuItemModel::GetLabelAt(int index) const {
  if (IsLabelDynamicAt(index))
    return delegate_->GetLabelForCommandId(GetCommandIdAt(index));
  return items_[index].label;
}

bool ButtonMenuItemModel::GetIconAt(int index, int* icon_idr) const {
  if (items_[index].icon_idr == -1)
    return false;

  *icon_idr = items_[index].icon_idr;
  return true;
}

bool ButtonMenuItemModel::GetGroupAt(int index, int* group) const {
  if (items_[index].group == -1)
    return false;

  *group = items_[index].group;
  return true;
}

void ButtonMenuItemModel::ActivatedCommand(int command_id) {
  if (delegate_)
    delegate_->ExecuteCommand(command_id);
}

}  // namespace menus
