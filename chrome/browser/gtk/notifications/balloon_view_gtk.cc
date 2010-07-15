// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/notifications/balloon_view_gtk.h"

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/notifications/balloon_view_host_gtk.h"
#include "chrome/browser/gtk/notifications/notification_options_menu_model.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "gfx/canvas.h"
#include "gfx/gtk_util.h"
#include "gfx/insets.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Margin, in pixels, between the notification frame and the contents
// of the notification.
const int kTopMargin = 0;
const int kBottomMargin = 1;
const int kLeftMargin = 1;
const int kRightMargin = 1;

// How many pixels of overlap there is between the shelf top and the
// balloon bottom.
const int kShelfBorderTopOverlap = 0;

// Properties of the dismiss button.
const int kDismissButtonWidth = 60;
const int kDismissButtonHeight = 20;

// Properties of the options menu.
const int kOptionsMenuWidth = 60;
const int kOptionsMenuHeight = 20;

// Properties of the origin label.
const int kLeftLabelMargin = 8;

// TODO(johnnyg): Add a shadow for the frame.
const int kLeftShadowWidth = 0;
const int kRightShadowWidth = 0;
const int kTopShadowWidth = 0;
const int kBottomShadowWidth = 0;

// Space in pixels between text and icon on the buttons.
const int kButtonIconSpacing = 10;

// Number of characters to show in the origin label before ellipsis.
const int kOriginLabelCharacters = 18;

// The shelf height for the system default font size.  It is scaled
// with changes in the default font size.
const int kDefaultShelfHeight = 21;
const int kShelfVerticalMargin = 3;

// The amount that the bubble collections class offsets from the side of the
// screen.
const int kScreenBorder = 5;

// Colors specified in various ways for different parts of the UI.
// These match the windows colors in balloon_view.cc
const char* kLabelColor = "#7D7D7D";
const double kShelfBackgroundColorR = 245.0 / 255.0;
const double kShelfBackgroundColorG = 245.0 / 255.0;
const double kShelfBackgroundColorB = 245.0 / 255.0;
const double kDividerLineColorR = 180.0 / 255.0;
const double kDividerLineColorG = 180.0 / 255.0;
const double kDividerLineColorB = 180.0 / 255.0;

// Makes the website label relatively smaller to the base text size.
const char* kLabelMarkup = "<span size=\"small\" color=\"%s\">%s</span>";

}  // namespace

BalloonViewImpl::BalloonViewImpl(BalloonCollection* collection)
    : balloon_(NULL),
      frame_container_(NULL),
      html_container_(NULL),
      method_factory_(this),
      close_button_(NULL),
      animation_(NULL) {
}

BalloonViewImpl::~BalloonViewImpl() {
}

void BalloonViewImpl::Close(bool by_user) {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BalloonViewImpl::DelayedClose, by_user));
}

gfx::Size BalloonViewImpl::GetSize() const {
  // BalloonView has no size if it hasn't been shown yet (which is when
  // balloon_ is set).
  if (!balloon_)
    return gfx::Size();

  // Although this may not be the instantaneous size of the balloon if
  // called in the middle of an animation, it is the effective size that
  // will result from the animation.
  return gfx::Size(GetDesiredTotalWidth(), GetDesiredTotalHeight());
}

void BalloonViewImpl::DelayedClose(bool by_user) {
  html_contents_->Shutdown();
  if (frame_container_) {
    // It's possible that |frame_container_| was destroyed before the
    // BalloonViewImpl if our related browser window was closed first.
    gtk_widget_hide(frame_container_);
  }
  balloon_->OnClose(by_user);
}

void BalloonViewImpl::RepositionToBalloon() {
  if (!frame_container_) {
    // No need to create a slide animation when this balloon is fading out.
    return;
  }

  DCHECK(balloon_);

  // Create an amination from the current position to the desired one.
  int start_x;
  int start_y;
  int start_w;
  int start_h;
  gtk_window_get_position(GTK_WINDOW(frame_container_), &start_x, &start_y);
  gtk_window_get_size(GTK_WINDOW(frame_container_), &start_w, &start_h);

  int end_x = balloon_->GetPosition().x();
  int end_y = balloon_->GetPosition().y();
  int end_w = GetDesiredTotalWidth();
  int end_h = GetDesiredTotalHeight();

  anim_frame_start_ = gfx::Rect(start_x, start_y, start_w, start_h);
  anim_frame_end_ = gfx::Rect(end_x, end_y, end_w, end_h);
  animation_.reset(new SlideAnimation(this));
  animation_->Show();
}

void BalloonViewImpl::AnimationProgressed(const Animation* animation) {
  DCHECK_EQ(animation, animation_.get());

  // Linear interpolation from start to end position.
  double end = animation->GetCurrentValue();
  double start = 1.0 - end;

  gfx::Rect frame_position(
      static_cast<int>(start * anim_frame_start_.x() +
                       end * anim_frame_end_.x()),
      static_cast<int>(start * anim_frame_start_.y() +
                       end * anim_frame_end_.y()),
      static_cast<int>(start * anim_frame_start_.width() +
                       end * anim_frame_end_.width()),
      static_cast<int>(start * anim_frame_start_.height() +
                       end * anim_frame_end_.height()));
  gtk_window_resize(GTK_WINDOW(frame_container_),
                    frame_position.width(), frame_position.height());
  gtk_window_move(GTK_WINDOW(frame_container_),
                  frame_position.x(), frame_position.y());

  gfx::Rect contents_rect = GetContentsRectangle();
  html_contents_->UpdateActualSize(contents_rect.size());
}

