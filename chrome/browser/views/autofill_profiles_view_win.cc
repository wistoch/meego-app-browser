// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/views/autofill_profiles_view_win.h"

#include <vsstyle.h>
#include <vssym32.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/theme_resources_util.h"
#include "chrome/browser/views/list_background.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/native_theme_win.h"
#include "gfx/size.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/border.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/scroll_view.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// padding on the sides of AutoFill settings dialog.
const int kDialogPadding = 7;

// Insets for subview controls.
const int kSubViewInsets = 5;

};  // namespace

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, static data:
AutoFillProfilesView* AutoFillProfilesView::instance_ = NULL;

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, static data:
int AutoFillProfilesView::ScrollViewContents::line_height_ = 0;

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, public:
AutoFillProfilesView::AutoFillProfilesView(
    AutoFillDialogObserver* observer,
    PersonalDataManager* personal_data_manager,
    PrefService* preferences,
    AutoFillProfile* imported_profile,
    CreditCard* imported_credit_card)
    : observer_(observer),
      personal_data_manager_(personal_data_manager),
      preferences_(preferences),
      save_changes_(NULL),
      scroll_view_(NULL),
      focus_manager_(NULL) {
  DCHECK(preferences_);
  default_profile_ = preferences_->GetString(prefs::kAutoFillDefaultProfile);
  default_credit_card_ =
      preferences_->GetString(prefs::kAutoFillDefaultCreditCard);
  default_profile_iterator_ = profiles_set_.end();
  default_credit_card_iterator_ = credit_card_set_.end();
  if (imported_profile) {
    profiles_set_.push_back(EditableSetInfo(imported_profile, true, true));
    default_profile_iterator_ = profiles_set_.begin();
  }
  if (imported_credit_card) {
    credit_card_set_.push_back(
        EditableSetInfo(imported_credit_card, true, true));
    default_credit_card_iterator_ = credit_card_set_.begin();
  }
}

AutoFillProfilesView::~AutoFillProfilesView() {
  // Removes observer if we are observing Profile load. Does nothing otherwise.
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

int AutoFillProfilesView::Show(gfx::NativeWindow parent,
                               AutoFillDialogObserver* observer,
                               PersonalDataManager* personal_data_manager,
                               PrefService* preferences,
                               AutoFillProfile* imported_profile,
                               CreditCard* imported_credit_card) {
  if (!instance_) {
    instance_ = new AutoFillProfilesView(observer, personal_data_manager,
        preferences, imported_profile, imported_credit_card);

    // |instance_| will get deleted once Close() is called.
    views::Window::CreateChromeWindow(parent, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible())
    instance_->window()->Show();
  else
    instance_->window()->Activate();
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, protected:
void AutoFillProfilesView::AddClicked(EditableSetType item_type) {
  int group_id = 0;
  if (item_type == EDITABLE_SET_ADDRESS) {
    AutoFillProfile address(std::wstring(), 0);
    // If it is the first item, set it to default. Otherwise default is already
    // set.
    bool default_item = (profiles_set_.size() == 0);
    profiles_set_.push_back(EditableSetInfo(&address, true, default_item));
    group_id = profiles_set_.size() - 1;
    SetDefaultProfileIterator();
  } else if (item_type == EDITABLE_SET_CREDIT_CARD) {
    CreditCard credit_card(std::wstring(), 0);
    // If it is the first item, set it to default. Otherwise default is already
    // set.
    bool default_item = (credit_card_set_.size() == 0);
    credit_card_set_.push_back(EditableSetInfo(&credit_card, true,
                                               default_item));
    group_id = profiles_set_.size() + credit_card_set_.size() - 1;
    SetDefaultCreditCardIterator();
  } else {
    NOTREACHED();
  }
  scroll_view_->RebuildView(FocusedItem(group_id,
                            EditableSetViewContents::kLabelText));
  scroll_view_->EnsureGroupOnScreen(group_id);
}

void AutoFillProfilesView::DeleteEditableSet(
    std::vector<EditableSetInfo>::iterator field_set_iterator) {
  FocusedItem focused_item_index;
  if (field_set_iterator->is_address) {
    string16 label = field_set_iterator->address.Label();
    bool set_new_default = false;
    if (field_set_iterator->is_default && profiles_set_.size() > 1)
      set_new_default = true;
    profiles_set_.erase(field_set_iterator);
    // Set first profile as a new default.
    if (set_new_default) {
      profiles_set_[0].is_default = true;
      default_profile_iterator_ = profiles_set_.begin();
    }

    for (std::vector<EditableSetInfo>::iterator it = credit_card_set_.begin();
         it != credit_card_set_.end();
         ++it) {
      if (it->credit_card.shipping_address() == label)
        it->credit_card.set_shipping_address(string16());
      if (it->credit_card.billing_address() == label)
        it->credit_card.set_billing_address(string16());
    }
    focused_item_index = FocusedItem(ScrollViewContents::kAddAddressButton, 0);
  } else {
    bool set_new_default = false;
    if (field_set_iterator->is_default && credit_card_set_.size() > 1)
      set_new_default = true;
    credit_card_set_.erase(field_set_iterator);
    // Set first credit card as a new default.
    if (set_new_default) {
      credit_card_set_[0].is_default = true;
      default_credit_card_iterator_ = credit_card_set_.begin();
    }
    focused_item_index = FocusedItem(ScrollViewContents::kAddCcButton, 0);
  }
  scroll_view_->RebuildView(focused_item_index);
}

void AutoFillProfilesView::CollapseStateChanged(
    std::vector<EditableSetInfo>::iterator field_set_iterator) {
  scroll_view_->RebuildView(FocusedItem());
}

void AutoFillProfilesView::NewDefaultSet(
    std::vector<EditableSetInfo>::iterator field_set_iterator) {
  if (field_set_iterator->is_address) {
    if (default_profile_iterator_ != profiles_set_.end())
      default_profile_iterator_->is_default = false;
    default_profile_iterator_ = field_set_iterator;
  } else {
    if (default_credit_card_iterator_ != credit_card_set_.end())
      default_credit_card_iterator_->is_default = false;
    default_credit_card_iterator_ = field_set_iterator;
  }
}


void AutoFillProfilesView::ValidateAndFixLabel() {
  std::wstring unset_label(l10n_util::GetString(IDS_AUTOFILL_UNTITLED_LABEL));
  for (std::vector<EditableSetInfo>::iterator it = profiles_set_.begin();
       it != profiles_set_.end();
       ++it) {
    if (it->address.Label().empty())
      it->address.set_label(unset_label);
  }
  for (std::vector<EditableSetInfo>::iterator it = credit_card_set_.begin();
       it != credit_card_set_.end();
       ++it) {
    if (it->credit_card.Label().empty())
      it->credit_card.set_label(unset_label);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::View implementations
void AutoFillProfilesView::Layout() {
  scroll_view_->SetBounds(kDialogPadding, kDialogPadding,
                          width() - (2 * kDialogPadding),
                          height() - (2 * kDialogPadding));
}

gfx::Size AutoFillProfilesView::GetPreferredSize() {
  return views::Window::GetLocalizedContentsSize(
      IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
      IDS_AUTOFILL_DIALOG_HEIGHT_LINES);
}

void AutoFillProfilesView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this)
    Init();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::DialogDelegate implementations:
int AutoFillProfilesView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL |
         MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring AutoFillProfilesView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_OK:
    return l10n_util::GetString(IDS_AUTOFILL_DIALOG_SAVE);
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return std::wstring();
  default:
    break;
  }
  NOTREACHED();
  return std::wstring();
}

views::View* AutoFillProfilesView::GetExtraView() {
  // The dialog sizes the extra view to fill the entire available space.
  // We use a container to lay it out properly.
  views::View* link_container = new views::View();
  views::GridLayout* layout = new views::GridLayout(link_container);
  link_container->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kDialogPadding);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  views::Link* link = new views::Link(
      l10n_util::GetString(IDS_AUTOFILL_LEARN_MORE));
  link->SetController(this);
  layout->AddView(link);

  return link_container;
}

bool AutoFillProfilesView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_OK:
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return true;
  default:
    break;
  }
  NOTREACHED();
  return false;
}


