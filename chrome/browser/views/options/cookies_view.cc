// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/cookies_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "views/border.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/tree/tree_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"

// static
views::Window* CookiesView::instance_ = NULL;
static const int kCookieInfoViewBorderSize = 1;
static const int kCookieInfoViewInsetSize = 3;
static const int kSearchFilterDelayMs = 500;

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeView
//  Overridden to handle Delete key presses

class CookiesTreeView : public views::TreeView {
 public:
  explicit CookiesTreeView(CookiesTreeModel* cookies_model);
  virtual ~CookiesTreeView() {}

  // Removes the items associated with the selected node in the TreeView
  void RemoveSelectedItems();

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiesTreeView);
};

CookiesTreeView::CookiesTreeView(CookiesTreeModel* cookies_model) {
  SetModel(cookies_model);
  SetRootShown(false);
  SetEditable(false);
}

void CookiesTreeView::RemoveSelectedItems() {
  TreeModelNode* selected_node = GetSelectedNode();
  if (selected_node) {
    static_cast<CookiesTreeModel*>(model())->DeleteCookieNode(
        static_cast<CookieTreeNode*>(GetSelectedNode()));
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView()
    : name_label_(NULL),
      name_value_field_(NULL),
      content_label_(NULL),
      content_value_field_(NULL),
      domain_label_(NULL),
      domain_value_field_(NULL),
      path_label_(NULL),
      path_value_field_(NULL),
      send_for_label_(NULL),
      send_for_value_field_(NULL),
      created_label_(NULL),
      created_value_field_(NULL),
      expires_label_(NULL),
      expires_value_field_(NULL) {
}

CookieInfoView::~CookieInfoView() {
}

void CookieInfoView::SetCookie(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  name_value_field_->SetText(UTF8ToWide(cookie.Name()));
  content_value_field_->SetText(UTF8ToWide(cookie.Value()));
  domain_value_field_->SetText(UTF8ToWide(domain));
  path_value_field_->SetText(UTF8ToWide(cookie.Path()));
  created_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()));

  if (cookie.DoesExpire()) {
    expires_value_field_->SetText(
        base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()));
  } else {
    // TODO(deanm) need a string that the average user can understand
    // "When you quit or restart your browser" ?
    expires_value_field_->SetText(
        l10n_util::GetString(IDS_COOKIES_COOKIE_EXPIRES_SESSION));
  }

  std::wstring sendfor_text;
  if (cookie.IsSecure()) {
    sendfor_text = l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_SECURE);
  } else {
    sendfor_text = l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_ANY);
  }
  send_for_value_field_->SetText(sendfor_text);
  EnableCookieDisplay(true);
}

void CookieInfoView::EnableCookieDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  content_value_field_->SetEnabled(enabled);
  domain_value_field_->SetEnabled(enabled);
  path_value_field_->SetEnabled(enabled);
  send_for_value_field_->SetEnabled(enabled);
  created_value_field_->SetEnabled(enabled);
  expires_value_field_->SetEnabled(enabled);
}

