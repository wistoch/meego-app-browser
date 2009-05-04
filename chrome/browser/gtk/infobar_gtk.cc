// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"
#include "chrome/browser/gtk/link_button_gtk.h"
#include "chrome/common/gtk_util.h"

namespace {

// TODO(estade): The background should be a gradient. For now we just use this
// solid color.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(250, 230, 145);

// Border color (the top pixel of the infobar).
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

// The total height of the info bar.
const int kInfoBarHeight = 37;

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

}  // namespace

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : container_(NULL),
      delegate_(delegate) {
  // Create |hbox_| and pad the sides.
  hbox_ = gtk_hbox_new(FALSE, kElementPadding);
  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      0, 0, kLeftPadding, kRightPadding);

  GtkWidget* bg_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_container_add(GTK_CONTAINER(bg_box), padding);

  // Set the top border and background color.
  gtk_widget_modify_bg(bg_box, GTK_STATE_NORMAL, &kBackgroundColor);
  border_bin_.Own(gfx::CreateGtkBorderBin(bg_box, &kBorderColor,
                                          0, 1, 0, 0));
  gtk_widget_set_size_request(border_bin_.get(), -1, kInfoBarHeight);

  // Add the icon on the left, if any.
  SkBitmap* icon = delegate->GetIcon();
  if (icon) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
  }

  close_button_.reset(CustomDrawButton::AddBarCloseButton(hbox_, 0));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButton), this);

  slide_widget_.reset(new SlideAnimatorGtk(border_bin_.get(),
                                           SlideAnimatorGtk::DOWN,
                                           0, true, this));
  // We store a pointer back to |this| so we can refer to it from the infobar
  // container.
  g_object_set_data(G_OBJECT(slide_widget_->widget()), "info-bar", this);
}

InfoBar::~InfoBar() {
  border_bin_.Destroy();
}

GtkWidget* InfoBar::widget() {
  return slide_widget_->widget();
}

void InfoBar::AnimateOpen() {
  slide_widget_->Open();
}

void InfoBar::Open() {
  slide_widget_->OpenWithoutAnimation();
  if (border_bin_.get()->window)
    gdk_window_lower(border_bin_.get()->window);
}

void InfoBar::AnimateClose() {
  slide_widget_->Close();
}

void InfoBar::Close() {
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
  delete this;
}

void InfoBar::RemoveInfoBar() const {
  container_->RemoveDelegate(delegate_);
}

void InfoBar::Closed() {
  Close();
}

// static
void InfoBar::OnCloseButton(GtkWidget* button, InfoBar* info_bar) {
  info_bar->RemoveInfoBar();
}

// AlertInfoBar ----------------------------------------------------------------

class AlertInfoBar : public InfoBar {
 public:
  AlertInfoBar(AlertInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    std::wstring text = delegate->GetMessageText();
    GtkWidget* label = gtk_label_new(WideToUTF8(text).c_str());
    gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 0);
  }
};

// LinkInfoBar -----------------------------------------------------------------

class LinkInfoBar : public InfoBar {
 public:
  LinkInfoBar(LinkInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    size_t link_offset;
    std::wstring display_text =
        delegate->GetMessageTextWithOffset(&link_offset);
    std::wstring link_text = delegate->GetLinkText();

    // Create the link button.
    link_button_.reset(new LinkButtonGtk(WideToUTF8(link_text).c_str()));
    g_signal_connect(link_button_->widget(), "clicked",
                     G_CALLBACK(OnLinkClick), this);

    // If link_offset is npos, we right-align the link instead of embedding it
    // in the text.
    if (link_offset == std::wstring::npos) {
      gtk_box_pack_end(GTK_BOX(hbox_), link_button_->widget(), FALSE, FALSE, 0);
      GtkWidget* label = gtk_label_new(WideToUTF8(display_text).c_str());
      gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 0);
    } else {
      GtkWidget* initial_label = gtk_label_new(
          WideToUTF8(display_text.substr(0, link_offset)).c_str());
      GtkWidget* trailing_label = gtk_label_new(
          WideToUTF8(display_text.substr(link_offset)).c_str());

      // We don't want any spacing between the elements, so we pack them into
      // this hbox that doesn't use kElementPadding.
      GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), initial_label, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), link_button_->widget(),
                         FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), trailing_label, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox_), hbox, FALSE, FALSE, 0);
    }
  }

 private:
  static void OnLinkClick(GtkWidget* button, LinkInfoBar* link_info_bar) {
    // TODO(estade): we need an equivalent for DispositionFromEventFlags().
    if (link_info_bar->delegate_->AsLinkInfoBarDelegate()->
        LinkClicked(CURRENT_TAB)) {
      link_info_bar->RemoveInfoBar();
    }
  }

  // The clickable link text.
  scoped_ptr<LinkButtonGtk> link_button_;
};

// ConfirmInfoBar --------------------------------------------------------------

class ConfirmInfoBar : public AlertInfoBar {
 public:
  ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
      : AlertInfoBar(delegate) {
    AddConfirmButton(ConfirmInfoBarDelegate::BUTTON_CANCEL);
    AddConfirmButton(ConfirmInfoBarDelegate::BUTTON_OK);
  }

 private:
  // Adds a button to the info bar by type. It will do nothing if the delegate
  // doesn't specify a button of the given type.
  void AddConfirmButton(ConfirmInfoBarDelegate::InfoBarButton type) {
    if (delegate_->AsConfirmInfoBarDelegate()->GetButtons() & type) {
      GtkWidget* button = gtk_button_new_with_label(WideToUTF8(
          delegate_->AsConfirmInfoBarDelegate()->GetButtonLabel(type)).c_str());
      GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
      gtk_box_pack_end(GTK_BOX(centering_vbox), button, TRUE, FALSE, 0);
      gtk_box_pack_end(GTK_BOX(hbox_), centering_vbox, FALSE, FALSE, 0);
      g_signal_connect(button, "clicked",
                       G_CALLBACK(type == ConfirmInfoBarDelegate::BUTTON_OK ?
                                  OnOkButton : OnCancelButton),
                       this);
    }
  }

  static void OnCancelButton(GtkWidget* button, ConfirmInfoBar* info_bar) {
    if (info_bar->delegate_->AsConfirmInfoBarDelegate()->Cancel())
      info_bar->RemoveInfoBar();
  }

  static void OnOkButton(GtkWidget* button, ConfirmInfoBar* info_bar) {
    if (info_bar->delegate_->AsConfirmInfoBarDelegate()->Accept())
      info_bar->RemoveInfoBar();
  }
};

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