std::wstring AutoFillProfilesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_AUTOFILL_DIALOG_TITLE);
}

void AutoFillProfilesView::WindowClosing() {
  DCHECK(focus_manager_);
  focus_manager_->RemoveFocusChangeListener(this);
  instance_ = NULL;
}

views::View* AutoFillProfilesView::GetContentsView() {
  return this;
}

bool AutoFillProfilesView::Accept() {
  DCHECK(observer_);
  ValidateAndFixLabel();
  std::vector<AutoFillProfile> profiles;
  profiles.reserve(profiles_set_.size());
  std::vector<EditableSetInfo>::iterator it;
  string16 new_default_profile;
  for (it = profiles_set_.begin(); it != profiles_set_.end(); ++it) {
    profiles.push_back(it->address);
    if (it->is_default)
      new_default_profile = it->address.Label();
  }
  std::vector<CreditCard> credit_cards;
  credit_cards.reserve(credit_card_set_.size());
  string16 new_default_cc;
  for (it = credit_card_set_.begin(); it != credit_card_set_.end(); ++it) {
    credit_cards.push_back(it->credit_card);
    if (it->is_default)
      new_default_cc = it->credit_card.Label();
  }
  DCHECK(preferences_);
  if (default_profile_ != new_default_profile) {
    default_profile_ = new_default_profile;
    preferences_->SetString(prefs::kAutoFillDefaultProfile, default_profile_);
  }
  if (default_credit_card_ != new_default_cc) {
    default_credit_card_ = new_default_cc;
    preferences_->SetString(prefs::kAutoFillDefaultCreditCard,
                            default_credit_card_);
  }
  observer_->OnAutoFillDialogApply(&profiles, &credit_cards);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::ButtonListener implementations:
void AutoFillProfilesView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  NOTIMPLEMENTED();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::LinkController implementations:
void AutoFillProfilesView::LinkActivated(views::Link* source, int event_flags) {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(GURL(kAutoFillLearnMoreUrl), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::TYPED);
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::FocusChangeListener implementations:
void AutoFillProfilesView::FocusWillChange(views::View* focused_before,
                                           views::View* focused_now) {
  if (focused_now) {
    focused_now->ScrollRectToVisible(gfx::Rect(focused_now->width(),
                                     focused_now->height()));
  }
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, PersonalDataManager::Observer implementation.
void  AutoFillProfilesView::OnPersonalDataLoaded() {
  personal_data_manager_->RemoveObserver(this);
  GetData();
  FocusedItem focused_item_index(ScrollViewContents::kAddAddressButton, 0);
  if (profiles_set_.size() + credit_card_set_.size() > 0) {
    focused_item_index = FocusedItem(0, EditableSetViewContents::kLabelText);
  }

  scroll_view_->RebuildView(focused_item_index);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, private:
void AutoFillProfilesView::Init() {
  GetData();
  scroll_view_ = new AutoFillScrollView(this,
                                        &profiles_set_,
                                        &credit_card_set_);

  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
    views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(scroll_view_);
  ValidateAndFixLabel();
  focus_manager_ = GetFocusManager();
  DCHECK(focus_manager_);
  focus_manager_->AddFocusChangeListener(this);
}

void AutoFillProfilesView::GetData() {
  if (!personal_data_manager_->IsDataLoaded()) {
    personal_data_manager_->SetObserver(this);
    return;
  }
  bool imported_data_present = !profiles_set_.empty() ||
                               !credit_card_set_.empty();
  bool default_set = !profiles_set_.empty();
  if (!imported_data_present) {
    profiles_set_.reserve(personal_data_manager_->profiles().size());
    for (std::vector<AutoFillProfile*>::const_iterator address_it =
         personal_data_manager_->profiles().begin();
         address_it != personal_data_manager_->profiles().end();
         ++address_it) {
      bool default_profile = ((*address_it)->Label() == default_profile_);
      default_set = (default_set || default_profile);
      profiles_set_.push_back(EditableSetInfo(*address_it, false,
                                              default_profile));
    }
  }

  // If nothing is default, set first to be default.
  if (!default_set && profiles_set_.size() > 0)
    profiles_set_[0].is_default = true;
  default_set = !credit_card_set_.empty();
  if (!imported_data_present) {
    credit_card_set_.reserve(personal_data_manager_->credit_cards().size());
    for (std::vector<CreditCard*>::const_iterator cc_it =
         personal_data_manager_->credit_cards().begin();
         cc_it != personal_data_manager_->credit_cards().end();
         ++cc_it) {
      bool default_cc = ((*cc_it)->Label() == default_credit_card_);
      default_set = (default_set || default_cc);
      credit_card_set_.push_back(EditableSetInfo(*cc_it, false, default_cc));
    }
  }
  // If nothing is default, set first to be default.
  if (!default_set && credit_card_set_.size() > 0)
     credit_card_set_[0].is_default = true;

  // Remember default iterators.
  SetDefaultProfileIterator();
  SetDefaultCreditCardIterator();
}

bool AutoFillProfilesView::IsDataReady() const {
  return personal_data_manager_->IsDataLoaded();
}

void AutoFillProfilesView::SetDefaultProfileIterator() {
  for (default_profile_iterator_ = profiles_set_.begin();
       default_profile_iterator_ != profiles_set_.end();
       ++default_profile_iterator_) {
    if (default_profile_iterator_->is_default)
      break;
  }
}

void AutoFillProfilesView::SetDefaultCreditCardIterator() {
  for (default_credit_card_iterator_ = credit_card_set_.begin();
       default_credit_card_iterator_ != credit_card_set_.end();
       ++default_credit_card_iterator_) {
    if (default_credit_card_iterator_->is_default)
      break;
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, public:
AutoFillProfilesView::PhoneSubView::PhoneSubView(
    views::Label* label,
    views::Textfield* text_country,
    views::Textfield* text_area,
    views::Textfield* text_phone)
    : label_(label),
      text_country_(text_country),
      text_area_(text_area),
      text_phone_(text_phone) {
  DCHECK(label_);
  DCHECK(text_country_);
  DCHECK(text_area_);
  DCHECK(text_phone_);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, views::View implementations
void AutoFillProfilesView::PhoneSubView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    const int triple_column_fill_view_set_id = 0;
    views::ColumnSet* column_set =
        layout->AddColumnSet(triple_column_fill_view_set_id);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
        views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
        views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 2,
         views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(0, triple_column_fill_view_set_id);
    layout->AddView(label_, 5, 1);
    layout->StartRow(0, triple_column_fill_view_set_id);
    text_country_->set_default_width_in_chars(5);
    text_area_->set_default_width_in_chars(5);
    text_phone_->set_default_width_in_chars(10);
    layout->AddView(text_country_);
    layout->AddView(text_area_);
    layout->AddView(text_phone_);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, static data:
AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::address_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LABEL, NO_SERVER_DATA },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FIRST_NAME,
    NAME_FIRST },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_MIDDLE_NAME,
    NAME_MIDDLE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LAST_NAME,
    NAME_LAST },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_EMAIL, EMAIL_ADDRESS },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_COMPANY_NAME,
    COMPANY_NAME },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_LINE_1,
    ADDRESS_HOME_LINE1 },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_LINE_2,
    ADDRESS_HOME_LINE2 },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_CITY,
    ADDRESS_HOME_CITY },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_STATE,
    ADDRESS_HOME_STATE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_ZIP,
    ADDRESS_HOME_ZIP },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_COUNTRY,
    ADDRESS_HOME_COUNTRY },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_COUNTRY,
    PHONE_HOME_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_AREA,
    PHONE_HOME_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_PHONE,
    PHONE_HOME_NUMBER },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_COUNTRY,
    PHONE_FAX_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_AREA,
    PHONE_FAX_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_PHONE,
    PHONE_FAX_NUMBER },
};

AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::credit_card_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LABEL, NO_SERVER_DATA },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NAME,
    CREDIT_CARD_NAME },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NUMBER,
    CREDIT_CARD_NUMBER },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_MONTH,
    CREDIT_CARD_EXP_MONTH },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_YEAR,
    CREDIT_CARD_EXP_4_DIGIT_YEAR },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_CVC,
    CREDIT_CARD_VERIFICATION_CODE },
  /* Phone is disabled for now.
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_COUNTRY,
  PHONE_HOME_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_AREA,
  PHONE_HOME_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_PHONE,
  PHONE_HOME_NUMBER },
  */
};

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, public:
AutoFillProfilesView::EditableSetViewContents::EditableSetViewContents(
    AutoFillProfilesView* observer,
    AddressComboBoxModel* billing_model,
    AddressComboBoxModel* shipping_model,
    std::vector<EditableSetInfo>::iterator field_set)
    : editable_fields_set_(field_set),
      delete_button_(NULL),
      expand_item_button_(NULL),
      title_label_(NULL),
      title_label_preview_(NULL),
      observer_(observer),
      billing_model_(billing_model),
      shipping_model_(shipping_model),
      combo_box_billing_(NULL),
      combo_box_shipping_(NULL) {
  ZeroMemory(text_fields_, sizeof(text_fields_));
}

