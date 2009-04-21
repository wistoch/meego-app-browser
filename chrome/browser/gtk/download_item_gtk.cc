// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/text_elider.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

// Amount of space we allot to showing the filename. If the filename is too wide
// it will be elided.
const int kTextWidth = 140;

const char* kLabelColorMarkup = "<span color='#%s'>%s</span>";
const char* kFilenameColor = "576C95";  // 87, 108, 149
const char* kStatusColor = "7B8DAE";  // 123, 141, 174

}  // namespace

// DownloadShelfContextMenuGtk -------------------------------------------------

class DownloadShelfContextMenuGtk : public DownloadShelfContextMenu,
                                    public MenuGtk::Delegate {
 public:
  // The constructor creates the menu and immediately pops it up.
  // |model| is the download item model associated with this context menu,
  // |widget| is the button that popped up this context menu, and |e| is
  // the button press event that caused this menu to be created.
  DownloadShelfContextMenuGtk(BaseDownloadItemModel* model)
      : DownloadShelfContextMenu(model),
        menu_is_for_complete_download_(false) {
  }

  ~DownloadShelfContextMenuGtk() {
  }

  void Popup(GtkWidget* widget, GdkEvent* event) {
    // Create the menu if we have not created it yet or we created it for
    // an in-progress download that has since completed.
    bool download_is_complete = download_->state() == DownloadItem::COMPLETE;
    if (menu_.get() == NULL ||
        (download_is_complete && !menu_is_for_complete_download_)) {
      menu_.reset(new MenuGtk(this, download_is_complete ?
          finished_download_menu : in_progress_download_menu, NULL));
      menu_is_for_complete_download_ = download_is_complete;
    }
    menu_->Popup(widget, event);
  }

  // MenuGtk::Delegate implementation ------------------------------------------
  virtual bool IsCommandEnabled(int id) const {
    return IsItemCommandEnabled(id);
  }

  virtual bool IsItemChecked(int id) const {
    return ItemIsChecked(id);
  }

  virtual void ExecuteCommand(int id) {
    return ExecuteItemCommand(id);
  }

 private:
  // The menu we show on Popup(). We keep a pointer to it for a couple reasons:
  //  * we don't want to have to recreate the menu every time it's popped up.
  //  * we have to keep it in scope for longer than the duration of Popup(), or
  //    completing the user-selected action races against the menu's
  //    destruction.
  scoped_ptr<MenuGtk> menu_;

  // If true, the MenuGtk in |menu_| refers to a finished download menu.
  bool menu_is_for_complete_download_;

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  static MenuCreateMaterial in_progress_download_menu[];

  static MenuCreateMaterial finished_download_menu[];
};

