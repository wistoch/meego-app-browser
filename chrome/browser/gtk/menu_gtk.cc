// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/menu_gtk.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/common/gtk_util.h"
#include "skia/include/SkBitmap.h"

namespace {

struct SetIconState {
  bool found;
  const SkBitmap* icon;
  int id;
};

void SetIconImpl(GtkWidget* widget, void* raw) {
  SetIconState* data = reinterpret_cast<SetIconState*>(raw);
  int this_id =
      reinterpret_cast<int>(g_object_get_data(G_OBJECT(widget), "menu-id"));

  if (this_id == data->id) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(data->icon);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
                                  gtk_image_new_from_pixbuf(pixbuf));
    g_object_unref(pixbuf);

    data->found = true;
  }
}

}  // namespace

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 const MenuCreateMaterial* menu_data,
                 GtkAccelGroup* accel_group)
    : delegate_(delegate),
      dummy_accel_group_(gtk_accel_group_new()),
      menu_(gtk_menu_new()) {
  BuildMenuIn(menu_.get(), menu_data, accel_group);
}

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate, bool load)
    : delegate_(delegate),
      dummy_accel_group_(NULL),
      menu_(gtk_menu_new()) {
  if (load)
    BuildMenuFromDelegate();
}

MenuGtk::~MenuGtk() {
  STLDeleteElements(&children_);
  menu_.Destroy();
  if (dummy_accel_group_)
    g_object_unref(dummy_accel_group_);
}

void MenuGtk::AppendMenuItemWithLabel(int command_id,
                                      const std::string& label) {
  GtkWidget* menu_item = gtk_menu_item_new_with_label(label.c_str());
  AddMenuItemWithId(menu_item, command_id);
}

void MenuGtk::AppendMenuItemWithIcon(int command_id,
                                     const std::string& label,
                                     const SkBitmap& icon) {
  GtkWidget* menu_item = BuildMenuItemWithImage(label, icon);
  AddMenuItemWithId(menu_item, command_id);
}

MenuGtk* MenuGtk::AppendSubMenuWithIcon(int command_id,
                                        const std::string& label,
                                        const SkBitmap& icon) {
  GtkWidget* menu_item = BuildMenuItemWithImage(label, icon);

  MenuGtk* submenu = new MenuGtk(delegate_, false);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu->menu_.get());
  children_.push_back(submenu);

  AddMenuItemWithId(menu_item, command_id);

  return submenu;
}

void MenuGtk::AppendSeparator() {
  GtkWidget* menu_item = gtk_separator_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_.get()), menu_item);
}

void MenuGtk::Popup(GtkWidget* widget, GdkEvent* event) {
  DCHECK(event->type == GDK_BUTTON_PRESS)
      << "Non-button press event sent to RunMenuAt";

  GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
  Popup(widget, event_button->button, event_button->time);
}

void MenuGtk::Popup(GtkWidget* widget, gint button_type, guint32 timestamp) {
  gtk_container_foreach(GTK_CONTAINER(menu_.get()), SetMenuItemInfo, this);

  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL,
                 MenuPositionFunc,
                 widget,
                 button_type, timestamp);
}

void MenuGtk::PopupAsContext(guint32 event_time) {
  gtk_container_foreach(GTK_CONTAINER(menu_.get()), SetMenuItemInfo, this);

  // TODO(estade): |button| value of 3 (6th argument) is not strictly true,
  // but does it matter?
  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL, NULL, NULL, 3, event_time);
}

void MenuGtk::Cancel() {
  gtk_menu_popdown(GTK_MENU(menu_.get()));
}

bool MenuGtk::SetIcon(const SkBitmap& icon, int item_id) {
  // First search items in this menu.
  SetIconState state;
  state.found = false;
  state.icon = &icon;
  state.id = item_id;
  gtk_container_foreach(GTK_CONTAINER(menu_.get()), SetIconImpl, &state);
  if (state.found)
    return true;

  for (std::vector<MenuGtk*>::iterator it = children_.begin();
       it != children_.end(); ++it) {
    if ((*it)->SetIcon(icon, item_id))
      return true;
  }

  return false;
}

// static
std::string MenuGtk::ConvertAcceleratorsFromWindowsStyle(
    const std::string& label) {
  std::string ret;
  ret.reserve(label.length());
  for (size_t i = 0; i < label.length(); ++i) {
    if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back(label[i]);
        ++i;
      } else {
        ret.push_back('_');
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

void MenuGtk::BuildMenuIn(GtkWidget* menu,
                          const MenuCreateMaterial* menu_data,
                          GtkAccelGroup* accel_group) {
  // We keep track of the last menu item in order to group radio items.
  GtkWidget* last_menu_item = NULL;
  for (; menu_data->type != MENU_END; ++menu_data) {
    GtkWidget* menu_item = NULL;

    std::string label;
    if (menu_data->label_argument) {
      label = l10n_util::GetStringFUTF8(
          menu_data->label_id,
          WideToUTF16(l10n_util::GetString(menu_data->label_argument)));
    } else if (menu_data->label_id) {
      label = l10n_util::GetStringUTF8(menu_data->label_id);
    } else if (menu_data->type != MENU_SEPARATOR) {
      label = delegate_->GetLabel(menu_data->id);
      DCHECK(!label.empty());
    }

    label = ConvertAcceleratorsFromWindowsStyle(label);

    switch (menu_data->type) {
      case MENU_RADIO:
        if (GTK_IS_RADIO_MENU_ITEM(last_menu_item)) {
          menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
              GTK_RADIO_MENU_ITEM(last_menu_item), label.c_str());
        } else {
          menu_item = gtk_radio_menu_item_new_with_mnemonic(
              NULL, label.c_str());
        }
        break;
      case MENU_CHECKBOX:
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
        break;
      case MENU_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;
      case MENU_NORMAL:
      default:
        menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());
        break;
    }

    if (menu_data->submenu) {
      GtkWidget* submenu = gtk_menu_new();
      BuildMenuIn(submenu, menu_data->submenu, accel_group);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    if (accel_group && menu_data->accel_key) {
      // If we ever want to let the user do any key remaping, we'll need to
      // change the following so we make a gtk_accel_map which keeps the actual
      // keys.
      gtk_widget_add_accelerator(menu_item,
                                 "activate",
                                 menu_data->only_show ? dummy_accel_group_ :
                                                        accel_group,
                                 menu_data->accel_key,
                                 GdkModifierType(menu_data->accel_modifiers),
                                 GTK_ACCEL_VISIBLE);
    }

    g_object_set_data(G_OBJECT(menu_item), "menu-data",
                      const_cast<MenuCreateMaterial*>(menu_data));

    g_signal_connect(G_OBJECT(menu_item), "activate",
                     G_CALLBACK(OnMenuItemActivated), this);

    gtk_widget_show(menu_item);
    gtk_menu_append(menu, menu_item);
    last_menu_item = menu_item;
  }
}

GtkWidget* MenuGtk::BuildMenuItemWithImage(const std::string& label,
                                           const SkBitmap& icon) {
  GtkWidget* menu_item = gtk_image_menu_item_new_with_label(label.c_str());

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);

  return menu_item;
}