// Two helpers to set focus correctly during rebuild of list view.
int AutoFillProfilesView::EditableSetViewContents::GetFocusedControlIndex(
    const views::View* focus) const {
  DCHECK(focus);
  if (static_cast<const views::View*>(expand_item_button_) == focus)
    return 0;
  if (static_cast<const views::View*>(combo_box_billing_) == focus)
    return 1;
  if (static_cast<const views::View*>(combo_box_shipping_) == focus)
    return 2;
  if (static_cast<const views::View*>(delete_button_) == focus)
    return 3;
  for (int i = 0; i < MAX_TEXT_FIELD; ++i) {
    if (static_cast<const views::View*>(text_fields_[i]) == focus)
      return i + 4;
  }
  return AutoFillProfilesView::kNoItemFocused;
}

views::View* AutoFillProfilesView::EditableSetViewContents::GetFocusedControl(
    int index) {
  if (index == 0 || index == AutoFillProfilesView::kNoItemFocused ||
      !editable_fields_set_->is_opened) {
    return expand_item_button_;
  }
  switch (index) {
    case 1:
      return combo_box_billing_;
    case 2:
      return combo_box_shipping_;
    case 3:
      return delete_button_;
    default:
      DCHECK(index - 4 < MAX_TEXT_FIELD);
      return text_fields_[index - 4];
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, views::View implementations
void AutoFillProfilesView::EditableSetViewContents::Layout() {
  View::Layout();
}

gfx::Size AutoFillProfilesView::EditableSetViewContents::GetPreferredSize() {
  gfx::Size prefsize;
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    prefsize = gfx::Size(width, GetHeightForWidth(width));
  }
  return prefsize;
}

void AutoFillProfilesView::EditableSetViewContents::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    views::GridLayout* layout = new views::GridLayout(this);
    layout->SetInsets(kSubViewInsets, kSubViewInsets,
                      kSubViewInsets, kSubViewInsets);
    SetLayoutManager(layout);
    InitLayoutGrid(layout);
    delete_button_ = new views::NativeButton(this,
        l10n_util::GetString(IDS_AUTOFILL_DELETE_BUTTON));

    InitTitle(layout);
    if (editable_fields_set_->is_opened) {
      if (editable_fields_set_->is_address)
        InitAddressFields(layout);
      else
        InitCreditCardFields(layout);
      // Create border, but only if it is opened.
      // The border is a standard group box.
      SkColor border_color =
          gfx::NativeTheme::instance()->GetThemeColorWithDefault(
          gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL,
          TMT_EDGESHADOWCOLOR, COLOR_GRAYTEXT);
      set_border(views::Border::CreateSolidBorder(1, border_color));
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Textfield::Controller implementations
void AutoFillProfilesView::EditableSetViewContents::ContentsChanged(
    views::Textfield* sender,  const string16& new_contents) {
  if (editable_fields_set_->is_address) {
    for (int field = 0; field < arraysize(address_fields_); ++field) {
      DCHECK(text_fields_[address_fields_[field].text_field]);
      if (text_fields_[address_fields_[field].text_field] == sender) {
        if (address_fields_[field].text_field == TEXT_LABEL) {
          editable_fields_set_->address.set_label(new_contents);
          title_label_->SetText(new_contents);
          // One of the address labels changed - update combo boxes
          billing_model_->LabelChanged();
          shipping_model_->LabelChanged();
        } else {
          editable_fields_set_->address.SetInfo(
              AutoFillType(address_fields_[field].type), new_contents);
        }
        return;
      }
    }
  } else {
    for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
      DCHECK(text_fields_[credit_card_fields_[field].text_field]);
      if (text_fields_[credit_card_fields_[field].text_field] == sender) {
        if (credit_card_fields_[field].text_field == TEXT_LABEL) {
          editable_fields_set_->credit_card.set_label(new_contents);
          title_label_->SetText(new_contents);
        } else {
          editable_fields_set_->credit_card.SetInfo(
              AutoFillType(credit_card_fields_[field].type), new_contents);
        }
        return;
      }
    }
  }
}

bool AutoFillProfilesView::EditableSetViewContents::HandleKeystroke(
    views::Textfield* sender, const views::Textfield::Keystroke& keystroke) {
  if (sender == text_fields_[TEXT_CC_NUMBER] &&
      !editable_fields_set_->has_credit_card_number_been_edited) {
    // You cannot edit obfuscated number, you must retype it anew.
    sender->SetText(string16());
    editable_fields_set_->has_credit_card_number_been_edited = true;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::ButtonListener implementations:
void AutoFillProfilesView::EditableSetViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == delete_button_) {
    observer_->DeleteEditableSet(editable_fields_set_);
  } else if (sender == expand_item_button_ ||
             sender == title_label_ || sender == title_label_preview_) {
    editable_fields_set_->is_opened = !editable_fields_set_->is_opened;
    observer_->CollapseStateChanged(editable_fields_set_);
  } else if (sender == default_) {
    editable_fields_set_->is_default = true;
    observer_->NewDefaultSet(editable_fields_set_);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Combobox::Listener implementations:
void AutoFillProfilesView::EditableSetViewContents::ItemChanged(
    views::Combobox* combo_box, int prev_index, int new_index) {
  DCHECK(billing_model_);
  DCHECK(shipping_model_);
  if (combo_box == combo_box_billing_) {
    if (new_index == -1) {
      NOTREACHED();
    } else {
      editable_fields_set_->credit_card.set_billing_address(
          billing_model_->GetItemAt(new_index));
    }
  } else if (combo_box == combo_box_shipping_) {
    if (new_index == -1) {
      NOTREACHED();
    } else if (new_index == 0) {
      editable_fields_set_->credit_card.set_shipping_address(
          editable_fields_set_->credit_card.billing_address());
    } else {
      editable_fields_set_->credit_card.set_shipping_address(
          shipping_model_->GetItemAt(new_index));
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, private:
void AutoFillProfilesView::EditableSetViewContents::InitTitle(
    views::GridLayout* layout) {
  std::wstring title;
  std::wstring title_preview;
  if (editable_fields_set_->is_address) {
    title = editable_fields_set_->address.Label();
    if (title.empty())
      title = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
    title_preview = editable_fields_set_->address.PreviewSummary();
  } else {
    title = editable_fields_set_->credit_card.Label();
    if (title.empty())
      title = l10n_util::GetString(IDS_AUTOFILL_NEW_CREDITCARD);
    title_preview = editable_fields_set_->credit_card.PreviewSummary();
  }
  expand_item_button_ = new views::ImageButton(this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* image = NULL;
  if (editable_fields_set_->is_opened) {
    image =
        rb.GetBitmapNamed(ThemeResourcesUtil::GetId("expand_arrow_down_icon"));
  } else {
    image =
        rb.GetBitmapNamed(ThemeResourcesUtil::GetId("expand_arrow_right_icon"));
    title_label_preview_ = new views::TextButton(this, title_preview);
  }
  expand_item_button_->SetImage(views::CustomButton::BS_NORMAL, image);
  expand_item_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                         views::ImageButton::ALIGN_MIDDLE);
  expand_item_button_->SetFocusable(true);
  title_label_ = new views::TextButton(this, title);
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label_->SetFont(title_font);
  // Text *must* be re-set after font to update dimensions.
  title_label_->SetText(title);

  SkColor title_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetEnabledColor(title_color);
  SkColor bk_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_PUSHBUTTON, PBS_NORMAL, TMT_BTNFACE,
      COLOR_BTNFACE);
  if (editable_fields_set_->is_opened) {
    expand_item_button_->set_background(
        views::Background::CreateSolidBackground(bk_color));
    title_label_->set_background(
        views::Background::CreateSolidBackground(bk_color));
  }
  title_label_->set_alignment(views::TextButton::ALIGN_LEFT);

  layout->StartRow(0, three_column_header_);
  layout->AddView(expand_item_button_, 2, 1);
  if (editable_fields_set_->is_opened) {
    layout->AddView(title_label_, 3, 1);
  } else {
    layout->AddView(title_label_);
    layout->AddView(title_label_preview_);
  }
}

void AutoFillProfilesView::EditableSetViewContents::InitAddressFields(
    views::GridLayout* layout) {
  DCHECK(editable_fields_set_->is_address);

  for (int field = 0; field < arraysize(address_fields_); ++field) {
    DCHECK(!text_fields_[address_fields_[field].text_field]);
    text_fields_[address_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[address_fields_[field].text_field]->SetController(this);
    if (address_fields_[field].text_field == TEXT_LABEL) {
      text_fields_[TEXT_LABEL]->SetText(
          editable_fields_set_->address.Label());
    } else {
      text_fields_[address_fields_[field].text_field]->SetText(
          editable_fields_set_->address.GetFieldText(
          AutoFillType(address_fields_[field].type)));
    }
  }

  default_ = new views::RadioButton(
      l10n_util::GetString(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT),
      kDefaultAddressesGroup);
  default_->SetChecked(editable_fields_set_->is_default);
  default_->set_listener(this);
  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_AUTOFILL_DIALOG_LABEL)));
  layout->AddView(default_, 3, 1);
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_LABEL]);
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FIRST_NAME));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_MIDDLE_NAME));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_LAST_NAME));
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_FIRST_NAME]);
  layout->AddView(text_fields_[TEXT_MIDDLE_NAME]);
  layout->AddView(text_fields_[TEXT_LAST_NAME]);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EMAIL));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COMPANY_NAME),
                  3, 1);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_EMAIL]);
  layout->AddView(text_fields_[TEXT_COMPANY_NAME]);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1)), 3, 1);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_1], 3, 1);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2)), 3, 1);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_2], 3, 1);

  layout->StartRow(0, four_column_city_state_zip_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CITY));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_STATE));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_ZIP_CODE));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COUNTRY));
  // City (33% - 16/48), state(33%), zip (12.7% - 5/42), country (21% - 11/48)
  text_fields_[TEXT_ADDRESS_CITY]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_STATE]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_ZIP]->set_default_width_in_chars(5);
  text_fields_[TEXT_ADDRESS_COUNTRY]->set_default_width_in_chars(11);

  layout->StartRow(0, four_column_city_state_zip_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_CITY]);
  layout->AddView(text_fields_[TEXT_ADDRESS_STATE]);
  layout->AddView(text_fields_[TEXT_ADDRESS_ZIP]);
  layout->AddView(text_fields_[TEXT_ADDRESS_COUNTRY]);

  PhoneSubView* phone = new PhoneSubView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_PHONE),
      text_fields_[TEXT_PHONE_COUNTRY],
      text_fields_[TEXT_PHONE_AREA],
      text_fields_[TEXT_PHONE_PHONE]);

  PhoneSubView* fax = new PhoneSubView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FAX),
      text_fields_[TEXT_FAX_COUNTRY],
      text_fields_[TEXT_FAX_AREA],
      text_fields_[TEXT_FAX_PHONE]);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(phone);
  layout->AddView(fax);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(delete_button_);
}

