// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/custom_button.h"

#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "chrome/browser/gtk/nine_box.h"

#include "grit/theme_resources.h"

CustomDrawButtonBase::CustomDrawButtonBase(
    int normal_id,
    int active_id,
    int highlight_id,
    int depressed_id)
    : paint_override_(-1) {
  // Load the button images from the resource bundle.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  pixbufs_[GTK_STATE_NORMAL] = normal_id ? rb.GetPixbufNamed(normal_id) : NULL;
  pixbufs_[GTK_STATE_ACTIVE] = active_id ? rb.GetPixbufNamed(active_id) : NULL;
  pixbufs_[GTK_STATE_PRELIGHT] =
      highlight_id ? rb.GetPixbufNamed(highlight_id) : NULL;
  pixbufs_[GTK_STATE_SELECTED] = NULL;
  pixbufs_[GTK_STATE_INSENSITIVE] =
      depressed_id ? rb.GetPixbufNamed(depressed_id) : NULL;
}

CustomDrawButtonBase::~CustomDrawButtonBase() {
}

gboolean CustomDrawButtonBase::OnExpose(GtkWidget* widget, GdkEventExpose* e) {
  GdkPixbuf* pixbuf = pixbufs_[paint_override_ >= 0 ?
                               paint_override_ : GTK_WIDGET_STATE(widget)];

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf)
    pixbuf = pixbufs_[GTK_STATE_NORMAL];

  if (!pixbuf)
    return FALSE;

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);

  return TRUE;
}

CustomDrawButton::CustomDrawButton(
    int normal_id,
    int active_id,
    int highlight_id,
    int depressed_id)
    : button_base_(normal_id, active_id, highlight_id, depressed_id) {
  widget_.Own(gtk_button_new());
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);

  gtk_widget_set_size_request(widget_.get(),
                              gdk_pixbuf_get_width(button_base_.pixbufs(0)),
                              gdk_pixbuf_get_height(button_base_.pixbufs(0)));

  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_.get(), FALSE);
  g_signal_connect(G_OBJECT(widget_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

CustomDrawButton::~CustomDrawButton() {
  widget_.Destroy();
}

void CustomDrawButton::SetPaintOverride(GtkStateType state) {
  button_base_.set_paint_override(state);
  gtk_widget_queue_draw(widget_.get());
}

void CustomDrawButton::UnsetPaintOverride() {
  button_base_.set_paint_override(-1);
  gtk_widget_queue_draw(widget_.get());
}

// static
gboolean CustomDrawButton::OnExpose(GtkWidget* widget,
                                    GdkEventExpose* e,
                                    CustomDrawButton* button) {
  return button->button_base_.OnExpose(widget, e);
}

// static
CustomDrawButton* CustomDrawButton::AddBarCloseButton(GtkWidget* hbox,
                                                      int padding) {
  CustomDrawButton* rv = new CustomDrawButton(IDR_CLOSE_BAR, IDR_CLOSE_BAR_P,
                                              IDR_CLOSE_BAR_H, 0);
  GTK_WIDGET_UNSET_FLAGS(rv->widget(), GTK_CAN_FOCUS);
  GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(centering_vbox), rv->widget(), TRUE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, padding);
  return rv;
}
