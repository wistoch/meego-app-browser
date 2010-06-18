// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
#define VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

#include "gfx/font.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {

// Layout type information for menu items. Use the instance() method to obtain
// the MenuConfig for the current platform.
struct MenuConfig {
  MenuConfig()
      : text_color(SK_ColorBLACK),
        item_top_margin(3),
        item_bottom_margin(4),
        item_no_icon_top_margin(1),
        item_no_icon_bottom_margin(3),
        item_left_margin(4),
        label_to_arrow_padding(10),
        arrow_to_edge_padding(5),
        icon_to_label_padding(8),
        gutter_to_label(5),
        check_width(16),
        check_height(16),
        radio_width(16),
        radio_height(16),
        arrow_height(9),
        arrow_width(9),
        gutter_width(0),
        separator_height(6),
        render_gutter(false),
        show_mnemonics(false),
        scroll_arrow_height(3),
        label_to_accelerator_padding(10) {
  }

  // Resets the single shared MenuConfig instance. The next time instance() is
  // invoked a new MenuConfig is created and configured.
  static void Reset();

  // Returns the single shared MenuConfig instance, creating if necessary.
  static const MenuConfig& instance();

  // Font used by menus.
  gfx::Font font;

  // Normal text color.
  SkColor text_color;

  // Margins between the top of the item and the label.
  int item_top_margin;

  // Margins between the bottom of the item and the label.
  int item_bottom_margin;

  // Margins used if the menu doesn't have icons.
  int item_no_icon_top_margin;
  int item_no_icon_bottom_margin;

  // Margins between the left of the item and the icon.
  int item_left_margin;

  // Padding between the label and submenu arrow.
  int label_to_arrow_padding;

  // Padding between the arrow and the edge.
  int arrow_to_edge_padding;

  // Padding between the icon and label.
  int icon_to_label_padding;

  // Padding between the gutter and label.
  int gutter_to_label;

  // Size of the check.
  int check_width;
  int check_height;

  // Size of the radio bullet.
  int radio_width;
  int radio_height;

  // Size of the submenu arrow.
  int arrow_height;
  int arrow_width;

  // Width of the gutter. Only used if render_gutter is true.
  int gutter_width;

  // Height of the separator.
  int separator_height;

  // Whether or not the gutter should be rendered. The gutter is specific to
  // Vista.
  bool render_gutter;

  // Are mnemonics shown?
  bool show_mnemonics;

  // Height of the scroll arrow.
  int scroll_arrow_height;

  // Padding between the label and accelerator. Only used if there is an
  // accelerator.
  int label_to_accelerator_padding;

 private:
  // Creates and configures a new MenuConfig as appropriate for the current
  // platform.
  static MenuConfig* Create();
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
