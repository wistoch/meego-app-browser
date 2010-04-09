// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/clock_menu_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "grit/generated_resources.h"

namespace chromeos {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockMenuButton::ClockMenuButton(StatusAreaHost* host)
    : MenuButton(NULL, std::wstring(), this, false),
      host_(host) {
  set_border(NULL);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(1, gfx::Font::BOLD));
  SetEnabledColor(0xB3FFFFFF); // White with 70% Alpha
  SetShowHighlighted(false);
  // Fill text with 0s to figure out max width of text size.
  ClearMaxTextSize();
  std::wstring zero = ASCIIToWide("0");
  std::wstring zerozero = ASCIIToWide("00");
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_AM,
                                zero, zerozero));
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_PM,
                                zero, zerozero));
  max_width_one_digit = GetPreferredSize().width();
  ClearMaxTextSize();
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_AM,
                                zerozero, zerozero));
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_PM,
                                zerozero, zerozero));
  max_width_two_digit = GetPreferredSize().width();
  set_alignment(TextButton::ALIGN_RIGHT);
  UpdateTextAndSetNextTimer();
  // Init member prefs so we can update the clock if prefs change.
  // This only works if we are within a browser and have a profile.
  if (host->GetProfile())
    timezone_.Init(prefs::kTimeZone, host->GetProfile()->GetPrefs(), this);
}

void ClockMenuButton::UpdateTextAndSetNextTimer() {
  UpdateText();

  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Start(base::TimeDelta::FromSeconds(seconds_left), this,
               &ClockMenuButton::UpdateTextAndSetNextTimer);
}

void ClockMenuButton::UpdateText() {
  // Use icu::Calendar because the correct timezone is set on icu::TimeZone's
  // default timezone.
  UErrorCode error = U_ZERO_ERROR;
  cal_.reset(icu::Calendar::createInstance(error));
  if (!cal_.get())
    return;

  int hour = cal_->get(UCAL_HOUR, error);
  int minute = cal_->get(UCAL_MINUTE, error);
  int ampm = cal_->get(UCAL_AM_PM, error);

  if (hour == 0)
    hour = 12;
  std::wstring hour_str = IntToWString(hour);
  std::wstring min_str = IntToWString(minute);
  // Append a "0" before the minute if it's only a single digit.
  if (minute < 10)
    min_str = IntToWString(0) + min_str;
  int msg = (ampm == UCAL_AM) ? IDS_STATUSBAR_CLOCK_SHORT_TIME_AM :
                                IDS_STATUSBAR_CLOCK_SHORT_TIME_PM;

  std::wstring time_string = l10n_util::GetStringF(msg, hour_str, min_str);

  // See if the preferred size changed. If so, relayout the StatusAreaView.
  int cur_width = GetPreferredSize().width();
  int new_width = hour < 10 ? max_width_one_digit : max_width_two_digit;
  SetText(time_string);
  set_max_width(new_width);

  // If width has changed, we want to relayout the StatusAreaView.
  if (new_width != cur_width)
    PreferredSizeChanged();

  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, menus::MenuModel implementation:

int ClockMenuButton::GetItemCount() const {
  // If options dialog is unavailable, don't count a separator and configure
  // menu item.
  return host_->ShouldOpenButtonOptions(this) ? 3 : 1;
}

menus::MenuModel::ItemType ClockMenuButton::GetTypeAt(int index) const {
  // There's a separator between the current date and the menu item to open
  // the options menu.
  return index == 1 ? menus::MenuModel::TYPE_SEPARATOR:
                      menus::MenuModel::TYPE_COMMAND;
}

string16 ClockMenuButton::GetLabelAt(int index) const {
  if (index == 0)
    return WideToUTF16(base::TimeFormatFriendlyDate(base::Time::Now()));
  return l10n_util::GetStringUTF16(IDS_STATUSBAR_CLOCK_OPEN_OPTIONS_DIALOG);
}

bool ClockMenuButton::IsEnabledAt(int index) const {
  // The 1st item is the current date, which is disabled.
  return index != 0;
}

void ClockMenuButton::ActivatedAt(int index) {
  host_->OpenButtonOptions(this);
}

///////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, NotificationObserver implementation:

void ClockMenuButton::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    const std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (!pref_name || *pref_name == prefs::kTimeZone) {
      UpdateText();
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::ViewMenuDelegate implementation:

void ClockMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  if (!clock_menu_.get())
    clock_menu_.reset(new views::Menu2(this));
  else
    clock_menu_->Rebuild();
  clock_menu_->UpdateStates();
  clock_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

}  // namespace chromeos
