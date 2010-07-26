// Copyright (c) 2010 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#pragma once

#include "views/controls/button/checkbox.h"

namespace views {

class NativeRadioButtonGtk;

// A Checkbox subclass representing a radio button.
class RadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked);

  // Overridden from View:
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual View* GetSelectedViewForGroup(int group_id);
  virtual bool IsGroupFocusTraversable() const;
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);

 protected:
  virtual std::string GetClassName() const;

  // Overridden from NativeButton:
  virtual NativeButtonWrapper* CreateWrapper();

 private:
  friend class NativeRadioButtonGtk;

  // Accessor for |native_wrapper_|.
  NativeButtonWrapper* native_wrapper() { return native_wrapper_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
