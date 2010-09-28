// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Boilerplate code was generated by http://burtonini.com/cgi/gobject.py

#ifndef CHROME_BROWSER_AUTOCOMPLETE_UNDO_VIEW_H_
#define CHROME_BROWSER_AUTOCOMPLETE_UNDO_VIEW_H_

#include <gtk/gtk.h>
#include "undo_manager.h"

G_BEGIN_DECLS

#define GTK_TYPE_UNDO_VIEW gtk_undo_view_get_type()

#define GTK_UNDO_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_UNDO_VIEW, GtkUndoView))

#define GTK_UNDO_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_UNDO_VIEW, GtkUndoViewClass))

#define GTK_IS_UNDO_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_UNDO_VIEW))

#define GTK_IS_UNDO_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_UNDO_VIEW))

#define GTK_UNDO_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_UNDO_VIEW, GtkUndoViewClass))

typedef struct {
  GtkTextView parent;
  GtkSourceUndoManager *undo_manager_;
} GtkUndoView;

typedef struct {
  GtkTextViewClass parent_class;

  void (*undo)(GtkUndoView *);
  void (*redo)(GtkUndoView *);
} GtkUndoViewClass;

GType gtk_undo_view_get_type(void);

GtkWidget* gtk_undo_view_new(GtkTextBuffer *buffer);

G_END_DECLS

#endif  // CHROME_BROWSER_AUTOCOMPLETE_UNDO_VIEW_H_