void BalloonViewImpl::Show(Balloon* balloon) {
  theme_provider_ = GtkThemeProvider::GetFrom(balloon->profile());

  const std::string source_label_text = l10n_util::GetStringFUTF8(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      WideToUTF16(balloon->notification().display_source()));
  const std::string options_text =
      l10n_util::GetStringUTF8(IDS_NOTIFICATION_OPTIONS_MENU_LABEL);
  const std::string dismiss_text =
      l10n_util::GetStringUTF8(IDS_NOTIFICATION_BALLOON_DISMISS_LABEL);

  balloon_ = balloon;
  frame_container_ = gtk_window_new(GTK_WINDOW_POPUP);

  // Construct the options menu.
  options_menu_model_.reset(new NotificationOptionsMenuModel(balloon_));
  options_menu_.reset(new MenuGtk(this, options_menu_model_.get()));

  // Create a BalloonViewHost to host the HTML contents of this balloon.
  html_contents_.reset(new BalloonViewHost(balloon));
  html_contents_->Init();
  gfx::NativeView contents = html_contents_->native_view();

  // Divide the frame vertically into the shelf and the content area.
  GtkWidget* vbox = gtk_vbox_new(0, 0);
  gtk_container_add(GTK_CONTAINER(frame_container_), vbox);

  shelf_ = gtk_hbox_new(0, 0);
  gtk_container_add(GTK_CONTAINER(vbox), shelf_);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(
      GTK_ALIGNMENT(alignment),
      kTopMargin, kBottomMargin, kLeftMargin, kRightMargin);
  gtk_widget_show_all(alignment);
  gtk_container_add(GTK_CONTAINER(alignment), contents);
  gtk_container_add(GTK_CONTAINER(vbox), alignment);

  // Create a toolbar and add it to the shelf.
  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_size_request(GTK_WIDGET(hbox_), -1, GetShelfHeight());
  gtk_container_add(GTK_CONTAINER(shelf_), hbox_);
  gtk_widget_show_all(vbox);

  g_signal_connect(frame_container_, "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(frame_container_, "destroy",
                   G_CALLBACK(OnDestroyThunk), this);

  // Create a label for the source of the notification and add it to the
  // toolbar.
  GtkWidget* source_label_ = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kLabelMarkup,
                                         kLabelColor,
                                         source_label_text.c_str());
  gtk_label_set_markup(GTK_LABEL(source_label_), markup);
  g_free(markup);
  gtk_label_set_max_width_chars(GTK_LABEL(source_label_),
                                kOriginLabelCharacters);
  gtk_label_set_ellipsize(GTK_LABEL(source_label_), PANGO_ELLIPSIZE_END);
  GtkWidget* label_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(label_alignment),
                            kShelfVerticalMargin, kShelfVerticalMargin,
                            kLeftLabelMargin, 0);
  gtk_container_add(GTK_CONTAINER(label_alignment), source_label_);
  gtk_box_pack_start(GTK_BOX(hbox_), label_alignment, FALSE, FALSE, 0);

  // Create a button to dismiss the balloon and add it to the toolbar.
  close_button_.reset(new CustomDrawButton(IDR_BALLOON_CLOSE,
                                           IDR_BALLOON_CLOSE_HOVER,
                                           IDR_BALLOON_CLOSE_HOVER,
                                           IDR_BALLOON_CLOSE_HOVER));
  gtk_widget_set_tooltip_text(close_button_->widget(), dismiss_text.c_str());
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonThunk), this);
  GTK_WIDGET_UNSET_FLAGS(close_button_->widget(), GTK_CAN_FOCUS);
  GtkWidget* close_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(close_alignment),
                            kShelfVerticalMargin, kShelfVerticalMargin,
                            0, kButtonIconSpacing);
  gtk_container_add(GTK_CONTAINER(close_alignment), close_button_->widget());
  gtk_box_pack_end(GTK_BOX(hbox_), close_alignment, FALSE, FALSE, 0);

  // Create a button for showing the options menu, and add it to the toolbar.
  options_menu_button_.reset(new CustomDrawButton(IDR_BALLOON_WRENCH,
                                                  IDR_BALLOON_WRENCH_HOVER,
                                                  IDR_BALLOON_WRENCH_HOVER,
                                                  IDR_BALLOON_WRENCH_HOVER));
  gtk_widget_set_tooltip_text(options_menu_button_->widget(),
                              options_text.c_str());
  g_signal_connect(options_menu_button_->widget(), "clicked",
                   G_CALLBACK(OnOptionsMenuButtonThunk), this);
  GTK_WIDGET_UNSET_FLAGS(options_menu_button_->widget(), GTK_CAN_FOCUS);
  GtkWidget* options_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(options_alignment),
                            kShelfVerticalMargin, kShelfVerticalMargin,
                            0, kButtonIconSpacing);
  gtk_container_add(GTK_CONTAINER(options_alignment),
                    options_menu_button_->widget());
  gtk_box_pack_end(GTK_BOX(hbox_), options_alignment, FALSE, FALSE, 0);

  notification_registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                              NotificationService::AllSources());

  // We don't do InitThemesFor() because it just forces a redraw.
  gtk_util::ActAsRoundedWindow(frame_container_, gfx::kGdkBlack, 3,
                               gtk_util::ROUNDED_ALL,
                               gtk_util::BORDER_ALL);

  // Realize the frame container so we can do size calculations.
  gtk_widget_realize(frame_container_);

  // Update to make sure we have everything sized properly and then move our
  // window offscreen for its initial animation.
  html_contents_->UpdateActualSize(balloon_->content_size());
  int window_width;
  gtk_window_get_size(GTK_WINDOW(frame_container_), &window_width, NULL);

  int pos_x = gdk_screen_width() - window_width - kScreenBorder;
  int pos_y = gdk_screen_height();
  gtk_window_move(GTK_WINDOW(frame_container_), pos_x, pos_y);
  balloon_->SetPosition(gfx::Point(pos_x, pos_y), false);
  gtk_widget_show_all(frame_container_);

  notification_registrar_.Add(this,
      NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon));
}

