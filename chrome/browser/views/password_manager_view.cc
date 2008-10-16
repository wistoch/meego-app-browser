// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/background.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_button.h"
#include "webkit/glue/password_form.h"

#include "generated_resources.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;

// We can only have one PasswordManagerView at a time.
static PasswordManagerView* instance_ = NULL;

static const int kDefaultWindowWidth = 530;
static const int kDefaultWindowHeight = 240;

////////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
//
MultiLabelButtons::MultiLabelButtons(const std::wstring& label,
                                     const std::wstring& alt_label)
    : NativeButton(label),
      label_(label),
      alt_label_(alt_label),
      pref_size_(-1, -1) {
}

gfx::Size MultiLabelButtons::GetPreferredSize() {
  if (pref_size_.width() == -1 && pref_size_.height() == -1) {
    // Let's compute our preferred size.
    std::wstring current_label = GetLabel();
    SetLabel(label_);
    pref_size_ = NativeButton::GetPreferredSize();
    SetLabel(alt_label_);
    gfx::Size alt_pref_size = NativeButton::GetPreferredSize();
    // Revert to the original label.
    SetLabel(current_label);
    pref_size_.SetSize(std::max(pref_size_.width(), alt_pref_size.width()),
                       std::max(pref_size_.height(), alt_pref_size.height()));
  }
  return gfx::Size(pref_size_.width(), pref_size_.height());
}

////////////////////////////////////////////////////////////////////
// PasswordManagerTableModel::PasswordRow
PasswordManagerTableModel::PasswordRow::~PasswordRow() {
  delete form;
}

////////////////////////////////////////////////////////////////////
// PasswordManagerTableModel
PasswordManagerTableModel::PasswordManagerTableModel(Profile* profile)
    : observer_(NULL),
      pending_login_query_(NULL),
      profile_(profile) {
  DCHECK(profile && profile->GetWebDataService(Profile::EXPLICIT_ACCESS));
}

PasswordManagerTableModel::~PasswordManagerTableModel() {
  CancelLoginsQuery();
}

int PasswordManagerTableModel::RowCount() {
  return static_cast<int>(saved_signons_.size());
}

std::wstring PasswordManagerTableModel::GetText(int row,
                                                int col_id) {
  switch (col_id) {
    case IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN:  // Site.
      return saved_signons_[row].display_url.display_url();
    case IDS_PASSWORD_MANAGER_VIEW_USERNAME_COLUMN:  // Username.
      return GetPasswordFormAt(row)->username_value;
    default:
      NOTREACHED() << "Invalid column.";
      return std::wstring();
  }
}

int PasswordManagerTableModel::CompareValues(int row1, int row2,
                                             int column_id) {
  if (column_id == IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN) {
    return saved_signons_[row1].display_url.Compare(
        saved_signons_[row2].display_url, GetCollator());
  }
  return TableModel::CompareValues(row1, row2, column_id);
}

void PasswordManagerTableModel::SetObserver(
    ChromeViews::TableModelObserver* observer) {
  observer_ = observer;
}

void PasswordManagerTableModel::GetAllSavedLoginsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = web_data_service()->GetAllAutofillableLogins(this);
}

void PasswordManagerTableModel::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK_EQ(pending_login_query_, h);
  pending_login_query_ = NULL;

  if (!result)
    return;

  DCHECK(result->GetType() == PASSWORD_RESULT);

  // Get the result from the database into a useable form.
  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);
  std::vector<PasswordForm*> rows = r->GetValue();
  saved_signons_.clear();
  saved_signons_.resize(rows.size());
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  for (size_t i = 0; i < rows.size(); ++i) {
    saved_signons_[i].form = rows[i];
    saved_signons_[i].display_url =
        gfx::SortedDisplayURL(rows[i]->origin, languages);
  }
  if (observer_)
    observer_->OnModelChanged();
}

void PasswordManagerTableModel::CancelLoginsQuery() {
  if (pending_login_query_) {
    web_data_service()->CancelRequest(pending_login_query_);
    pending_login_query_ = NULL;
  }
}

PasswordForm* PasswordManagerTableModel::GetPasswordFormAt(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return saved_signons_[row].form;
}

void PasswordManagerTableModel::ForgetAndRemoveSignon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  PasswordRows::iterator target_iter = saved_signons_.begin() + row;
  // Remove from DB, memory, and vector.
  web_data_service()->RemoveLogin(*(target_iter->form));
  saved_signons_.erase(target_iter);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void PasswordManagerTableModel::ForgetAndRemoveAllSignons() {
  PasswordRows::iterator iter = saved_signons_.begin();
  while (iter != saved_signons_.end()) {
    web_data_service()->RemoveLogin(*(iter->form));
    iter = saved_signons_.erase(iter);
  }
  if (observer_)
    observer_->OnModelChanged();
}

//////////////////////////////////////////////////////////////////////
// PasswordManagerView

// static
void PasswordManagerView::Show(Profile* profile) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new PasswordManagerView(profile);

    // manager is owned by the dialog window, so Close() will delete it.
    ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }
}