void MenuGtk::BuildMenuFromDelegate() {
  // Note that the menu IDs start at 1, not 0.
  for (int i = 1; i <= delegate_->GetItemCount(); ++i) {
    GtkWidget* menu_item = NULL;

    if (delegate_->IsItemSeparator(i)) {
      menu_item = gtk_separator_menu_item_new();
    } else if (delegate_->HasIcon(i)) {
      const SkBitmap* icon = delegate_->GetIcon(i);
      menu_item = BuildMenuItemWithImage(delegate_->GetLabel(i), *icon);
    } else {
      menu_item = gtk_menu_item_new_with_label(delegate_->GetLabel(i).c_str());
    }

    AddMenuItemWithId(menu_item, i);
  }
}

void MenuGtk::AddMenuItemWithId(GtkWidget* menu_item, int id) {
  g_object_set_data(G_OBJECT(menu_item), "menu-id",
                    reinterpret_cast<void*>(id));

  g_signal_connect(G_OBJECT(menu_item), "activate",
                   G_CALLBACK(OnMenuItemActivatedById), this);

  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_.get()), menu_item);
}

// static
void MenuGtk::OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu) {
  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (!gtk_menu_item_get_submenu(menuitem)) {
    const MenuCreateMaterial* data =
        reinterpret_cast<const MenuCreateMaterial*>(
            g_object_get_data(G_OBJECT(menuitem), "menu-data"));
    // The menu item can still be activated by hotkeys even if it is disabled.
    if (menu->delegate_->IsCommandEnabled(data->id))
      menu->delegate_->ExecuteCommand(data->id);
  }
}

// static
void MenuGtk::OnMenuItemActivatedById(GtkMenuItem* menuitem, MenuGtk* menu) {
  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (!gtk_menu_item_get_submenu(menuitem)) {
    int id = reinterpret_cast<int>(
        g_object_get_data(G_OBJECT(menuitem), "menu-id"));
    // The menu item can still be activated by hotkeys even if it is disabled.
    if (menu->delegate_->IsCommandEnabled(id))
      menu->delegate_->ExecuteCommand(id);
  }
}

// static
void MenuGtk::MenuPositionFunc(GtkMenu* menu,
                               int* x,
                               int* y,
                               gboolean* push_in,
                               void* void_widget) {
  GtkWidget* widget = GTK_WIDGET(void_widget);
  GtkRequisition menu_req;

  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  gdk_window_get_origin(widget->window, x, y);
  GdkScreen *screen = gtk_widget_get_screen(widget);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor,
                                  &screen_rect);

  *x += widget->allocation.x;
  *y += widget->allocation.y + widget->allocation.height;

  // g_object_get_data() returns NULL if no such object is found. |left_align|
  // acts as a boolean, but we can't actually cast it to bool because gcc
  // complains about losing precision.
  if (!g_object_get_data(G_OBJECT(widget), "left-align-popup"))
    *x += widget->allocation.width - menu_req.width;

  if (*y + menu_req.height >= screen_rect.height)
    *y -= menu_req.height;

  *push_in = FALSE;
}

// static
void MenuGtk::SetMenuItemInfo(GtkWidget* widget, gpointer userdata) {
  MenuGtk* menu = reinterpret_cast<MenuGtk*>(userdata);
  int id;

  const MenuCreateMaterial* data =
      reinterpret_cast<const MenuCreateMaterial*>(
          g_object_get_data(G_OBJECT(widget), "menu-data"));
  if (data) {
    id = data->id;
  } else {
    id = reinterpret_cast<int>(
        g_object_get_data(G_OBJECT(widget), "menu-id"));
  }

  if (GTK_IS_CHECK_MENU_ITEM(widget)) {
    GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);
    gtk_check_menu_item_set_active(
        item, menu->delegate_->IsItemChecked(id));
  }

  if (GTK_IS_MENU_ITEM(widget)) {
    gtk_widget_set_sensitive(
        widget, menu->delegate_->IsCommandEnabled(id));

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
    if (submenu) {
      gtk_container_foreach(GTK_CONTAINER(submenu), &SetMenuItemInfo,
                            userdata);
    }
  }
}
