// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX2_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX2_H_

#include "chrome/views/controls/button/native_button2.h"

namespace views {

class Label;

// A NativeButton subclass representing a checkbox.
class Checkbox2 : public NativeButton2 {
 public:
  // The button's class name.
  static const char kViewClassName[];

  Checkbox2();
  explicit Checkbox2(ButtonListener* listener);
  Checkbox2(ButtonListener* listener, const std::wstring& label);
  virtual ~Checkbox2();

  // Sets whether or not the checkbox label should wrap multiple lines of text.
  // If true, long lines are wrapped, and this is reflected in the preferred
  // size returned by GetPreferredSize. If false, text that will not fit within
  // the available bounds for the label will be cropped.
  void SetMultiline(bool multiline);

  // Sets/Gets whether or not the checkbox is checked.
  void SetChecked(bool checked);
  bool checked() const { return checked_; }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 protected:
  virtual std::string GetClassName() const;

  // Overridden from NativeButton2:
  virtual void CreateWrapper();
  virtual void InitBorder();

 private:
  // Called from the constructor to create and configure the checkbox label.
  void CreateLabel(const std::wstring& label_text);

  // The checkbox's label. We don't use the OS version because of transparency
  // and sizing issues.
  Label* label_;

  // True if the checkbox is checked. 
  bool checked_;

  DISALLOW_COPY_AND_ASSIGN(Checkbox2);
};

// TODO(beng): move to own file and un-stub:
class RadioButton2 : public Checkbox2 {

};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX2_H_
