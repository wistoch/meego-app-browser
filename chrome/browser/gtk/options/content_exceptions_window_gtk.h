// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/gtk/options/content_exception_editor.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/gtk_tree.h"

class HostContentSettingsMap;

// Dialog that lists each of the exceptions to the current content policy, and
// has options for adding/editing/removing entries. Modal to parrent.
class ContentExceptionsWindowGtk : public gtk_tree::TableAdapter::Delegate,
                                   public ContentExceptionEditor::Delegate {
 public:
  static void ShowExceptionsWindow(GtkWindow* window,
                                   HostContentSettingsMap* map,
                                   ContentSettingsType content_type);

  ~ContentExceptionsWindowGtk();

  // gtk_tree::TableAdapter::Delegate implementation:
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

  // ContentExceptionEditor::Delegate implementation:
  virtual void AcceptExceptionEdit(const std::string& host,
                                   ContentSetting setting,
                                   int index,
                                   bool is_new);

 private:
  // Column ids for |list_store_|.
  enum {
    COL_HOSTNAME,
    COL_ACTION,
    COL_COUNT
  };

  ContentExceptionsWindowGtk(GtkWindow* parent,
                             HostContentSettingsMap* map,
                             ContentSettingsType type);

  // Updates which buttons are enabled.
  void UpdateButtonState();

  // Callbacks for the buttons.
  void Add();
  void Edit();
  void Remove();
  void RemoveAll();

  // Returns the title of the window (changes based on what ContentSettingsType
  // was set to in the constructor).
  std::string GetWindowTitle() const;

  // GTK Callbacks
  static void OnResponse(GtkDialog* dialog,
                         int response_id,
                         ContentExceptionsWindowGtk* window);
  static void OnWindowDestroy(GtkWidget* widget,
                              ContentExceptionsWindowGtk* window);
  static void OnSelectionChanged(GtkTreeSelection* selection,
                                 ContentExceptionsWindowGtk* window);

  // The list presented in |treeview_|; a gobject instead of a C++ object.
  GtkListStore* list_store_;

  // The C++, views-ish, cross-platform model class that actually contains the
  // gold standard data.
  scoped_ptr<ContentExceptionsTableModel> model_;

  // The adapter that ferries data back and forth between |model_| and
  // |list_store_| whenever either of them change.
  scoped_ptr<gtk_tree::TableAdapter> model_adapter_;

  // The exception window.
  GtkWidget* dialog_;

  // The treeview that presents the site/action pairs.
  GtkWidget* treeview_;

  // The current user selection from |treeview_|.
  GtkTreeSelection* treeview_selection_;

  // Buttons.
  GtkWidget* edit_button_;
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_
