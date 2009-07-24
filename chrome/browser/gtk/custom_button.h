// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
#define CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_

#include <gtk/gtk.h>

#include <string>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GtkThemeProvider;

// These classes implement two kinds of custom-drawn buttons.  They're
// used on the toolbar and the bookmarks bar.

// CustomDrawButtonBase provides the base for building a custom drawn button.
// It handles managing the pixbufs containing all the static images used to draw
// the button.  It also manages painting these pixbufs.
class CustomDrawButtonBase : public NotificationObserver {
 public:
  // If the images come from ResourceBundle rather than the theme provider,
  // pass in NULL for |theme_provider|.
  CustomDrawButtonBase(GtkThemeProvider* theme_provider,
                       int normal_id,
                       int active_id,
                       int highlight_id,
                       int depressed_id);

  ~CustomDrawButtonBase();

  GdkPixbuf* pixbufs(int i) const { return pixbufs_[i]; }

  gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e);

  void set_paint_override(int state) { paint_override_ = state; }
  int paint_override() const { return paint_override_; }

  // Set the background details.
  void SetBackground(SkColor color, SkBitmap* image, SkBitmap* mask);

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];

  // The background image.
  GdkPixbuf* background_image_;

  // If non-negative, the state to paint the button.
  int paint_override_;

  // We need to remember the image ids that the user passes in and the theme
  // provider so we can reload images if the user changes theme.
  int normal_id_;
  int active_id_;
  int highlight_id_;
  int depressed_id_;
  GtkThemeProvider* theme_provider_;

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButtonBase);
};

// CustomDrawButton is a plain button where all its various states are drawn
// with static images. In GTK rendering mode, it will show the standard button
// with GTK |stock_id|.
class CustomDrawButton : public NotificationObserver {
 public:
  // The constructor takes 4 resource ids.  If a resource doesn't exist for a
  // button, pass in 0.
  CustomDrawButton(int normal_id,
                   int active_id,
                   int highlight_id,
                   int depressed_id);

  // Same as above, but uses themed (and possibly tinted) images.
  CustomDrawButton(GtkThemeProvider* theme_provider,
                   int normal_id,
                   int active_id,
                   int highlight_id,
                   int depressed_id,
                   const char* stock_id,
                   GtkIconSize stock_size);

  ~CustomDrawButton();

  void Init();

  GtkWidget* widget() const { return widget_.get(); }

  gfx::Rect bounds() const {
      return gfx::Rect(widget_->allocation.x,
                       widget_->allocation.y,
                       widget_->allocation.width,
                       widget_->allocation.height);
  }

  int width() const { return widget_->allocation.width; }
  int height() const { return widget_->allocation.height; }

  // Set the state to draw. We will paint the widget as if it were in this
  // state.
  void SetPaintOverride(GtkStateType state);

  // Resume normal drawing of the widget's state.
  void UnsetPaintOverride();

  // Set the background details.
  void SetBackground(SkColor color, SkBitmap* image, SkBitmap* mask);

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns a standard close button. Pass a |theme_provider| to use Gtk icons
  // in Gtk rendering mode.
  static CustomDrawButton* CloseButton(GtkThemeProvider* theme_provider);

 private:
  // Sets the button to themed or not.
  void SetBrowserTheme();

  // Callback for custom button expose, used to draw the custom graphics.
  static gboolean OnCustomExpose(GtkWidget* widget,
                                 GdkEventExpose* e,
                                 CustomDrawButton* obj);

  // The actual button widget.
  OwnedWidgetGtk widget_;

  CustomDrawButtonBase button_base_;

  // Our theme provider.
  GtkThemeProvider* theme_provider_;

  // The stock icon name and size.
  const char* gtk_stock_name_;
  GtkIconSize icon_size_;

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButton);
};

#endif  // CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
