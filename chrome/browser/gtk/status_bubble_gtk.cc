// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/status_bubble_gtk.h"

#include <gtk/gtk.h>

#include "app/gfx/gtk_util.h"
#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

// Inner padding between the border and the text label.
const int kInternalTopBottomPadding = 1;
const int kInternalLeftRightPadding = 2;

// The radius of the edges of our bubble.
const int kCornerSize = 3;

// Milliseconds before we hide the status bubble widget when you mouseout.
const int kHideDelay = 250;

}  // namespace

StatusBubbleGtk::StatusBubbleGtk(Profile* profile)
    : theme_provider_(GtkThemeProvider::GetFrom(profile)),
      timer_factory_(this) {
  InitWidgets();

  theme_provider_->InitThemesFor(this);
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

StatusBubbleGtk::~StatusBubbleGtk() {
  container_.Destroy();
}

void StatusBubbleGtk::SetStatus(const std::wstring& status_text_wide) {
  std::string status_text = WideToUTF8(status_text_wide);
  if (status_text_ == status_text)
    return;

  status_text_ = status_text;
  if (!status_text_.empty()) {
    SetStatusTextTo(status_text_);
  } else if (!url_text_.empty()) {
    SetStatusTextTo(url_text_);
  } else {
    SetStatusTextTo(std::string());
  }
}

void StatusBubbleGtk::SetURL(const GURL& url, const std::wstring& languages) {
  // If we want to clear a displayed URL but there is a status still to
  // display, display that status instead.
  if (url.is_empty() && !status_text_.empty()) {
    url_text_ = std::string();
    SetStatusTextTo(status_text_);
    return;
  }

  // Set Elided Text corresponding to the GURL object.  We limit the width of
  // the URL to a third of the width of the browser window (matching the width
  // on Windows).
  GtkWidget* parent = gtk_widget_get_parent(container_.get());
  int window_width = parent->allocation.width;
  // TODO(tc): We don't actually use gfx::Font as the font in the status
  // bubble.  We should extend gfx::ElideUrl to take some sort of pango font.
  url_text_ = WideToUTF8(gfx::ElideUrl(url, gfx::Font(), window_width / 3,
                                       languages));
  SetStatusTextTo(url_text_);
}

void StatusBubbleGtk::Show() {
  // If we were going to hide, stop.
  timer_factory_.RevokeAll();

  gtk_widget_show_all(container_.get());

  if (container_->window)
    gdk_window_raise(container_->window);
}

void StatusBubbleGtk::Hide() {
  gtk_widget_hide_all(container_.get());
}

void StatusBubbleGtk::SetStatusTextTo(const std::string& status_utf8) {
  if (status_utf8.empty()) {
    HideInASecond();
  } else {
    gtk_label_set_text(GTK_LABEL(label_), status_utf8.c_str());
    Show();
  }
}

void StatusBubbleGtk::HideInASecond() {
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      timer_factory_.NewRunnableMethod(&StatusBubbleGtk::Hide),
      kHideDelay);
}

void StatusBubbleGtk::MouseMoved() {
  // We can't do that fancy sliding behaviour where the status bubble slides
  // out of the window because the window manager gets in the way. So totally
  // ignore this message for now.
  //
  // TODO(erg): At least get some sliding behaviour so that it slides out of
  // the way to hide the status bubble on mouseover.
}

void StatusBubbleGtk::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED) {
    UserChangedTheme();
  }
}

void StatusBubbleGtk::InitWidgets() {
  label_ = gtk_label_new(NULL);

  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      kInternalTopBottomPadding, kInternalTopBottomPadding,
      kInternalLeftRightPadding,
      kInternalLeftRightPadding + kCornerSize);
  gtk_container_add(GTK_CONTAINER(padding), label_);

  container_.Own(gtk_event_box_new());
  gtk_util::ActAsRoundedWindow(
      container_.get(), gfx::kGdkWhite, kCornerSize,
      gtk_util::ROUNDED_TOP_RIGHT,
      gtk_util::BORDER_TOP | gtk_util::BORDER_RIGHT);
  gtk_widget_set_name(container_.get(), "status-bubble");
  gtk_container_add(GTK_CONTAINER(container_.get()), padding);

  UserChangedTheme();
}

void StatusBubbleGtk::UserChangedTheme() {
  if (theme_provider_->UseGtkTheme()) {
    gtk_widget_modify_fg(label_, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_bg(container_.get(), GTK_STATE_NORMAL, NULL);
  } else {
    // TODO(erg): This is the closest to "text that will look good on a
    // toolbar" that I can find. Maybe in later iterations of the theme system,
    // there will be a better color to pick.
    GdkColor bookmark_text =
        theme_provider_->GetGdkColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_widget_modify_fg(label_, GTK_STATE_NORMAL, &bookmark_text);

    GdkColor toolbar_color =
        theme_provider_->GetGdkColor(BrowserThemeProvider::COLOR_TOOLBAR);
    gtk_widget_modify_bg(container_.get(), GTK_STATE_NORMAL, &toolbar_color);
  }

  gtk_util::SetRoundedWindowBorderColor(container_.get(),
                                        theme_provider_->GetBorderColor());
}
