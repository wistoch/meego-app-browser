// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_

#include "chrome/views/controls/native_control_win.h"
#include "chrome/views/controls/button/native_button_wrapper.h"

namespace views {

// A View that hosts a native Windows button.
class NativeButtonWin : public NativeControlWin,
                        public NativeButtonWrapper {
 public:
  explicit NativeButtonWin(NativeButton2* native_button);
  virtual ~NativeButtonWin();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateLabel();
  virtual void UpdateFont();
  virtual void UpdateDefault();
  virtual View* GetView();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from NativeControlWin:
  virtual LRESULT ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param);
  virtual bool OnKeyDown(int vkey);

 protected:
  virtual bool NotifyOnKeyDown() const;
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND control_hwnd);

 private:
  // The NativeButton we are bound to.
  NativeButton2* native_button_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonWin);
};

// A View that hosts a native Windows checkbox.
class NativeCheckboxWin : public NativeButtonWin {
 public:
  explicit NativeCheckboxWin(Checkbox2* native_button);
  virtual ~NativeCheckboxWin();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateChecked();
  virtual void SetHighlight(bool highlight);

  // Overridden from NativeControlWin:
  virtual LRESULT ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param);

 protected:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND control_hwnd);

 private:
  // The Checkbox we are bound to.
  Checkbox2* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxWin);
};

// A View that hosts a native Windows radio button.
class NativeRadioButtonWin : public NativeCheckboxWin {
 public:
  explicit NativeRadioButtonWin(RadioButton2* radio_button);
  virtual ~NativeRadioButtonWin();

 protected:
  // Overridden from NativeCheckboxWin:
  virtual void CreateNativeControl();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeRadioButtonWin);
};
  
}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