MenuCreateMaterial DownloadShelfContextMenuGtk::finished_download_menu[] = {
  { MENU_NORMAL, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN, 0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_LINK_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

MenuCreateMaterial DownloadShelfContextMenuGtk::in_progress_download_menu[] = {
  { MENU_CHECKBOX, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE,
    0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_LINK_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

// DownloadItemGtk -------------------------------------------------------------

NineBox* DownloadItemGtk::body_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::menu_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_active_ = NULL;

DownloadItemGtk::DownloadItemGtk(BaseDownloadItemModel* download_model,
                                 GtkWidget* parent_shelf,
                                 GtkWidget* bounding_widget)
    : download_model_(download_model),
      parent_shelf_(parent_shelf),
      bounding_widget_(bounding_widget) {
  InitNineBoxes();

  body_ = gtk_button_new();
  // Eventually we will show an icon and graphical download progress, but for
  // now the only contents of body_ is text, so to make its size request the
  // same as the width of the text (plus a little padding: see below).
  gtk_widget_set_size_request(body_, kTextWidth + 50, -1);
  gtk_widget_set_app_paintable(body_, TRUE);
  g_signal_connect(G_OBJECT(body_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(body_, GTK_CAN_FOCUS);
  GtkWidget* name_label = gtk_label_new(NULL);
  // TODO(estade): This is at best an educated guess, since we don't actually
  // use ChromeFont() to draw the text. This is why we need to add so
  // much padding when we set the size request. We need to either use ChromeFont
  // or somehow extend TextElider.
  std::wstring elided_filename = gfx::ElideFilename(
      download_model->download()->GetFileName().ToWStringHack(),
      ChromeFont(), kTextWidth);
  gchar* label_markup =
      g_markup_printf_escaped(kLabelColorMarkup, kFilenameColor,
                              WideToUTF8(elided_filename).c_str());
  gtk_label_set_markup(GTK_LABEL(name_label), label_markup);
  g_free(label_markup);
  status_label_ = gtk_label_new(NULL);
  // Left align and vertically center the labels.
  gtk_misc_set_alignment(GTK_MISC(name_label), 0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(status_label_), 0, 0.5);

  // Stack the labels on top of one another.
  GtkWidget* text_stack = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), name_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), status_label_, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(body_), text_stack);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  GTK_WIDGET_UNSET_FLAGS(menu_button_, GTK_CAN_FOCUS);
  g_signal_connect(G_OBJECT(menu_button_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(G_OBJECT(menu_button_), "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  g_object_set_data(G_OBJECT(menu_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), body_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent_shelf), hbox_, FALSE, FALSE, 0);
  // Insert as the leftmost item.
  gtk_box_reorder_child(GTK_BOX(parent_shelf), hbox_, 1);
  gtk_widget_show_all(hbox_);

  g_signal_connect(G_OBJECT(parent_shelf_), "size-allocate",
                   G_CALLBACK(OnShelfResized), this);

  download_model_->download()->AddObserver(this);
}

DownloadItemGtk::~DownloadItemGtk() {
  download_model_->download()->RemoveObserver(this);
}

void DownloadItemGtk::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());
  if (!status_label_) {
    return;
  }

  std::wstring status_text = download_model_->GetStatusText();
  // Remove the status text label.
  if (status_text.empty()) {
    gtk_widget_destroy(status_label_);
    status_label_ = NULL;
    return;
  }

  gchar* label_markup =
      g_markup_printf_escaped(kLabelColorMarkup, kStatusColor,
                              WideToUTF8(status_text).c_str());
  gtk_label_set_markup(GTK_LABEL(status_label_), label_markup);
  g_free(label_markup);
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  body_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);

  body_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_H,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);

  body_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_P,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);

  menu_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM, 0, 0);

  menu_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H, 0, 0);

  menu_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P, 0, 0);
}

// static
gboolean DownloadItemGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                   DownloadItemGtk* download_item) {
  NineBox* nine_box = NULL;
  // If true, this widget is |body_|, otherwise it is |menu_button_|.
  bool is_body = widget == download_item->body_;
  if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
    nine_box = is_body ? body_nine_box_prelight_ : menu_nine_box_prelight_;
  else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
    nine_box = is_body ? body_nine_box_active_ : menu_nine_box_active_;
  else
    nine_box = is_body ? body_nine_box_normal_ : menu_nine_box_normal_;

  nine_box->RenderToWidget(widget);

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

gboolean DownloadItemGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                 GdkEvent* event,
                                                 DownloadItemGtk* item) {
  // TODO(port): this never puts the button into the "active" state,
  // so this may need to be changed. See note in BrowserToolbarGtk.
  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      if (item->menu_.get() == NULL) {
        item->menu_.reset(new DownloadShelfContextMenuGtk(
            item->download_model_.get()));
      }
      item->menu_->Popup(button, event);
    }
  }

  return FALSE;
}

void DownloadItemGtk::OnShelfResized(GtkWidget *widget,
                                     GtkAllocation *allocation,
                                     DownloadItemGtk* item) {
  if (item->hbox_->allocation.x + item->hbox_->allocation.width >
      item->bounding_widget_->allocation.x)
    gtk_widget_hide(item->hbox_);
  else
    gtk_widget_show(item->hbox_);
}