void CookieInfoView::ClearCookieDisplay() {
  std::wstring no_cookie_string =
      l10n_util::GetString(IDS_COOKIES_COOKIE_NONESELECTED);
  name_value_field_->SetText(no_cookie_string);
  content_value_field_->SetText(no_cookie_string);
  domain_value_field_->SetText(no_cookie_string);
  path_value_field_->SetText(no_cookie_string);
  send_for_value_field_->SetText(no_cookie_string);
  created_value_field_->SetText(no_cookie_string);
  expires_value_field_->SetText(no_cookie_string);
  EnableCookieDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, views::View overrides:

void CookieInfoView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, private:

void CookieInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kCookieInfoViewBorderSize, border_color);
  set_border(border);

  name_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_NAME_LABEL));
  name_value_field_ = new views::Textfield;
  content_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_CONTENT_LABEL));
  content_value_field_ = new views::Textfield;
  domain_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_DOMAIN_LABEL));
  domain_value_field_ = new views::Textfield;
  path_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_PATH_LABEL));
  path_value_field_ = new views::Textfield;
  send_for_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_LABEL));
  send_for_value_field_ = new views::Textfield;
  created_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_CREATED_LABEL));
  created_value_field_ = new views::Textfield;
  expires_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_EXPIRES_LABEL));
  expires_value_field_ = new views::Textfield;

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(name_label_);
  layout->AddView(name_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(content_label_);
  layout->AddView(content_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(domain_label_);
  layout->AddView(domain_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(path_label_);
  layout->AddView(path_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(send_for_label_);
  layout->AddView(send_for_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(created_label_);
  layout->AddView(created_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(expires_label_);
  layout->AddView(expires_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  name_value_field_->SetReadOnly(true);
  name_value_field_->RemoveBorder();
  name_value_field_->SetBackgroundColor(text_area_background);
  content_value_field_->SetReadOnly(true);
  content_value_field_->RemoveBorder();
  content_value_field_->SetBackgroundColor(text_area_background);
  domain_value_field_->SetReadOnly(true);
  domain_value_field_->RemoveBorder();
  domain_value_field_->SetBackgroundColor(text_area_background);
  path_value_field_->SetReadOnly(true);
  path_value_field_->RemoveBorder();
  path_value_field_->SetBackgroundColor(text_area_background);
  send_for_value_field_->SetReadOnly(true);
  send_for_value_field_->RemoveBorder();
  send_for_value_field_->SetBackgroundColor(text_area_background);
  created_value_field_->SetReadOnly(true);
  created_value_field_->RemoveBorder();
  created_value_field_->SetBackgroundColor(text_area_background);
  expires_value_field_->SetReadOnly(true);
  expires_value_field_->RemoveBorder();
  expires_value_field_->SetBackgroundColor(text_area_background);
}

///////////////////////////////////////////////////////////////////////////////
// LocalStorageInfoView, public:

LocalStorageInfoView::LocalStorageInfoView()
    : origin_label_(NULL),
      origin_value_field_(NULL),
      size_label_(NULL),
      size_value_field_(NULL),
      last_modified_label_(NULL),
      last_modified_value_field_(NULL) {
}

LocalStorageInfoView::~LocalStorageInfoView() {
}

void LocalStorageInfoView::SetLocalStorageInfo(
    const BrowsingDataLocalStorageHelper::LocalStorageInfo&
    local_storage_info) {
  origin_value_field_->SetText(UTF8ToWide(local_storage_info.origin));
  size_value_field_->SetText(
      FormatBytes(local_storage_info.size,
                  GetByteDisplayUnits(local_storage_info.size),
                  true));
  last_modified_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(local_storage_info.last_modified));
  EnableLocalStorageDisplay(true);
}

void LocalStorageInfoView::EnableLocalStorageDisplay(bool enabled) {
  origin_value_field_->SetEnabled(enabled);
  size_value_field_->SetEnabled(enabled);
  last_modified_value_field_->SetEnabled(enabled);
}

void LocalStorageInfoView::ClearLocalStorageDisplay() {
  std::wstring no_cookie_string =
      l10n_util::GetString(IDS_COOKIES_COOKIE_NONESELECTED);
  origin_value_field_->SetText(no_cookie_string);
  size_value_field_->SetText(no_cookie_string);
  last_modified_value_field_->SetText(no_cookie_string);
  EnableLocalStorageDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// LocalStorageInfoView, views::View overrides:

void LocalStorageInfoView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// LocalStorageInfoView, private:

void LocalStorageInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kCookieInfoViewBorderSize, border_color);
  set_border(border);

  origin_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL));
  origin_value_field_ = new views::Textfield;
  size_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL));
  size_value_field_ = new views::Textfield;
  last_modified_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL));
  last_modified_value_field_ = new views::Textfield;

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(origin_label_);
  layout->AddView(origin_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(size_label_);
  layout->AddView(size_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(last_modified_label_);
  layout->AddView(last_modified_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  origin_value_field_->SetReadOnly(true);
  origin_value_field_->RemoveBorder();
  origin_value_field_->SetBackgroundColor(text_area_background);
  size_value_field_->SetReadOnly(true);
  size_value_field_->RemoveBorder();
  size_value_field_->SetBackgroundColor(text_area_background);
  last_modified_value_field_->SetReadOnly(true);
  last_modified_value_field_->RemoveBorder();
  last_modified_value_field_->SetBackgroundColor(text_area_background);
}


///////////////////////////////////////////////////////////////////////////////
// CookiesView, public:

// static
void CookiesView::ShowCookiesWindow(Profile* profile) {
  if (!instance_) {
    CookiesView* cookies_view = new CookiesView(profile);
    instance_ = views::Window::CreateChromeWindow(
        NULL, gfx::Rect(), cookies_view);
  }
  if (!instance_->IsVisible()) {
    instance_->Show();
  } else {
    instance_->Activate();
  }
}

CookiesView::~CookiesView() {
  cookies_tree_->SetModel(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::Buttonlistener implementation:

void CookiesView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == remove_button_) {
    cookies_tree_->RemoveSelectedItems();
    if (cookies_tree_model_->GetRoot()->GetChildCount() == 0)
      UpdateForEmptyState();
  } else if (sender == remove_all_button_) {
    cookies_tree_model_->DeleteAllCookies();
    UpdateForEmptyState();
  } else if (sender == clear_search_button_) {
    ResetSearchQuery();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::Textfield::Controller implementation:

void CookiesView::ContentsChanged(views::Textfield* sender,
                                  const std::wstring& new_contents) {
  clear_search_button_->SetEnabled(!search_field_->text().empty());
  search_update_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      search_update_factory_.NewRunnableMethod(
          &CookiesView::UpdateSearchResults), kSearchFilterDelayMs);
}

bool CookiesView::HandleKeystroke(views::Textfield* sender,
                                  const views::Textfield::Keystroke& key) {
  if (key.GetKeyboardCode() == base::VKEY_ESCAPE) {
    ResetSearchQuery();
  } else if (key.GetKeyboardCode() == base::VKEY_RETURN) {
    search_update_factory_.RevokeAll();
    UpdateSearchResults();
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::DialogDelegate implementation:

std::wstring CookiesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
}

void CookiesView::WindowClosing() {
  instance_ = NULL;
}

views::View* CookiesView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::View overrides:

void CookiesView::Layout() {
  // Lay out the Remove/Remove All buttons in the parent view.
  gfx::Size ps = remove_button_->GetPreferredSize();
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  int y_buttons = parent_bounds.bottom() - ps.height() - kButtonVEdgeMargin;

  remove_button_->SetBounds(kPanelHorizMargin, y_buttons, ps.width(),
                            ps.height());

  ps = remove_all_button_->GetPreferredSize();
  int remove_all_x = remove_button_->x() + remove_button_->width() +
      kRelatedControlHorizontalSpacing;
  remove_all_button_->SetBounds(remove_all_x, y_buttons, ps.width(),
                                ps.height());

  // Lay out this View
  View::Layout();
}

gfx::Size CookiesView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_COOKIES_DIALOG_WIDTH_CHARS,
      IDS_COOKIES_DIALOG_HEIGHT_LINES));
}

void CookiesView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::TreeViewController overrides:

void CookiesView::OnTreeViewSelectionChanged(views::TreeView* tree_view) {
  CookieTreeNode::DetailedInfo detailed_info =
      static_cast<CookieTreeNode*>(tree_view->GetSelectedNode())->
      GetDetailedInfo();
  if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
    UpdateVisibleDetailedInfo(cookie_info_view_);
    cookie_info_view_->SetCookie(detailed_info.cookie->first,
                                 detailed_info.cookie->second);
  } else if (detailed_info.node_type ==
             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE) {
    UpdateVisibleDetailedInfo(local_storage_info_view_);
    local_storage_info_view_->SetLocalStorageInfo(
        *detailed_info.local_storage_info);
  } else {
    UpdateVisibleDetailedInfo(cookie_info_view_);
    cookie_info_view_->ClearCookieDisplay();
  }
}

