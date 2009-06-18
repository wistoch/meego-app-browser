// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/common/owned_widget_gtk.h"

class GURL;

// GTK implementation of StatusBubble. Unlike Windows, our status bubble
// doesn't have the nice leave-the-window effect since we can't rely on the
// window manager to not try to be "helpful" and center our popups, etc.
// We therefore position it absolutely in a GtkFixed, that we don't own.
class StatusBubbleGtk : public StatusBubble {
 public:
  StatusBubbleGtk();
  virtual ~StatusBubbleGtk();

  // StatusBubble implementation.
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();

  // Called when the download shelf becomes visible or invisible.
  // This is used by to ensure that the status bubble does not obscure
  // the download shelf, when it is visible.
  virtual void UpdateDownloadShelfVisibility(bool visible) { }

  // Top of the widget hierarchy for a StatusBubble. This top level widget is
  // guarenteed to have its gtk_widget_name set to "status-bubble" for
  // identification.
  GtkWidget* widget() { return container_.get(); }

 private:
  // The same as the public SetStatus method, but using utf8 instead.
  void SetStatus(const std::string& status_utf8);

  // Set the text in the gtk widget and handle when to show/hide the bubble.
  void UpdateWidgetText(const std::string& text);

  // Sets the status bubble's location in the parent GtkFixed, shows the widget
  // and makes sure that the status bubble has the highest z-order.
  void Show();

  // Sets an internal timer to hide the status bubble after a delay.
  void HideInASecond();

  // Builds the widgets, containers, etc.
  void InitWidgets();

  // A GtkAlignment that is the child of |slide_widget_|.
  OwnedWidgetGtk container_;

  // The GtkLabel holding the text.
  GtkWidget* label_;

  // We keep both the status and url that we want to show and try to pick the
  // right one to display to the user.
  std::string status_text_;
  std::string url_text_;

  // A timer that hides our window after a delay.
  ScopedRunnableMethodFactory<StatusBubbleGtk> timer_factory_;
};

#endif  // CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
