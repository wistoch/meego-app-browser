// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_COMBOBOX_MODEL_H_
#define APP_COMBOBOX_MODEL_H_
#pragma once

#include "base/string16.h"

// The interface for models backing a combobox.
class ComboboxModel {
 public:
  virtual ~ComboboxModel() {}

  // Return the number of items in the combo box.
  virtual int GetItemCount() = 0;

  // Return the string that should be used to represent a given item.
  virtual string16 GetItemAt(int index) = 0;
};

#endif  // APP_COMBOBOX_MODEL_H_
