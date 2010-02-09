// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/content_blocked_bubble_contents.h"

#include "app/l10n_util.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

ContentBlockedBubbleContents::ContentBlockedBubbleContents(
    ContentSettingsType content_type,
    const std::string& host,
    const std::wstring& display_host,
    Profile* profile,
    TabContents* tab_contents)
    : content_type_(content_type),
      host_(host),
      display_host_(display_host),
      profile_(profile),
      tab_contents_(tab_contents),
      info_bubble_(NULL),
      allow_radio_(NULL),
      block_radio_(NULL),
      close_button_(NULL),
      manage_link_(NULL) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentBlockedBubbleContents::~ContentBlockedBubbleContents() {
}

void ContentBlockedBubbleContents::ViewHierarchyChanged(bool is_add,
                                                        View* parent,
                                                        View* child) {
  if (is_add && (child == this))
    InitControlLayout();
}

void ContentBlockedBubbleContents::ButtonPressed(views::Button* sender,
                                                 const views::Event& event) {
  if (sender == close_button_) {
    info_bubble_->Close();  // CAREFUL: This deletes us.
    return;
  }

  DCHECK((sender == allow_radio_) || (sender == block_radio_));
  profile_->GetHostContentSettingsMap()->SetContentSetting(host_, content_type_,
      allow_radio_->checked() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

void ContentBlockedBubbleContents::LinkActivated(views::Link* source,
                                                 int event_flags) {
  if (source == manage_link_) {
    if (tab_contents_)
      tab_contents_->delegate()->ShowContentSettingsWindow(content_type_);
    else
      browser::ShowContentSettingsWindow(NULL, content_type_, profile_);
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  PopupLinks::const_iterator i(popup_links_.find(source));
  DCHECK(i != popup_links_.end());
  if (tab_contents_ && tab_contents_->blocked_popup_container())
    tab_contents_->blocked_popup_container()->LaunchPopupForContents(i->second);
}

void ContentBlockedBubbleContents::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}

void ContentBlockedBubbleContents::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  static const int kTitleIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    IDS_BLOCKED_COOKIES_TITLE,
    IDS_BLOCKED_IMAGES_TITLE,
    IDS_BLOCKED_JAVASCRIPT_TITLE,
    IDS_BLOCKED_PLUGINS_TITLE,
    IDS_BLOCKED_POPUPS_TITLE,
  };
  DCHECK_EQ(arraysize(kTitleIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  views::Label* title_label =
      new views::Label(l10n_util::GetString(kTitleIDs[content_type_]));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (content_type_ == CONTENT_SETTINGS_TYPE_POPUPS) {
    BlockedPopupContainer::BlockedContents blocked_contents;
    DCHECK(tab_contents_->blocked_popup_container());
    tab_contents_->blocked_popup_container()->GetBlockedContents(
        &blocked_contents);
    for (BlockedPopupContainer::BlockedContents::const_iterator
         i(blocked_contents.begin()); i != blocked_contents.end(); ++i) {
      views::Link* link = new views::Link(UTF16ToWideHack((*i)->GetTitle()));
      link->SetController(this);
      popup_links_[link] = *i;
      if (i != blocked_contents.begin())
        layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(link);
    }
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  if (content_type_ != CONTENT_SETTINGS_TYPE_COOKIES) {
    static const int kAllowIDs[CONTENT_SETTINGS_NUM_TYPES] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_UNBLOCK,
      IDS_BLOCKED_JAVASCRIPT_UNBLOCK,
      IDS_BLOCKED_PLUGINS_UNBLOCK,
      IDS_BLOCKED_POPUPS_UNBLOCK,
    };
    DCHECK_EQ(arraysize(kAllowIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    const int radio_button_group = 0;
    allow_radio_ = new views::RadioButton(
        l10n_util::GetStringF(kAllowIDs[content_type_], display_host_),
        radio_button_group);
    allow_radio_->set_listener(this);

    static const int kBlockIDs[CONTENT_SETTINGS_NUM_TYPES] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_NO_ACTION,
      IDS_BLOCKED_JAVASCRIPT_NO_ACTION,
      IDS_BLOCKED_PLUGINS_NO_ACTION,
      IDS_BLOCKED_POPUPS_NO_ACTION,
    };
    DCHECK_EQ(arraysize(kBlockIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    block_radio_ = new views::RadioButton(
        l10n_util::GetString(kBlockIDs[content_type_]), radio_button_group);
    block_radio_->set_listener(this);

    layout->StartRow(0, single_column_set_id);
    layout->AddView(allow_radio_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, single_column_set_id);
    layout->AddView(block_radio_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    // Now that the buttons have been added to the view hierarchy, it's safe to
    // call SetChecked() on them.
    if (profile_->GetHostContentSettingsMap()->GetContentSetting(host_,
            content_type_) == CONTENT_SETTING_ALLOW)
      allow_radio_->SetChecked(true);
    else
      block_radio_->SetChecked(true);

    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator, 1, 1, GridLayout::FILL, GridLayout::FILL);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  const int double_column_set_id = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(double_column_set_id);
  double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  static const int kLinkIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    IDS_BLOCKED_COOKIES_LINK,
    IDS_BLOCKED_IMAGES_LINK,
    IDS_BLOCKED_JAVASCRIPT_LINK,
    IDS_BLOCKED_PLUGINS_LINK,
    IDS_BLOCKED_POPUPS_LINK,
  };
  DCHECK_EQ(arraysize(kLinkIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  manage_link_ = new views::Link(l10n_util::GetString(kLinkIDs[content_type_]));
  manage_link_->SetController(this);

  layout->StartRow(0, double_column_set_id);
  layout->AddView(manage_link_);

  close_button_ =
      new views::NativeButton(this, l10n_util::GetString(IDS_CLOSE));
  layout->AddView(close_button_);
}
