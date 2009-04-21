// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/common/owned_widget_gtk.h"

class BaseDownloadItemModel;
class CustomDrawButton;
class DownloadItemGtk;
class LinkButtonGtk;
class SlideAnimatorGtk;

class DownloadShelfGtk : public DownloadShelf {
 public:
  explicit DownloadShelfGtk(TabContents* tab_contents);

  ~DownloadShelfGtk();

  // DownloadShelf implementation.
  virtual void AddDownload(BaseDownloadItemModel* download_model);
  virtual bool IsShowing() const;

 private:
  static void OnButtonClick(GtkWidget* button, DownloadShelfGtk* toolbar);

  // The top level widget of the shelf.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // |hbox_| holds the download items and buttons of the shelf.
  GtkWidget* hbox_;

  // |shelf_| is the second highest level widget. See the constructor
  // for an explanation of the widget layout.
  OwnedWidgetGtk shelf_;

  // This hbox holds the link text and download icon. It also holds the
  // distinction of being the leftmost non-download item widget on the shelf.
  GtkWidget* link_hbox_;

  // The clickable "Show all downloads..." link text.
  scoped_ptr<LinkButtonGtk> link_button_;

  // The 'x' that the user can press to hide the download shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  // Keeps track of our current hide/show state.
  bool is_showing_;

  // The download items we have added to our shelf.
  std::vector<DownloadItemGtk*> download_items_;
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_