void AutoFillProfilesView::EditableSetViewContents::InitCreditCardFields(
    views::GridLayout* layout) {
  DCHECK(!editable_fields_set_->is_address);
  DCHECK(billing_model_);
  DCHECK(shipping_model_);

  for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
    DCHECK(!text_fields_[credit_card_fields_[field].text_field]);
    text_fields_[credit_card_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[credit_card_fields_[field].text_field]->SetController(this);
    string16 field_text;
    switch (credit_card_fields_[field].text_field) {
    case TEXT_LABEL:
      field_text = editable_fields_set_->credit_card.Label();
      break;
    case TEXT_CC_NUMBER:
      field_text = editable_fields_set_->credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type));
      if (!field_text.empty())
        field_text = editable_fields_set_->credit_card.ObfuscatedNumber();
      break;
    default:
      field_text = editable_fields_set_->credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type));
      break;
    }
    text_fields_[credit_card_fields_[field].text_field]->SetText(field_text);
  }

  default_ = new views::RadioButton(
      l10n_util::GetString(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT),
      kDefaultCreditCardsGroup);
  default_->SetChecked(editable_fields_set_->is_default);
  default_->set_listener(this);
  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_LABEL));
  layout->AddView(default_, 3, 1);
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_LABEL]);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_CC_NAME]);
  // Address combo boxes.
  combo_box_billing_ = new views::Combobox(billing_model_);
  combo_box_billing_->set_listener(this);
  combo_box_billing_->SetSelectedItem(
      billing_model_->GetIndex(
      editable_fields_set_->credit_card.billing_address()));
  billing_model_->UsedWithComboBox(combo_box_billing_);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_BILLING_ADDRESS));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(combo_box_billing_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  combo_box_shipping_ = new views::Combobox(shipping_model_);
  combo_box_shipping_->set_listener(this);
  if (editable_fields_set_->credit_card.shipping_address() ==
      editable_fields_set_->credit_card.billing_address()) {
    // The addresses are the same, so use "the same address" label.
    combo_box_shipping_->SetSelectedItem(0);
  } else {
    combo_box_shipping_->SetSelectedItem(
        shipping_model_->GetIndex(
        editable_fields_set_->credit_card.shipping_address()));
  }
  shipping_model_->UsedWithComboBox(combo_box_shipping_);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_SHIPPING_ADDRESS));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(combo_box_shipping_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Layout credit card info
  layout->StartRow(0, four_column_ccnumber_expiration_cvc_);
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE), 3, 1);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CVC));
  layout->StartRow(0, four_column_ccnumber_expiration_cvc_);
  // Number (20 chars), month(2 chars), year (4 chars), cvc (4 chars)
  text_fields_[TEXT_CC_NUMBER]->set_default_width_in_chars(20);
  text_fields_[TEXT_CC_EXPIRATION_MONTH]->set_default_width_in_chars(2);
  text_fields_[TEXT_CC_EXPIRATION_YEAR]->set_default_width_in_chars(4);
  text_fields_[TEXT_CC_EXPIRATION_CVC]->set_default_width_in_chars(4);
  layout->AddView(text_fields_[TEXT_CC_NUMBER]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_MONTH]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_YEAR]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_CVC]);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(delete_button_);
}

