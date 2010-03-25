// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton

// static
const int PowerMenuButton::kNumPowerImages = 12;

PowerMenuButton::PowerMenuButton()
    : StatusAreaButton(this),
      ALLOW_THIS_IN_INITIALIZER_LIST(power_menu_(this)),
      icon_id_(-1) {
  UpdateIcon();
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
}

PowerMenuButton::~PowerMenuButton() {
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, menus::MenuModel implementation:

int PowerMenuButton::GetItemCount() const {
  return 2;
}

menus::MenuModel::ItemType PowerMenuButton::GetTypeAt(int index) const {
  return menus::MenuModel::TYPE_COMMAND;
}

string16 PowerMenuButton::GetLabelAt(int index) const {
  PowerLibrary* cros = CrosLibrary::Get()->GetPowerLibrary();
  // The first item shows the percentage of battery left.
  if (index == 0) {
    // If fully charged, always show 100% even if internal number is a bit less.
    double percent = cros->battery_fully_charged() ? 100 :
                                                     cros->battery_percentage();
    return l10n_util::GetStringFUTF16(IDS_STATUSBAR_BATTERY_PERCENTAGE,
        IntToString16(static_cast<int>(percent)));
  }

  // The second item shows the battery is charged if it is.
  if (cros->battery_fully_charged())
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_BATTERY_IS_CHARGED);

  // If battery is in an intermediate charge state, we show how much time left.
  base::TimeDelta time = cros->line_power_on() ? cros->battery_time_to_full() :
                                                 cros->battery_time_to_empty();
  if (time.InSeconds() == 0) {
    // If time is 0, then that means we are still calculating how much time.
    // Depending if line power is on, we either show a message saying that we
    // are calculating time until full or calculating remaining time.
    int msg = cros->line_power_on() ?
        IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_FULL :
        IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_EMPTY;
    return l10n_util::GetStringUTF16(msg);
  } else {
    // Depending if line power is on, we either show a message saying XX:YY
    // until full or XX:YY remaining where XX is number of hours and YY is
    // number of minutes.
    int msg = cros->line_power_on() ? IDS_STATUSBAR_BATTERY_TIME_UNTIL_FULL :
                                      IDS_STATUSBAR_BATTERY_TIME_UNTIL_EMPTY;
    int hour = time.InHours();
    int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
    string16 hour_str = IntToString16(hour);
    string16 min_str = IntToString16(min);
    // Append a "0" before the minute if it's only a single digit.
    if (min < 10)
      min_str = ASCIIToUTF16("0") + min_str;
    return l10n_util::GetStringFUTF16(msg, hour_str, min_str);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::ViewMenuDelegate implementation:

void PowerMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  power_menu_.Rebuild();
  power_menu_.UpdateStates();
  power_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, PowerLibrary::Observer implementation:

void PowerMenuButton::PowerChanged(PowerLibrary* obj) {
  UpdateIcon();
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, StatusAreaButton implementation:

void PowerMenuButton::DrawPressed(gfx::Canvas* canvas) {
  DrawPowerIcon(canvas, *ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_STATUSBAR_BATTERY_PRESSED));
}

void PowerMenuButton::DrawIcon(gfx::Canvas* canvas) {
  DrawPowerIcon(canvas, icon());
}

void PowerMenuButton::DrawPowerIcon(gfx::Canvas* canvas, SkBitmap icon) {
  // Draw the battery icon 6 pixels down to center it.
  // Because the status icon is 24x24 but the images are 24x16.
  // But since the images are shifted up by 4 pixels, we draw at 6 pixels down.
  static const int kIconVerticalPadding = 6;
  canvas->DrawBitmapInt(icon, 0, kIconVerticalPadding);
}

void PowerMenuButton::UpdateIcon() {
  PowerLibrary* cros = CrosLibrary::Get()->GetPowerLibrary();
  icon_id_ = IDR_STATUSBAR_BATTERY_UNKNOWN;
  if (CrosLibrary::Get()->EnsureLoaded()) {
    if (!cros->battery_is_present()) {
      icon_id_ = IDR_STATUSBAR_BATTERY_MISSING;
    } else if (cros->line_power_on() && cros->battery_fully_charged()) {
      icon_id_ = IDR_STATUSBAR_BATTERY_CHARGED;
    } else {
      // If fully charged, always show 100% even if percentage is a bit less.
      double percent = cros->battery_fully_charged() ? 100 :
          cros->battery_percentage();
      // Gets the power image depending on battery percentage. Percentage is
      // from 0 to 100, so we need to convert that to 0 to kNumPowerImages - 1.
      int index = static_cast<int>(percent / 100.0 *
                  nextafter(static_cast<float>(kNumPowerImages), 0));
      // Make sure that index is between 0 and kNumWifiImages - 1
      if (index < 0)
        index = 0;
      if (index >= kNumPowerImages)
        index = kNumPowerImages - 1;
      if (cros->line_power_on())
        icon_id_ = IDR_STATUSBAR_BATTERY_CHARGING_1 + index;
      else
        icon_id_ = IDR_STATUSBAR_BATTERY_DISCHARGING_1 + index;
    }
  }
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(icon_id_));
  SchedulePaint();
}

}  // namespace chromeos
