// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/cookie_prompt_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/cookie_info_view.h"
#include "chrome/browser/views/local_storage_info_view.h"
#include "chrome/browser/views/options/content_settings_window_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/non_client_view.h"

static const int kCookiePromptViewInsetSize = 5;

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, public:

CookiePromptView::CookiePromptView(
    CookiePromptModalDialog* parent,
    gfx::NativeWindow root_window,
    Profile* profile,
    const std::string& host,
    const std::string& cookie_line,
    CookiePromptModalDialogDelegate* delegate)
    : cookie_ui_(true),
      parent_(parent),
      root_window_(root_window),
      profile_(profile),
      cookie_line_(cookie_line),
      delegate_(delegate) {
  InitializeViewResources(host);
}

CookiePromptView::CookiePromptView(
    CookiePromptModalDialog* parent,
    gfx::NativeWindow root_window,
    Profile* profile,
    const BrowsingDataLocalStorageHelper::LocalStorageInfo& storage_info,
    CookiePromptModalDialogDelegate* delegate)
    : cookie_ui_(false),
      parent_(parent),
      root_window_(root_window),
      profile_(profile),
      local_storage_info_(storage_info),
      delegate_(delegate) {
  InitializeViewResources(storage_info.host);
}

CookiePromptView::~CookiePromptView() {
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::View overrides:

gfx::Size CookiePromptView::GetPreferredSize() {
  gfx::Size client_size = views::View::GetPreferredSize();
  return gfx::Size(client_size.width(),
                   client_size.height() + GetExtendedViewHeight());
}


void CookiePromptView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, ModalDialogDelegate implementation:

gfx::NativeWindow CookiePromptView::GetDialogRootWindow() {
  return root_window_;
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::DialogDelegate implementation:

std::wstring CookiePromptView::GetWindowTitle() const {
  return title_;
}

void CookiePromptView::WindowClosing() {
  if (!signaled_ && delegate_)
    delegate_->BlockSiteData(false);
  parent_->CompleteDialog();
}

views::View* CookiePromptView::GetContentsView() {
  return this;
}

// CookieInfoViewDelegate overrides:
void CookiePromptView::ModifyExpireDate(bool session_expire) {
  session_expire_ = session_expire;
}


///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::ButtonListener implementation:

void CookiePromptView::ButtonPressed(views::Button* sender,
                                     const views::Event& event) {
  if (sender == allow_button_) {
    if (delegate_) {
      delegate_->AllowSiteData(remember_radio_->checked(), session_expire_);
      signaled_ = true;
    }
    GetWindow()->Close();
  } else if (sender == block_button_) {
    if (delegate_) {
      delegate_->BlockSiteData(remember_radio_->checked());
      signaled_ = true;
    }
    GetWindow()->Close();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::LinkController implementation:
void CookiePromptView::LinkActivated(views::Link* source, int event_flags) {
  if (source == show_cookie_link_)
    ToggleDetailsViewExpand();
  else if (source == manage_cookies_link_)
    ContentSettingsWindowView::Show(CONTENT_SETTINGS_TYPE_COOKIES, profile_);
  else
    NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, private:

CookiePromptView::CookiePromptView(Profile* profile,
                                   CookiePromptModalDialogDelegate* delegate)
    : remember_radio_(NULL),
      ask_radio_(NULL),
      allow_button_(NULL),
      block_button_(NULL),
      show_cookie_link_(NULL),
      manage_cookies_link_(NULL),
      info_view_(NULL),
      session_expire_(false),
      expanded_view_(false),
      signaled_(false),
      delegate_(delegate),
      profile_(profile) {
}

void CookiePromptView::Init() {
  std::wstring display_host = UTF8ToWide(host_);
  views::Label* description_label = new views::Label(l10n_util::GetStringF(
      cookie_ui_ ? IDS_COOKIE_ALERT_LABEL : IDS_DATA_ALERT_LABEL,
      display_host));
  int radio_group_id = 0;
  remember_radio_ = new views::RadioButton(
      l10n_util::GetStringF(IDS_COOKIE_ALERT_REMEMBER_RADIO, display_host),
      radio_group_id);
  remember_radio_->set_listener(this);
  ask_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIE_ALERT_ASK_RADIO), radio_group_id);
  ask_radio_->set_listener(this);
  allow_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIE_ALERT_ALLOW_BUTTON));
  block_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIE_ALERT_BLOCK_BUTTON));
  show_cookie_link_ = new views::Link(
      l10n_util::GetString(IDS_COOKIE_SHOW_DETAILS_LABEL));
  show_cookie_link_->SetController(this);
  manage_cookies_link_ = new views::Link(
      l10n_util::GetString(IDS_COOKIE_MANAGE_ALERTS_LABEL));
  manage_cookies_link_->SetController(this);

  using views::GridLayout;

  GridLayout* layout = CreatePanelGridLayout(this);
  layout->SetInsets(kCookiePromptViewInsetSize, kCookiePromptViewInsetSize,
                    kCookiePromptViewInsetSize, kCookiePromptViewInsetSize);
  SetLayoutManager(layout);

  const int one_column_layout_id = 0;
  views::ColumnSet* one_column_set = layout->AddColumnSet(one_column_layout_id);
  one_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  one_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
  one_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(description_label);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(remember_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(ask_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  View* button_container = new View();
  GridLayout* button_layout = new GridLayout(button_container);
  button_container->SetLayoutManager(button_layout);
  const int inner_column_layout_id = 1;
  views::ColumnSet* inner_column_set = button_layout->AddColumnSet(
      inner_column_layout_id);
  inner_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                              GridLayout::USE_PREF, 0, 0);
  inner_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  inner_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                              GridLayout::USE_PREF, 0, 0);
  button_layout->StartRow(0, inner_column_layout_id);
  button_layout->AddView(allow_button_);
  button_layout->AddView(block_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  int button_column_layout_id = 2;
  views::ColumnSet* button_column_set =
      layout->AddColumnSet(button_column_layout_id);
  button_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  button_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                               GridLayout::USE_PREF, 0, 0);
  button_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(button_container, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  int link_column_layout_id = 3;
  views::ColumnSet* link_column_set =
      layout->AddColumnSet(link_column_layout_id);
  link_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  link_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                             GridLayout::USE_PREF, 0, 0);
  link_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  link_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                             GridLayout::USE_PREF, 0, 0);
  link_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  layout->StartRow(0, link_column_layout_id);
  layout->AddView(show_cookie_link_);
  layout->AddView(manage_cookies_link_, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, one_column_layout_id);

  if (cookie_ui_) {
    CookieInfoView* cookie_info_view = new CookieInfoView(true);
    cookie_info_view->set_delegate(this);
    layout->AddView(cookie_info_view, 1, 1, GridLayout::FILL,
                    GridLayout::CENTER);

    cookie_info_view->SetCookieString(host_, cookie_line_);
    info_view_ = cookie_info_view;
  } else {
    LocalStorageInfoView* local_storage_info_view = new LocalStorageInfoView();
    layout->AddView(local_storage_info_view, 1, 1, GridLayout::FILL,
                    GridLayout::CENTER);
    local_storage_info_view->SetLocalStorageInfo(local_storage_info_);
    info_view_ = local_storage_info_view;
  }
  info_view_->SetVisible(false);

  // Set default values.
  ask_radio_->SetChecked(true);
}

int CookiePromptView::GetExtendedViewHeight() {
  DCHECK(info_view_);
  return expanded_view_ ?
      kRelatedControlVerticalSpacing : -info_view_->GetPreferredSize().height();
}

void CookiePromptView::ToggleDetailsViewExpand() {
  expanded_view_ = !expanded_view_;
  views::Window* parent = GetWindow();
  gfx::Size non_client_size = parent->GetNonClientView()->GetPreferredSize();
  gfx::Rect bounds = parent->GetBounds();
  bounds.set_height(non_client_size.height() + GetExtendedViewHeight());
  parent->SetBounds(bounds, NULL);

  info_view_->SetVisible(expanded_view_);
  Layout();
}

void CookiePromptView::InitializeViewResources(const std::string& host) {
  DCHECK(host.empty() || host[0] != '.');
  host_ = host;
  title_ = l10n_util::GetStringF(
      cookie_ui_ ? IDS_COOKIE_ALERT_TITLE : IDS_DATA_ALERT_TITLE,
      UTF8ToWide(host_));
}