PasswordManagerView::PasswordManagerView(Profile* profile)
    : show_button_(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON),
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_HIDE_BUTTON)),
      remove_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_VIEW_REMOVE_BUTTON)),
      remove_all_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_VIEW_REMOVE_ALL_BUTTON)),
      table_model_(profile) {
  Init();
}

void PasswordManagerView::SetupTable() {
  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<ChromeViews::TableColumn> columns;
  columns.push_back(
      ChromeViews::TableColumn(
          IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN,
          ChromeViews::TableColumn::LEFT, -1, 0.55f));
  columns.back().sortable = true;
  columns.push_back(
      ChromeViews::TableColumn(
          IDS_PASSWORD_MANAGER_VIEW_USERNAME_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0.37f));
  columns.back().sortable = true;
  table_view_ = new ChromeViews::TableView(&table_model_,
                                           columns,
                                           ChromeViews::TEXT_ONLY,
                                           true, true, true);
  // Make the table initially sorted by host.
  ChromeViews::TableView::SortDescriptors sort;
  sort.push_back(
      ChromeViews::TableView::SortDescriptor(
          IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN, true));
  table_view_->SetSortDescriptors(sort);
  table_view_->SetObserver(this);
}

void PasswordManagerView::SetupButtonsAndLabels() {
  // Tell View not to delete class stack allocated views.

  show_button_.SetParentOwned(false);
  show_button_.SetListener(this);
  show_button_.SetEnabled(false);

  remove_button_.SetParentOwned(false);
  remove_button_.SetListener(this);
  remove_button_.SetEnabled(false);

  remove_all_button_.SetParentOwned(false);
  remove_all_button_.SetListener(this);

  password_label_.SetParentOwned(false);
}

void PasswordManagerView::Init() {
  // Configure the background and view elements (buttons, labels, table).
  SetupButtonsAndLabels();
  SetupTable();

  // Do the layout thing.
  const int top_column_set_id = 0;
  const int lower_column_set_id = 1;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(top_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 300, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(lower_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(0, 2, -1);

  // Fill the grid.
  layout->StartRow(0.05f, top_column_set_id);
  layout->AddView(table_view_, 1, 3);
  layout->AddView(&remove_button_);
  layout->StartRow(0.05f, top_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(&show_button_);
  layout->StartRow(0.80f, top_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(&password_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Ask the database for saved password data.
  table_model_.GetAllSavedLoginsForProfile();
}

PasswordManagerView::~PasswordManagerView() {
}

void PasswordManagerView::Layout() {
  GetLayoutManager()->Layout(this);

  // Manually lay out the Remove All button in the same row as
  // the close button.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  gfx::Size prefsize = remove_all_button_.GetPreferredSize();
  int button_y =
      parent_bounds.bottom() - prefsize.height() - kButtonVEdgeMargin;
  remove_all_button_.SetBounds(kPanelHorizMargin, button_y, prefsize.width(),
                               prefsize.height());
}

gfx::Size PasswordManagerView::GetPreferredSize() {
  return gfx::Size(kDefaultWindowWidth, kDefaultWindowHeight);
}

void PasswordManagerView::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  if (child == this) {
    // Add and remove the Remove All button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(&remove_all_button_);
    } else {
      parent->RemoveChildView(&remove_all_button_);
    }
  }
}

void PasswordManagerView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);
  // Reset the password related views.
  show_button_.SetLabel(
      l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON));
  show_button_.SetEnabled(has_selection);
  password_label_.SetText(std::wstring());
}

int PasswordManagerView::GetDialogButtons() const {
  return DIALOGBUTTON_CANCEL;
}

bool PasswordManagerView::CanResize() const {
  return true;
}

bool PasswordManagerView::CanMaximize() const {
  return false;
}

bool PasswordManagerView::IsAlwaysOnTop() const {
  return false;
}

bool PasswordManagerView::HasAlwaysOnTopMenu() const {
  return false;
}

std::wstring PasswordManagerView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_TITLE);
}

void PasswordManagerView::ButtonPressed(ChromeViews::NativeButton* sender) {
  DCHECK(window());
  // Close will result in our destruction.
  if (sender == &remove_all_button_) {
    table_model_.ForgetAndRemoveAllSignons();
    return;
  }

  // The following require a selection (and only one, since table is single-
  // select only).
  ChromeViews::TableSelectionIterator iter = table_view_->SelectionBegin();
  int row = *iter;
  PasswordForm* selected = table_model_.GetPasswordFormAt(row);
  DCHECK(++iter == table_view_->SelectionEnd());

  if (sender == &remove_button_) {
    table_model_.ForgetAndRemoveSignon(row);
  } else if (sender == &show_button_) {
    if (password_label_.GetText().length() == 0) {
      password_label_.SetText(selected->password_value);
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_HIDE_BUTTON));
    } else {
      password_label_.SetText(L"");
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON));
    }
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void PasswordManagerView::WindowClosing() {
  // The table model will be deleted before the table view, so detach it.
  table_view_->SetModel(NULL);

  // Clear the static instance so the next time Show() is called, a new
  // instance is created.
  instance_ = NULL;
}

ChromeViews::View* PasswordManagerView::GetContentsView() {
  return this;
}
