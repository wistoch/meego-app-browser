// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/geolocation_exceptions_view.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "gfx/rect.h"
#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

static const int kExceptionsViewInsetSize = 5;
static GeolocationExceptionsView* instance = NULL;

// static
void GeolocationExceptionsView::ShowExceptionsWindow(
    gfx::NativeWindow parent,
    GeolocationContentSettingsMap* map) {
  if (!instance) {
    instance = new GeolocationExceptionsView(map);
    views::Window::CreateChromeWindow(parent, gfx::Rect(), instance);
  }

  // This will show invisible windows and bring visible windows to the front.
  instance->window()->Show();
}

GeolocationExceptionsView::~GeolocationExceptionsView() {
  instance = NULL;
  table_->SetModel(NULL);
}

void GeolocationExceptionsView::OnSelectionChanged() {
  UpdateButtonState();
}

void GeolocationExceptionsView::OnTableViewDelete(
    views::TableView* table_view) {
  Remove();
}

void GeolocationExceptionsView::ButtonPressed(views::Button* sender,
                                              const views::Event& event) {
  switch (sender->tag()) {
    case IDS_EXCEPTIONS_REMOVEALL_BUTTON:
      RemoveAll();
      break;
    case IDS_EXCEPTIONS_REMOVE_BUTTON:
      Remove();
      break;
    default:
      NOTREACHED();
  }
}

void GeolocationExceptionsView::Layout() {
  views::NativeButton* buttons[] = { remove_button_, remove_all_button_ };

  // The buttons are placed in the parent, but we need to lay them out.
  int max_y = GetParent()->GetLocalBounds(false).bottom() - kButtonVEdgeMargin;
  int x = kPanelHorizMargin;

  for (size_t i = 0; i < arraysize(buttons); ++i) {
    gfx::Size pref = buttons[i]->GetPreferredSize();
    buttons[i]->SetBounds(x, max_y - pref.height(), pref.width(),
                          pref.height());
    x += pref.width() + kRelatedControlHorizontalSpacing;
  }

  // Lay out the rest of this view.
  View::Layout();
}

gfx::Size GeolocationExceptionsView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_GEOLOCATION_EXCEPTION_DIALOG_WIDTH_CHARS,
      IDS_GEOLOCATION_EXCEPTION_DIALOG_HEIGHT_LINES));
}

void GeolocationExceptionsView::ViewHierarchyChanged(bool is_add,
                                                     views::View* parent,
                                                     views::View* child) {
  if (is_add && child == this)
    Init();
}

std::wstring GeolocationExceptionsView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_GEOLOCATION_EXCEPTION_TITLE);
}

GeolocationExceptionsView::GeolocationExceptionsView(
    GeolocationContentSettingsMap* map)
    : model_(map),
      table_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL) {
}

void GeolocationExceptionsView::Init() {
  if (table_)
    return;  // We've already Init'd.

  using views::GridLayout;

  std::vector<TableColumn> columns;
  columns.push_back(
      TableColumn(IDS_EXCEPTIONS_HOSTNAME_HEADER, TableColumn::LEFT, -1, .75));
  columns.back().sortable = true;
  columns.push_back(
      TableColumn(IDS_EXCEPTIONS_ACTION_HEADER, TableColumn::LEFT, -1, .25));
  columns.back().sortable = true;
  table_ = new views::TableView(&model_, columns, views::TEXT_ONLY, false, true,
                                false);
  views::TableView::SortDescriptors sort;
  sort.push_back(
      views::TableView::SortDescriptor(IDS_EXCEPTIONS_HOSTNAME_HEADER, true));
  table_->SetSortDescriptors(sort);
  table_->SetObserver(this);

  remove_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_EXCEPTIONS_REMOVE_BUTTON));
  remove_button_->set_tag(IDS_EXCEPTIONS_REMOVE_BUTTON);
  remove_all_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_EXCEPTIONS_REMOVEALL_BUTTON));
  remove_all_button_->set_tag(IDS_EXCEPTIONS_REMOVEALL_BUTTON);

  View* parent = GetParent();
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kExceptionsViewInsetSize, kExceptionsViewInsetSize,
                    kExceptionsViewInsetSize, kExceptionsViewInsetSize);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(table_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  UpdateButtonState();
}

GeolocationContentSettingsTableModel::Rows
    GeolocationExceptionsView::GetSelectedRows() const {
  GeolocationContentSettingsTableModel::Rows rows;
  for (views::TableView::iterator i(table_->SelectionBegin());
       i != table_->SelectionEnd(); ++i)
    rows.insert(*i);
  return rows;
}

void GeolocationExceptionsView::UpdateButtonState() {
  remove_button_->SetEnabled(model_.CanRemoveExceptions(GetSelectedRows()));
  remove_all_button_->SetEnabled(model_.RowCount() > 0);
}

void GeolocationExceptionsView::Remove() {
  model_.RemoveExceptions(GetSelectedRows());
  UpdateButtonState();
}

void GeolocationExceptionsView::RemoveAll() {
  model_.RemoveAll();
  UpdateButtonState();
}
