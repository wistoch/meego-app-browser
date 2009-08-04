// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ACCELERATOR_TABLE_GTK_H_
#define CHROME_BROWSER_VIEWS_ACCELERATOR_TABLE_GTK_H_

#include <gtk/gtk.h>

// This contains the list of accelerators shared between the Linux Gtk and
// toolkit_view implementation.
namespace browser {

  struct AcceleratorMapping {
    guint keyval;
    int command_id;
    GdkModifierType modifier_type;
  };

  // The list of accelerators.
  extern const AcceleratorMapping kAcceleratorMap[];

  // The numbers of elements in kAcceleratorMap.
  extern const size_t kAcceleratorMapLength;
}

#endif  // CHROME_BROWSER_VIEWS_ACCELERATOR_TABLE_GTK_H_