void BalloonViewImpl::Update() {
  DCHECK(html_contents_.get()) << "BalloonView::Update called before Show";
  if (html_contents_->render_view_host())
    html_contents_->render_view_host()->NavigateToURL(
        balloon_->notification().content_url());
}

gfx::Point BalloonViewImpl::GetContentsOffset() const {
  return gfx::Point(kLeftShadowWidth + kLeftMargin,
                    GetShelfHeight() + kTopShadowWidth + kTopMargin);
}

int BalloonViewImpl::GetShelfHeight() const {
  // TODO(johnnyg): add scaling here.
  return kDefaultShelfHeight;
}

int BalloonViewImpl::GetDesiredTotalWidth() const {
  return balloon_->content_size().width() +
      kLeftMargin + kRightMargin + kLeftShadowWidth + kRightShadowWidth;
}

int BalloonViewImpl::GetDesiredTotalHeight() const {
  return balloon_->content_size().height() +
      kTopMargin + kBottomMargin + kTopShadowWidth + kBottomShadowWidth +
      GetShelfHeight();
}

gfx::Rect BalloonViewImpl::GetContentsRectangle() const {
  if (!frame_container_)
    return gfx::Rect();

  gfx::Size content_size = balloon_->content_size();
  gfx::Point offset = GetContentsOffset();
  int x = 0, y = 0;
  gtk_window_get_position(GTK_WINDOW(frame_container_), &x, &y);
  return gfx::Rect(x + offset.x(), y + offset.y(),
                   content_size.width(), content_size.height());
}

void BalloonViewImpl::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::NOTIFY_BALLOON_DISCONNECTED) {
    // If the renderer process attached to this balloon is disconnected
    // (e.g., because of a crash), we want to close the balloon.
    notification_registrar_.Remove(this,
        NotificationType::NOTIFY_BALLOON_DISCONNECTED,
        Source<Balloon>(balloon_));
    Close(false);
  } else if (type == NotificationType::BROWSER_THEME_CHANGED) {
    // Since all the buttons change their own properties, and our expose does
    // all the real differences, we'll need a redraw.
    gtk_widget_queue_draw(frame_container_);
  } else {
    NOTREACHED();
  }
}

gboolean BalloonViewImpl::OnExpose(GtkWidget* sender, GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(sender->window));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  gfx::Size content_size = balloon_->content_size();
  gfx::Point offset = GetContentsOffset();

  // Draw a background color behind the shelf.
  cairo_set_source_rgb(cr, kShelfBackgroundColorR,
                       kShelfBackgroundColorG, kShelfBackgroundColorB);
  cairo_rectangle(cr, kLeftMargin, kTopMargin + 0.5,
                  content_size.width() - 0.5, GetShelfHeight());
  cairo_fill(cr);

  // Now draw a one pixel line between content and shelf.
  cairo_move_to(cr, offset.x(), offset.y() - 1);
  cairo_line_to(cr, offset.x() + content_size.width(), offset.y() - 1);
  cairo_set_line_width(cr, 0.5);
  cairo_set_source_rgb(cr, kDividerLineColorR,
                       kDividerLineColorG, kDividerLineColorB);
  cairo_stroke(cr);

  cairo_destroy(cr);

  return FALSE;
}

void BalloonViewImpl::OnOptionsMenuButton(GtkWidget* widget) {
  options_menu_->PopupAsContext(gtk_get_current_event_time());
}

gboolean BalloonViewImpl::OnDestroy(GtkWidget* widget) {
  frame_container_ = NULL;
  Close(false);
  return FALSE;  // Propagate.
}
