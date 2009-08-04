// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/tabbed_pane/tabbed_pane.h"

#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"

namespace views {

// static
const char TabbedPane::kViewClassName[] = "views/TabbedPane";

TabbedPane::TabbedPane() : native_tabbed_pane_(NULL), listener_(NULL) {
  SetFocusable(true);
}

TabbedPane::~TabbedPane() {
}

void TabbedPane::SetListener(Listener* listener) {
  listener_ = listener;
}

void TabbedPane::AddTab(const std::wstring& title, View* contents) {
  native_tabbed_pane_->AddTab(title, contents);
}

void TabbedPane::AddTabAtIndex(int index,
                               const std::wstring& title,
                               View* contents,
                               bool select_if_first_tab) {
  native_tabbed_pane_->AddTabAtIndex(index, title, contents,
                                     select_if_first_tab);
}

int TabbedPane::GetSelectedTabIndex() {
  return native_tabbed_pane_->GetSelectedTabIndex();
}

View* TabbedPane::GetSelectedTab() {
  return native_tabbed_pane_->GetSelectedTab();
}

View* TabbedPane::RemoveTabAtIndex(int index) {
  return native_tabbed_pane_->RemoveTabAtIndex(index);
}

void TabbedPane::SelectTabAt(int index) {
   native_tabbed_pane_->SelectTabAt(index);
}

int TabbedPane::GetTabCount() {
  return native_tabbed_pane_->GetTabCount();
}

void TabbedPane::CreateWrapper() {
  native_tabbed_pane_ = NativeTabbedPaneWrapper::CreateNativeWrapper(this);
}

// View overrides:
std::string TabbedPane::GetClassName() const {
  return kViewClassName;
}

void TabbedPane::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_tabbed_pane_ && GetWidget()) {
    CreateWrapper();
    AddChildView(native_tabbed_pane_->GetView());
    LoadAccelerators();
  }
}

bool TabbedPane::AcceleratorPressed(const views::Accelerator& accelerator) {
  // We only accept Ctrl+Tab keyboard events.
  DCHECK(accelerator.GetKeyCode() ==
      base::VKEY_TAB && accelerator.IsCtrlDown());

  int tab_count = GetTabCount();
  if (tab_count <= 1)
    return false;
  int selected_tab_index = GetSelectedTabIndex();
  int next_tab_index = accelerator.IsShiftDown() ?
      (selected_tab_index - 1) % tab_count :
      (selected_tab_index + 1) % tab_count;
  // Wrap around.
  if (next_tab_index < 0)
    next_tab_index += tab_count;
  SelectTabAt(next_tab_index);
  return true;
}

void TabbedPane::LoadAccelerators() {
  // Ctrl+Shift+Tab
  AddAccelerator(views::Accelerator(base::VKEY_TAB, true, true, false));
  // Ctrl+Tab
  AddAccelerator(views::Accelerator(base::VKEY_TAB, false, true, false));
}

void TabbedPane::Layout() {
  if (native_tabbed_pane_) {
    native_tabbed_pane_->GetView()->SetBounds(0, 0, width(), height());
    native_tabbed_pane_->GetView()->Layout();
  }
}

void TabbedPane::Focus() {
  // Forward the focus to the wrapper.
  if (native_tabbed_pane_)
    native_tabbed_pane_->SetFocus();
  else
    View::Focus();  // Will focus the RootView window (so we still get keyboard
                    // messages).
}

}  // namespace views
