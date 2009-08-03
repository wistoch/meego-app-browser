// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include "chrome/browser/accelerator_table_linux.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"

namespace browser {

// Keep this in sync with various context menus which display the accelerators.
const AcceleratorMapping kAcceleratorMap[] = {
  // Focus.
  { GDK_k, IDC_FOCUS_SEARCH, GDK_CONTROL_MASK },
  { GDK_e, IDC_FOCUS_SEARCH, GDK_CONTROL_MASK },
  { XF86XK_Search, IDC_FOCUS_SEARCH, GdkModifierType(0) },
  { GDK_l, IDC_FOCUS_LOCATION, GDK_CONTROL_MASK },
  { GDK_d, IDC_FOCUS_LOCATION, GDK_MOD1_MASK },
  { GDK_F6, IDC_FOCUS_LOCATION, GdkModifierType(0) },
  { XF86XK_OpenURL, IDC_FOCUS_LOCATION, GdkModifierType(0) },
  { XF86XK_Go, IDC_FOCUS_LOCATION, GdkModifierType(0) },

  // Tab/window controls.
  { GDK_Page_Down, IDC_SELECT_NEXT_TAB, GDK_CONTROL_MASK },
  { GDK_Page_Up, IDC_SELECT_PREVIOUS_TAB, GDK_CONTROL_MASK },
  { GDK_w, IDC_CLOSE_TAB, GDK_CONTROL_MASK },
  { GDK_t, IDC_RESTORE_TAB,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

  { GDK_1, IDC_SELECT_TAB_0, GDK_CONTROL_MASK },
  { GDK_2, IDC_SELECT_TAB_1, GDK_CONTROL_MASK },
  { GDK_3, IDC_SELECT_TAB_2, GDK_CONTROL_MASK },
  { GDK_4, IDC_SELECT_TAB_3, GDK_CONTROL_MASK },
  { GDK_5, IDC_SELECT_TAB_4, GDK_CONTROL_MASK },
  { GDK_6, IDC_SELECT_TAB_5, GDK_CONTROL_MASK },
  { GDK_7, IDC_SELECT_TAB_6, GDK_CONTROL_MASK },
  { GDK_8, IDC_SELECT_TAB_7, GDK_CONTROL_MASK },
  { GDK_9, IDC_SELECT_LAST_TAB, GDK_CONTROL_MASK },

  { GDK_1, IDC_SELECT_TAB_0, GDK_MOD1_MASK },
  { GDK_2, IDC_SELECT_TAB_1, GDK_MOD1_MASK },
  { GDK_3, IDC_SELECT_TAB_2, GDK_MOD1_MASK },
  { GDK_4, IDC_SELECT_TAB_3, GDK_MOD1_MASK },
  { GDK_5, IDC_SELECT_TAB_4, GDK_MOD1_MASK },
  { GDK_6, IDC_SELECT_TAB_5, GDK_MOD1_MASK },
  { GDK_7, IDC_SELECT_TAB_6, GDK_MOD1_MASK },
  { GDK_8, IDC_SELECT_TAB_7, GDK_MOD1_MASK },
  { GDK_9, IDC_SELECT_LAST_TAB, GDK_MOD1_MASK },

  { GDK_KP_1, IDC_SELECT_TAB_0, GDK_CONTROL_MASK },
  { GDK_KP_2, IDC_SELECT_TAB_1, GDK_CONTROL_MASK },
  { GDK_KP_3, IDC_SELECT_TAB_2, GDK_CONTROL_MASK },
  { GDK_KP_4, IDC_SELECT_TAB_3, GDK_CONTROL_MASK },
  { GDK_KP_5, IDC_SELECT_TAB_4, GDK_CONTROL_MASK },
  { GDK_KP_6, IDC_SELECT_TAB_5, GDK_CONTROL_MASK },
  { GDK_KP_7, IDC_SELECT_TAB_6, GDK_CONTROL_MASK },
  { GDK_KP_8, IDC_SELECT_TAB_7, GDK_CONTROL_MASK },
  { GDK_KP_9, IDC_SELECT_LAST_TAB, GDK_CONTROL_MASK },

  { GDK_KP_1, IDC_SELECT_TAB_0, GDK_MOD1_MASK },
  { GDK_KP_2, IDC_SELECT_TAB_1, GDK_MOD1_MASK },
  { GDK_KP_3, IDC_SELECT_TAB_2, GDK_MOD1_MASK },
  { GDK_KP_4, IDC_SELECT_TAB_3, GDK_MOD1_MASK },
  { GDK_KP_5, IDC_SELECT_TAB_4, GDK_MOD1_MASK },
  { GDK_KP_6, IDC_SELECT_TAB_5, GDK_MOD1_MASK },
  { GDK_KP_7, IDC_SELECT_TAB_6, GDK_MOD1_MASK },
  { GDK_KP_8, IDC_SELECT_TAB_7, GDK_MOD1_MASK },
  { GDK_KP_9, IDC_SELECT_LAST_TAB, GDK_MOD1_MASK },

  { GDK_F4, IDC_CLOSE_TAB, GDK_CONTROL_MASK },
  { GDK_F4, IDC_CLOSE_WINDOW, GDK_MOD1_MASK },

  // Zoom level.
  { GDK_plus, IDC_ZOOM_PLUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_equal, IDC_ZOOM_PLUS, GDK_CONTROL_MASK },
  { XF86XK_ZoomIn, IDC_ZOOM_PLUS, GdkModifierType(0) },
  { GDK_0, IDC_ZOOM_NORMAL, GDK_CONTROL_MASK },
  { GDK_minus, IDC_ZOOM_MINUS, GDK_CONTROL_MASK },
  { GDK_underscore, IDC_ZOOM_MINUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { XF86XK_ZoomOut, IDC_ZOOM_MINUS, GdkModifierType(0) },

  // Find in page.
  { GDK_g, IDC_FIND_NEXT, GDK_CONTROL_MASK },
  { GDK_F3, IDC_FIND_NEXT, GdkModifierType(0) },
  { GDK_g, IDC_FIND_PREVIOUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_F3, IDC_FIND_PREVIOUS, GDK_SHIFT_MASK },

  // Navigation / toolbar buttons.
  { GDK_Home, IDC_HOME, GDK_MOD1_MASK },
  { XF86XK_HomePage, IDC_HOME, GdkModifierType(0) },
  { GDK_Escape, IDC_STOP, GdkModifierType(0) },
  { XF86XK_Stop, IDC_STOP, GdkModifierType(0) },
  { GDK_Left, IDC_BACK, GDK_MOD1_MASK },
  { GDK_BackSpace, IDC_BACK, GdkModifierType(0) },
  { XF86XK_Back, IDC_BACK, GdkModifierType(0) },
  { GDK_Right, IDC_FORWARD, GDK_MOD1_MASK },
  { GDK_BackSpace, IDC_FORWARD, GDK_SHIFT_MASK },
  { XF86XK_Forward, IDC_FORWARD, GdkModifierType(0) },
  { GDK_r, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD, GdkModifierType(0) },
  { GDK_F5, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD, GDK_SHIFT_MASK },
  { XF86XK_Reload, IDC_RELOAD, GdkModifierType(0) },
  { XF86XK_Refresh, IDC_RELOAD, GdkModifierType(0) },

  // Miscellany.
  { GDK_d, IDC_STAR, GDK_CONTROL_MASK },
  { XF86XK_AddFavorite, IDC_STAR, GdkModifierType(0) },
  { XF86XK_Favorites, IDC_SHOW_BOOKMARK_BAR, GdkModifierType(0) },
  { XF86XK_History, IDC_SHOW_HISTORY, GdkModifierType(0) },
  { GDK_o, IDC_OPEN_FILE, GDK_CONTROL_MASK },
  { GDK_F11, IDC_FULLSCREEN, GdkModifierType(0) },
  { GDK_u, IDC_VIEW_SOURCE, GDK_CONTROL_MASK },
  { GDK_p, IDC_PRINT, GDK_CONTROL_MASK },
  { GDK_Escape, IDC_TASK_MANAGER, GDK_SHIFT_MASK },

#if defined(OS_CHROMEOS)
  { GDK_f, IDC_FULLSCREEN,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_Delete, IDC_TASK_MANAGER,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_comma, IDC_CONTROL_PANEL, GdkModifierType(GDK_CONTROL_MASK) },
#endif
};

const size_t kAcceleratorMapLength = arraysize(kAcceleratorMap);

}  // namespace browser
