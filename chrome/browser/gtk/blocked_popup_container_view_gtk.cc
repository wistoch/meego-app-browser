// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

// Color of the gradient in the background.
const double kBackgroundColorTop[] = { 246.0 / 255, 250.0 / 255, 1.0 };
const double kBackgroundColorBottom[] = { 219.0 / 255, 235.0 / 255, 1.0 };

// Rounded corner radius (in pixels).
const int kCornerSize = 4;

}  // namespace

// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewGtk(container);
}

BlockedPopupContainerViewGtk::~BlockedPopupContainerViewGtk() {
  container_.Destroy();
}

TabContentsViewGtk* BlockedPopupContainerViewGtk::ContainingView() {
  return static_cast<TabContentsViewGtk*>(
      model_->GetConstrainingContents(NULL)->view());
}

void BlockedPopupContainerViewGtk::GetURLAndTitleForPopup(
    size_t index, string16* url, string16* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = model_->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToUTF16(tab_contents_url.possibly_invalid_spec());
  *title = tab_contents->GetTitle();
}

// Overridden from BlockedPopupContainerView:
void BlockedPopupContainerViewGtk::SetPosition() {
  // No-op. Not required with the GTK version.
}

void BlockedPopupContainerViewGtk::ShowView() {
  // TODO(erg): Animate in.
  gtk_widget_show_all(container_.get());
}

void BlockedPopupContainerViewGtk::UpdateLabel() {
  size_t blocked_popups = model_->GetBlockedPopupCount();

  GtkWidget* label = gtk_bin_get_child(GTK_BIN(menu_button_));
  if (!label) {
    label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(menu_button_), label);
  }

  gtk_label_set_text(
      GTK_LABEL(label),
      (blocked_popups > 0) ?
      l10n_util::GetStringFUTF8(IDS_POPUPS_BLOCKED_COUNT,
                                UintToString16(blocked_popups)).c_str() :
      l10n_util::GetStringUTF8(IDS_POPUPS_UNBLOCKED).c_str());
}

void BlockedPopupContainerViewGtk::HideView() {
  // TODO(erg): Animate out.
  gtk_widget_hide(container_.get());
}

void BlockedPopupContainerViewGtk::Destroy() {
  ContainingView()->RemoveBlockedPopupView(this);
  delete this;
}

void BlockedPopupContainerViewGtk::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);

  // Make sure the label exists (so we can change its colors).
  UpdateLabel();

  // Update the label's colors.
  GtkWidget* label = gtk_bin_get_child(GTK_BIN(menu_button_));
  if (theme_provider_->UseGtkTheme()) {
    gtk_util::SetLabelColor(label, NULL);
  } else {
    GdkColor color = theme_provider_->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_util::SetLabelColor(label, &color);
  }

  GdkColor color = theme_provider_->GetBorderColor();
  gtk_util::SetRoundedWindowBorderColor(container_.get(), color);
}

bool BlockedPopupContainerViewGtk::IsCommandEnabled(int command_id) const {
  return true;
}

bool BlockedPopupContainerViewGtk::IsItemChecked(int id) const {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    return model_->IsHostWhitelisted(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  }

  return false;
}

void BlockedPopupContainerViewGtk::ExecuteCommand(int id) {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    model_->ToggleWhitelistingForHost(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  } else {
    model_->LaunchPopupAtIndex(id_size_t - 1);
  }
}

BlockedPopupContainerViewGtk::BlockedPopupContainerViewGtk(
    BlockedPopupContainer* container)
    : model_(container),
      theme_provider_(GtkThemeProvider::GetFrom(container->profile())),
      close_button_(CustomDrawButton::CloseButton(theme_provider_)) {
  Init();

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
}

void BlockedPopupContainerViewGtk::Init() {
  menu_button_ = theme_provider_->BuildChromeButton();
  UpdateLabel();
  g_signal_connect(menu_button_, "clicked",
                   G_CALLBACK(OnMenuButtonClicked), this);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), menu_button_, FALSE, FALSE, kSmallPadding);
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClicked), this);

  container_.Own(gtk_util::CreateGtkBorderBin(hbox, NULL,
      kSmallPadding, kSmallPadding, kSmallPadding, kSmallPadding));
  // Connect an expose signal that draws the background. Most connect before
  // the ActAsRoundedWindow one.
  g_signal_connect(container_.get(), "expose-event",
                   G_CALLBACK(OnRoundedExposeCallback), this);
  gtk_util::ActAsRoundedWindow(
      container_.get(), gfx::kGdkBlack, kCornerSize,
      gtk_util::ROUNDED_TOP_LEFT | gtk_util::ROUNDED_TOP_RIGHT,
      gtk_util::BORDER_LEFT | gtk_util::BORDER_TOP | gtk_util::BORDER_RIGHT);

  ContainingView()->AttachBlockedPopupView(this);
}

void BlockedPopupContainerViewGtk::OnMenuButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->launch_menu_.reset(new MenuGtk(container, false));

  // Set items 1 .. popup_count as individual popups.
  size_t popup_count = container->model_->GetBlockedPopupCount();
  for (size_t i = 0; i < popup_count; ++i) {
    string16 url, title;
    container->GetURLAndTitleForPopup(i, &url, &title);
    // We can't just use the index into container_ here because Menu reserves
    // the value 0 as the nop command.
    container->launch_menu_->AppendMenuItemWithLabel(i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_TITLE_FORMAT, url, title));
  }

  // Set items (kImpossibleNumberOfPopups + 1) ..
  // (kImpossibleNumberOfPopups + 1 + hosts.size()) as hosts.
  std::vector<std::string> hosts(container->model_->GetHosts());
  if (!hosts.empty() && (popup_count > 0))
    container->launch_menu_->AppendSeparator();
  for (size_t i = 0; i < hosts.size(); ++i) {
    container->launch_menu_->AppendCheckMenuItemWithLabel(
        BlockedPopupContainer::kImpossibleNumberOfPopups + i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_HOST_FORMAT,
                                  UTF8ToUTF16(hosts[i])));
  }

  container->launch_menu_->PopupAsContext(gtk_get_current_event_time());
}

void BlockedPopupContainerViewGtk::OnCloseButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->model_->set_dismissed();
  container->model_->CloseAll();
}

gboolean BlockedPopupContainerViewGtk::OnRoundedExposeCallback(
    GtkWidget* widget, GdkEventExpose* event,
    BlockedPopupContainerViewGtk* container) {
  if (!container->theme_provider_->UseGtkTheme()) {
    int width = widget->allocation.width;
    int height = widget->allocation.height;

    // Clip to our damage rect.
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(event->window));
    cairo_rectangle(cr, event->area.x, event->area.y,
                    event->area.width, event->area.height);
    cairo_clip(cr);

    // TODO(erg): We draw the gradient background only when GTK themes are
    // off. This isn't a perfect solution as this isn't themed! The views
    // version doesn't appear to be themed either, so at least for now,
    // constants are OK.
    int half_width = width / 2;
    cairo_pattern_t* pattern = cairo_pattern_create_linear(
        half_width, 0,  half_width, height);
    cairo_pattern_add_color_stop_rgb(
        pattern, 0.0,
        kBackgroundColorTop[0], kBackgroundColorTop[1], kBackgroundColorTop[2]);
    cairo_pattern_add_color_stop_rgb(
        pattern, 1.0,
        kBackgroundColorBottom[0], kBackgroundColorBottom[1],
        kBackgroundColorBottom[2]);
    cairo_set_source(cr, pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(pattern);

    cairo_destroy(cr);
  }

  return FALSE;
}
