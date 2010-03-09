// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/content_blocked_bubble_contents.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include "app/l10n_util.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/content_setting_bubble_model.h"
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
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

class ContentSettingBubbleContents::Favicon : public views::ImageView {
 public:
  Favicon(const SkBitmap& image,
          ContentSettingBubbleContents* parent,
          views::Link* link);
  virtual ~Favicon();

 private:
#if defined(OS_WIN)
  static HCURSOR g_hand_cursor;
#endif

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual gfx::NativeCursor GetCursorForPoint(
      views::Event::EventType event_type,
      const gfx::Point& p);

  ContentSettingBubbleContents* parent_;
  views::Link* link_;
};

#if defined(OS_WIN)
HCURSOR ContentSettingBubbleContents::Favicon::g_hand_cursor = NULL;
#endif

ContentSettingBubbleContents::Favicon::Favicon(
    const SkBitmap& image,
    ContentSettingBubbleContents* parent,
    views::Link* link)
    : parent_(parent),
      link_(link) {
  SetImage(image);
}

ContentSettingBubbleContents::Favicon::~Favicon() {
}

bool ContentSettingBubbleContents::Favicon::OnMousePressed(
    const views::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

void ContentSettingBubbleContents::Favicon::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  if (!canceled &&
      (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
      HitTest(event.location()))
    parent_->LinkActivated(link_, event.GetFlags());
}

gfx::NativeCursor ContentSettingBubbleContents::Favicon::GetCursorForPoint(
    views::Event::EventType event_type,
    const gfx::Point& p) {
#if defined(OS_WIN)
  if (!g_hand_cursor)
    g_hand_cursor = LoadCursor(NULL, IDC_HAND);
  return g_hand_cursor;
#elif defined(OS_LINUX)
  return gdk_cursor_new(GDK_HAND2);
#endif
}

ContentSettingBubbleContents::ContentSettingBubbleContents(
    ContentSettingBubbleModel* content_setting_bubble_model,
    Profile* profile,
    TabContents* tab_contents)
    : content_setting_bubble_model_(content_setting_bubble_model),
      profile_(profile),
      tab_contents_(tab_contents),
      info_bubble_(NULL),
      close_button_(NULL),
      manage_link_(NULL) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
}

void ContentSettingBubbleContents::ViewHierarchyChanged(bool is_add,
                                                        View* parent,
                                                        View* child) {
  if (is_add && (child == this))
    InitControlLayout();
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const views::Event& event) {
  if (sender == close_button_) {
    info_bubble_->Close();  // CAREFUL: This deletes us.
    return;
  }

  for (std::vector<RadioGroup>::const_iterator i = radio_groups_.begin();
       i != radio_groups_.end(); ++i) {
    for (RadioGroup::const_iterator j = i->begin(); j != i->end(); ++j) {
      if (sender == *j) {
        content_setting_bubble_model_->OnRadioClicked(
            i - radio_groups_.begin(), j - i->begin());
        return;
      }
    }
  }
  NOTREACHED() << "unknown radio";
}

void ContentSettingBubbleContents::LinkActivated(views::Link* source,
                                                 int event_flags) {
  if (source == manage_link_) {
    content_setting_bubble_model_->OnManageLinkClicked();
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  PopupLinks::const_iterator i(popup_links_.find(source));
  DCHECK(i != popup_links_.end());
  content_setting_bubble_model_->OnPopupClicked(i->second);
}

void ContentSettingBubbleContents::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}

void ContentSettingBubbleContents::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  views::Label* title_label = new views::Label(UTF8ToWide(
      bubble_content.title));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS) {
    const int popup_column_set_id = 2;
    views::ColumnSet* popup_column_set =
        layout->AddColumnSet(popup_column_set_id);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                                GridLayout::USE_PREF, 0, 0);
    popup_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                                GridLayout::USE_PREF, 0, 0);

    for (std::vector<ContentSettingBubbleModel::PopupItem>::const_iterator
         i(bubble_content.popup_items.begin());
         i != bubble_content.popup_items.end(); ++i) {
      if (i != bubble_content.popup_items.begin())
        layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
      layout->StartRow(0, popup_column_set_id);

      views::Link* link = new views::Link(UTF8ToWide(i->title));
      link->SetController(this);
      popup_links_[link] = i - bubble_content.popup_items.begin();
      layout->AddView(new Favicon((*i).bitmap, this, link));
      layout->AddView(link);
    }
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  const ContentSettingBubbleModel::RadioGroups& radio_groups =
      content_setting_bubble_model_->bubble_content().radio_groups;
  for (ContentSettingBubbleModel::RadioGroups::const_iterator i =
       radio_groups.begin(); i != radio_groups.end(); ++i) {
    const ContentSettingBubbleModel::RadioItems& radio_items = i->radio_items;
    RadioGroup radio_group;
    for (ContentSettingBubbleModel::RadioItems::const_iterator j =
         radio_items.begin(); j != radio_items.end(); ++j) {
      views::RadioButton* radio = new views::RadioButton(
            UTF8ToWide(*j), i - radio_groups.begin());
      radio->set_listener(this);
      radio_group.push_back(radio);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(radio);
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    }
    radio_groups_.push_back(radio_group);
    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator, 1, 1, GridLayout::FILL, GridLayout::FILL);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    // Now that the buttons have been added to the view hierarchy, it's safe
    // to call SetChecked() on them.
    radio_group[i->default_item]->SetChecked(true);
  }

  const int double_column_set_id = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(double_column_set_id);
  double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  double_column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  manage_link_ = new views::Link(UTF8ToWide(bubble_content.manage_link));
  manage_link_->SetController(this);

  layout->StartRow(0, double_column_set_id);
  layout->AddView(manage_link_);

  close_button_ =
      new views::NativeButton(this, l10n_util::GetString(IDS_DONE));
  layout->AddView(close_button_);
}