void AutoFillProfilesView::EditableSetViewContents::InitLayoutGrid(
    views::GridLayout* layout) {
  views::ColumnSet* column_set =
      layout->AddColumnSet(double_column_fill_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  int i;
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(double_column_leading_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_fill_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_leading_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  // City (33% - 16/48), state(33%), zip (12.7% - 5/42), country (21% - 11/48)
  column_set = layout->AddColumnSet(four_column_city_state_zip_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        16, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        16, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        5, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        11, views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(four_column_ccnumber_expiration_cvc_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  // Number, expiration (month/year), and CVC are in ratio 20:2:4:4
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        20, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        2, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        4, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        4, views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(three_column_header_);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::FIXED, 0, 0);
}

views::Label*
AutoFillProfilesView::EditableSetViewContents::CreateLeftAlignedLabel(
    int label_id) {
  views::Label* label = new views::Label(l10n_util::GetString(label_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AddressComboBoxModel, public:
AutoFillProfilesView::AddressComboBoxModel::AddressComboBoxModel(
    bool is_billing)
    : address_labels_(NULL),
      is_billing_(is_billing) {
}

void AutoFillProfilesView::AddressComboBoxModel::set_address_labels(
    const std::vector<EditableSetInfo>* address_labels) {
  DCHECK(!address_labels_);
  address_labels_ = address_labels;
}

void AutoFillProfilesView::AddressComboBoxModel::UsedWithComboBox(
    views::Combobox* combo_box) {
  DCHECK(address_labels_);
  combo_boxes_.push_back(combo_box);
}

void AutoFillProfilesView::AddressComboBoxModel::LabelChanged() {
  DCHECK(address_labels_);
  for (std::list<views::Combobox*>::iterator it = combo_boxes_.begin();
       it != combo_boxes_.end();
       ++it)
    (*it)->ModelChanged();
}

int AutoFillProfilesView::AddressComboBoxModel::GetIndex(const string16 &s) {
  int shift = is_billing_ ? 0 : 1;
  DCHECK(address_labels_);
  for (size_t i = 0; i < address_labels_->size(); ++i) {
    DCHECK(address_labels_->at(i).is_address);
    if (address_labels_->at(i).address.Label() == s)
      return i + shift;
  }
  return -1;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AddressComboBoxModel,  ComboboxModel methods
int AutoFillProfilesView::AddressComboBoxModel::GetItemCount() {
  DCHECK(address_labels_);
  int shift = is_billing_ ? 0 : 1;
  return static_cast<int>(address_labels_->size()) + shift;
}

std::wstring AutoFillProfilesView::AddressComboBoxModel::GetItemAt(int index) {
  DCHECK(address_labels_);
  int shift = is_billing_ ? 0 : 1;
  DCHECK(index < (static_cast<int>(address_labels_->size()) + shift));
  if (!is_billing_ && !index)
    return l10n_util::GetString(IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);
  DCHECK(address_labels_->at(index - shift).is_address);
  std::wstring label = address_labels_->at(index - shift).address.Label();
  if (label.empty())
    label = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
  return label;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, public:
AutoFillProfilesView::ScrollViewContents::ScrollViewContents(
    AutoFillProfilesView* observer,
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards),
      add_address_(NULL),
      add_credit_card_(NULL),
      observer_(observer),
      billing_model_(true),
      shipping_model_(false) {
}

AutoFillProfilesView::FocusedItem
AutoFillProfilesView::ScrollViewContents::GetFocusedControlIndex(
    const views::View* focus) const {
  if (static_cast<const views::View*>(add_address_) == focus)
    return FocusedItem(kAddAddressButton, 0);
  if (static_cast<const views::View*>(add_credit_card_) == focus)
    return FocusedItem(kAddCcButton, 0);
  for (size_t i = 0; i < editable_contents_.size(); ++i) {
    int index = editable_contents_[i]->GetFocusedControlIndex(focus);
    if (index != AutoFillProfilesView::kNoItemFocused)
      return FocusedItem(i, index);
  }
  return FocusedItem();
}

views::View* AutoFillProfilesView::ScrollViewContents::GetFocusedControl(
    const AutoFillProfilesView::FocusedItem& index) {
  if (index.group == AutoFillProfilesView::kNoItemFocused)
    return add_address_;
  switch (index.group) {
    case kAddAddressButton:
      return add_address_;
    case kAddCcButton:
      return add_credit_card_;
    default:
      DCHECK(index.group < static_cast<int>(editable_contents_.size()));
      DCHECK(index.group >= 0);
      return editable_contents_[index.group]->GetFocusedControl(
          index.item);
  }
}

views::View* AutoFillProfilesView::ScrollViewContents::GetGroup(
    int group_index) {
  DCHECK(static_cast<size_t>(group_index) < editable_contents_.size());
  return static_cast<views::View*>(editable_contents_[group_index]);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, views::View implementations
int AutoFillProfilesView::ScrollViewContents::GetLineScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void AutoFillProfilesView::ScrollViewContents::Layout() {
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(x(), y(), width, height);
  } else {
    gfx::Size prefsize = GetPreferredSize();
    SetBounds(0, 0, prefsize.width(), prefsize.height());
  }
  View::Layout();
}

gfx::Size AutoFillProfilesView::ScrollViewContents::GetPreferredSize() {
  return gfx::Size();
}

void AutoFillProfilesView::ScrollViewContents::ViewHierarchyChanged(
     bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    if (!line_height_) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      line_height_ = rb.GetFont(ResourceBundle::BaseFont).height();
    }
    Init();
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents,
// views::ButtonListener implementations
void AutoFillProfilesView::ScrollViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == add_address_)
    observer_->AddClicked(EDITABLE_SET_ADDRESS);
  else if (sender == add_credit_card_)
    observer_->AddClicked(EDITABLE_SET_CREDIT_CARD);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, private
void AutoFillProfilesView::ScrollViewContents::Init() {
  gfx::Rect lb = GetLocalBounds(false);
  SetBounds(lb);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_filled_view_set_id = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(single_column_filled_view_set_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
      views::GridLayout::USE_PREF, 0, 0);
  if (!observer_->IsDataReady()) {
    layout->StartRow(0, single_column_filled_view_set_id);
    layout->AddView(new views::Label(
        l10n_util::GetString(IDS_AUTOFILL_LOADING)));
    return;
  }
  const int single_column_left_view_set_id = 1;
  column_set = layout->AddColumnSet(single_column_left_view_set_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        1, views::GridLayout::USE_PREF, 0, 0);
  const int single_column_filled_view_set_id_full_width = 2;
  column_set =
      layout->AddColumnSet(single_column_filled_view_set_id_full_width);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
      views::GridLayout::USE_PREF, 0, 0);
  views::Label* title_label = new views::Label(
      l10n_util::GetString(IDS_AUTOFILL_ADDRESSES_GROUP_NAME));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  layout->StartRow(0, single_column_left_view_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_filled_view_set_id_full_width);
  layout->AddView(new views::Separator);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  editable_contents_.reserve(profiles_->size() + credit_cards_->size());
  std::vector<EditableSetInfo>::iterator it;
  for (it = profiles_->begin(); it != profiles_->end(); ++it) {
    EditableSetViewContents* address_view =
        new EditableSetViewContents(observer_, &billing_model_,
                                    &shipping_model_, it);
    layout->StartRow(0, single_column_filled_view_set_id);
    layout->AddView(address_view);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    editable_contents_.push_back(address_view);
  }

  billing_model_.set_address_labels(profiles_);
  shipping_model_.set_address_labels(profiles_);

  add_address_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_ADD_ADDRESS_BUTTON));
  layout->StartRow(0, single_column_left_view_set_id);
  layout->AddView(add_address_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  title_label = new views::Label(
      l10n_util::GetString(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME));
  title_label->SetFont(title_font);
  layout->StartRow(0, single_column_left_view_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_filled_view_set_id_full_width);
  layout->AddView(new views::Separator);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  for (it = credit_cards_->begin(); it != credit_cards_->end(); ++it) {
    EditableSetViewContents* cc_view =
        new EditableSetViewContents(observer_, &billing_model_,
                                    &shipping_model_, it);
    layout->StartRow(0, single_column_filled_view_set_id);
    layout->AddView(cc_view);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    editable_contents_.push_back(cc_view);
  }

  add_credit_card_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));

  layout->StartRow(0, single_column_left_view_set_id);
  layout->AddView(add_credit_card_);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AutoFillScrollView, public:
AutoFillProfilesView::AutoFillScrollView::AutoFillScrollView(
    AutoFillProfilesView* observer,
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : scroll_view_(new views::ScrollView),
      scroll_contents_view_(
          new ScrollViewContents(observer, profiles, credit_cards)),
      profiles_(profiles),
      credit_cards_(credit_cards),
      observer_(observer) {
  AddChildView(scroll_view_);
  // After the following call, |scroll_view_| owns |scroll_contents_view_|
  // and deletes it when it gets deleted or reset.
  scroll_view_->SetContents(scroll_contents_view_);
  set_background(new ListBackground());
}

void AutoFillProfilesView::AutoFillScrollView::RebuildView(
    const AutoFillProfilesView::FocusedItem& new_focus_index) {
  AutoFillProfilesView::FocusedItem focus_index(new_focus_index);
  gfx::Rect visible_rectangle = scroll_view_->GetVisibleRect();
  if (focus_index.group == AutoFillProfilesView::kNoItemFocused &&
      GetFocusManager()) {
    // Save focus and restore it later.
    focus_index = scroll_contents_view_->GetFocusedControlIndex(
        GetFocusManager()->GetFocusedView());
  }

  scroll_contents_view_ = new ScrollViewContents(observer_,
                                                 profiles_,
                                                 credit_cards_);
  // Deletes the old contents view and takes ownership of
  // |scroll_contents_view_|.
  scroll_view_->SetContents(scroll_contents_view_);
  if (focus_index.group != AutoFillProfilesView::kNoItemFocused) {
    views::View* view = scroll_contents_view_->GetFocusedControl(focus_index);
    if (view && GetFocusManager()) {
      GetFocusManager()->SetFocusedView(view);
    }
  }
  scroll_contents_view_->ScrollRectToVisible(visible_rectangle);
}

void AutoFillProfilesView::AutoFillScrollView::EnsureGroupOnScreen(
    int group_index) {
  views::View* group = scroll_contents_view_->GetGroup(group_index);

  group->ScrollRectToVisible(gfx::Rect(group->width(), group->height()));
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AutoFillScrollView, views::View implementations
void AutoFillProfilesView::AutoFillScrollView::Layout() {
  gfx::Rect lb = GetLocalBounds(false);

  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::LIST);
  border.set_width(border.width() + kPanelHorizMargin);
  lb.Inset(border.width(), border.height());
  scroll_view_->SetBounds(lb);
  scroll_view_->Layout();
}

// Declared in "chrome/browser/autofill/autofill_dialog.h"
void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile,
                        AutoFillProfile* imported_profile,
                        CreditCard* imported_credit_card) {
  DCHECK(profile);

  // It's possible we haven't shown the InfoBar yet, but if the user is in the
  // AutoFill dialog, she doesn't need to be asked to enable or disable
  // AutoFill.
  profile->GetPrefs()->SetBoolean(prefs::kAutoFillInfoBarShown, true);

  PersonalDataManager* personal_data_manager =
      profile->GetPersonalDataManager();
  DCHECK(personal_data_manager);
  AutoFillProfilesView::Show(parent, observer, personal_data_manager,
      profile->GetPrefs(), imported_profile, imported_credit_card);
}
