// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "base/basictypes.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/common/resource_bundle.h"

#include "grit/theme_resources.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

}

NineBox* DownloadItemGtk::body_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::menu_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_active_ = NULL;

DownloadItemGtk::DownloadItemGtk(BaseDownloadItemModel* download_model,
                                 GtkWidget* parent_shelf)
    : download_model_(download_model),
      parent_shelf_(parent_shelf) {
  InitNineBoxes();

  body_ = gtk_button_new();
  gtk_widget_set_app_paintable(body_, TRUE);
  g_signal_connect(G_OBJECT(body_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(body_, GTK_CAN_FOCUS);
  GtkWidget* label = gtk_label_new(download_model->download()->GetFileName()
      .value().c_str());
  gtk_container_add(GTK_CONTAINER(body_), label);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  GTK_WIDGET_UNSET_FLAGS(menu_button_, GTK_CAN_FOCUS);
  g_signal_connect(G_OBJECT(menu_button_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), body_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent_shelf), hbox_, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox_);
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  GdkPixbuf* images[9];
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  int i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);
  body_nine_box_normal_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);
  body_nine_box_prelight_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);
  body_nine_box_active_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_normal_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP_H);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_prelight_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP_P);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_active_ = new NineBox(images);
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

  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                     true,  // alpha
                                     8,  // bits per channel
                                     widget->allocation.width,
                                     widget->allocation.height);

  nine_box->RenderToPixbuf(pixbuf);

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);

  gdk_pixbuf_unref(pixbuf);

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

