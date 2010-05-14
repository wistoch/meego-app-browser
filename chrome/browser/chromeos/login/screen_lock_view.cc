// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_lock_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"

namespace {

// Max image size.
const int kMaxImageSize = 260;

// Gap between edge and image view, and image view and controls.
const int kBorderSize = 30;

// Background color.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color.
const SkColor kTextColor = SK_ColorWHITE;

}  // namespace

namespace chromeos {

using views::GridLayout;

ScreenLockView::ScreenLockView(ScreenLocker* screen_locker)
    : image_view_(NULL),
      password_field_(NULL),
      unlock_button_(NULL),
      screen_locker_(screen_locker) {
  DCHECK(screen_locker_);
}

void ScreenLockView::Init() {
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_IMAGE_CHANGED,
                 NotificationService::AllSources());

  views::View* main = new views::View();
  main->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  // Password field.
  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_EMPTY_PASSWORD_TEXT));
  password_field_->SetController(this);

  // Unlock button.
  // TODO(sky|oshima): change ids
  unlock_button_ = new views::TextButton(
      this, l10n_util::GetString(IDS_LOGIN_BUTTON));

  // User icon.
  image_view_ = new views::ImageView();
  UserManager::User user = screen_locker_->user();
  SetImage(user.image(), user.image().width(), user.image().height());

  // User name.
  std::wstring text = UTF8ToWide(user.GetDisplayName());
  views::Label* label = new views::Label(text);
  label->SetColor(kTextColor);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font =
      rb.GetFont(ResourceBundle::LargeFont).DeriveFont(0, gfx::Font::BOLD);
  label->SetFont(font);

  // Layouts image, textfield and button components.
  GridLayout* layout = new GridLayout(main);
  main->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, 5);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 5);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 5);

  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 0);
  layout->AddView(image_view_);
  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 1);
  layout->AddView(password_field_);
  layout->AddView(unlock_button_);
  layout->AddPaddingRow(0, 5);

  unlock_button_->SetFocusable(true);

  // Layouts the main view and the account label.
  layout = new GridLayout(this);
  SetLayoutManager(layout);
  column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(main);
  layout->StartRow(0, 1);
  layout->AddView(label);
}

void ScreenLockView::ClearAndSetFocusToPassword() {
  password_field_->RequestFocus();
  password_field_->SetText(string16());
}

void ScreenLockView::SetEnabled(bool enabled) {
  views::View::SetEnabled(enabled);

  // TODO(oshima): Re-enabling does not move the focus to the view
  // that had a focus (issue http://crbug.com/43131).
  // Move the focus to other field as a workaround.
  if (!enabled)
    unlock_button_->RequestFocus();
  unlock_button_->SetEnabled(enabled);
  password_field_->SetEnabled(enabled);
}

void ScreenLockView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  screen_locker_->Authenticate(password_field_->text());
}

bool ScreenLockView::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    screen_locker_->Authenticate(password_field_->text());
    return true;
  }
  return false;
}

void ScreenLockView::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type != NotificationType::LOGIN_USER_IMAGE_CHANGED || !image_view_)
    return;

  UserManager::User* user = Details<UserManager::User>(details).ptr();
  if (screen_locker_->user().email() != user->email())
    return;

  SetImage(user->image(), user->image().width(), user->image().height());
  image_view_->SchedulePaint();
}

//////////////////////////////////////////////////////////////////////////
// private:

void ScreenLockView::SetImage(const SkBitmap& image,
                              int desired_width,
                              int desired_height) {
  image_view_->SetImage(image);
  image_view_->SetImageSize(gfx::Size(std::min(desired_width, kMaxImageSize),
                                      std::min(desired_height, kMaxImageSize)));
}

}  // namespace chromeos

