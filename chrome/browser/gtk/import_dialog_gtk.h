// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_IMPORT_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_IMPORT_DIALOG_GTK_H_

#include "chrome/browser/importer/importer.h"

#include <gtk/gtk.h>

class Profile;
typedef struct _GtkWindow GtkWindow;

class ImportDialogGtk {
 public:
  // Displays the import box to import data from another browser into |profile|
  static void Show(GtkWindow* parent, Profile* profile);

 private:
  ImportDialogGtk(GtkWindow* parent, Profile* profile);
  ~ImportDialogGtk() { }

  static void HandleOnResponseDialog(GtkWidget* widget,
                                     int response,
                                     gpointer user_data) {
    reinterpret_cast<ImportDialogGtk*>(user_data)->OnDialogResponse(widget,
                                                                    response);
  }
  void OnDialogResponse(GtkWidget* widget, int response);

  // Combo box that displays list of profiles from which we can import.
  GtkWidget* combo_;

  // Bookmarks/Favorites checkbox
  GtkWidget* bookmarks_;

  // Search Engines checkbox
  GtkWidget* search_engines_;

  // Passwords checkbox
  GtkWidget* passwords_;

  // History checkbox
  GtkWidget* history_;

  // Our current profile
  Profile* profile_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(ImportDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_IMPORT_DIALOG_GTK_H_
