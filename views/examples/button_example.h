// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#define VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"

namespace examples {

// ButtonExample simply counts the number of clicks.
class ButtonExample : protected ExampleBase, private views::ButtonListener {
 public:
  explicit ButtonExample(ExamplesMain* main) : ExampleBase(main), count_(0) {
    button_ = new views::TextButton(this, L"Button");
  }

  virtual ~ButtonExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Text Button";
  }

  virtual views::View* GetExampleView() {
    return button_;
  }

 private:
  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    PrintStatus(L"Pressed! count:%d", ++count_);
  }

  // The only control in this test.
  views::TextButton* button_;

  // The number of times the button is pressed.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(ButtonExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

