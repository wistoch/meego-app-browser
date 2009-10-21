// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_POWER_MENU_BUTTON_H_

#include "chrome/browser/chromeos/cros_power_library.h"
#include "chrome/browser/chromeos/status_area_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class CrosPowerLibrary;
class SkBitmap;

// The power menu button in the status area.
// This class will handle getting the power status and populating the menu.
class PowerMenuButton : public StatusAreaButton,
                        public views::ViewMenuDelegate,
                        public views::Menu2Model,
                        public CrosPowerLibrary::Observer {
 public:
  PowerMenuButton();
  virtual ~PowerMenuButton();

  // views::Menu2Model implementation.
  virtual bool HasIcons() const  { return false; }
  virtual int GetItemCount() const;
  virtual views::Menu2Model::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      views::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const { return false; }
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const { return false; }
  virtual bool IsEnabledAt(int index) const { return false; }
  virtual Menu2Model* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index) {}
  virtual void MenuWillShow() {}

  // CrosPowerLibrary::Observer implementation.
  virtual void PowerChanged(CrosPowerLibrary* obj);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Update the power icon depending on the power status.
  void UpdateIcon();

  // The number of power images.
  static const int kNumPowerImages;

  // The power menu.
  views::Menu2 power_menu_;

  DISALLOW_COPY_AND_ASSIGN(PowerMenuButton);
};

#endif  // CHROME_BROWSER_CHROMEOS_POWER_MENU_BUTTON_H_