void CookiesView::OnTreeViewKeyDown(base::KeyboardCode keycode) {
  if (keycode == base::VKEY_DELETE)
    cookies_tree_->RemoveSelectedItems();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, private:

CookiesView::CookiesView(Profile* profile)
    :
      search_label_(NULL),
      search_field_(NULL),
      clear_search_button_(NULL),
      description_label_(NULL),
      cookies_tree_(NULL),
      cookie_info_view_(NULL),
      local_storage_info_view_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(search_update_factory_(this)) {
}


void CookiesView::UpdateSearchResults() {
  cookies_tree_model_->UpdateSearchResults(search_field_->text());
  remove_button_->SetEnabled(cookies_tree_model_->GetRoot()->
      GetTotalNodeCount() > 1);
  remove_all_button_->SetEnabled(cookies_tree_model_->GetRoot()->
      GetTotalNodeCount() > 1);
}

void CookiesView::Init() {
  search_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_SEARCH_LABEL));
  search_field_ = new views::Textfield;
  search_field_->SetController(this);
  clear_search_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_CLEAR_SEARCH_LABEL));
  clear_search_button_->SetEnabled(false);
  description_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_INFO_LABEL));
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  cookies_tree_model_.reset(new CookiesTreeModel(profile_));
  cookie_info_view_ = new CookieInfoView;
  local_storage_info_view_ = new LocalStorageInfoView;
  cookies_tree_ = new CookiesTreeView(cookies_tree_model_.get());
  remove_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_REMOVE_LABEL));
  remove_all_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_REMOVE_ALL_LABEL));

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int five_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(five_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  const int single_column_layout_id = 1;
  column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, five_column_layout_id);
  layout->AddView(search_label_);
  layout->AddView(search_field_);
  layout->AddView(clear_search_button_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(description_label_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  cookies_tree_->set_lines_at_root(true);
  cookies_tree_->set_auto_expand_children(true);
  layout->AddView(cookies_tree_);

  cookies_tree_->SetController(this);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_layout_id);
  layout->AddView(cookie_info_view_, 1, 2);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(local_storage_info_view_);

  // Add the Remove/Remove All buttons to the ClientView
  View* parent = GetParent();
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);
  if (!cookies_tree_model_.get()->GetRoot()->GetChildCount())
    UpdateForEmptyState();
  else
    UpdateVisibleDetailedInfo(cookie_info_view_);
}

void CookiesView::ResetSearchQuery() {
  search_field_->SetText(std::wstring());
  clear_search_button_->SetEnabled(false);
  UpdateSearchResults();
}

void CookiesView::UpdateForEmptyState() {
  cookie_info_view_->ClearCookieDisplay();
  remove_button_->SetEnabled(false);
  remove_all_button_->SetEnabled(false);
  UpdateVisibleDetailedInfo(cookie_info_view_);
}

void CookiesView::UpdateVisibleDetailedInfo(views::View* view) {
  view->SetVisible(true);
  views::View* other = local_storage_info_view_;
  if (view == local_storage_info_view_) other = cookie_info_view_;
  other->SetVisible(false);
}
