// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_H_

#include <gtk/gtk.h>
#include <vector>
#include <string>

class BookmarkModel;
class BookmarkNode;
class GtkThemeProvider;
class Profile;

namespace bookmark_utils {

extern const char kBookmarkNode[];

// Padding between the image and the label of a bookmark bar button.
extern const int kBarButtonPadding;

// Get the image that is used to represent the node. This function adds a ref
// to the returned pixbuf, so it requires a matching call to g_object_unref().
GdkPixbuf* GetPixbufForNode(const BookmarkNode* node, BookmarkModel* model,
                            bool native);

// Returns a GtkWindow with a visual hierarchy for passing to
// gtk_drag_set_icon_widget().
GtkWidget* GetDragRepresentation(const BookmarkNode* node,
                                 BookmarkModel* model,
                                 GtkThemeProvider* provider);

// Helper function that sets visual properties of GtkButton |button| to the
// contents of |node|.
void ConfigureButtonForNode(const BookmarkNode* node, BookmarkModel* model,
                            GtkWidget* button, GtkThemeProvider* provider);

// Returns the tooltip.
std::string BuildTooltipFor(const BookmarkNode* node);

// Returns the "bookmark-node" property of |widget| casted to the correct type.
const BookmarkNode* BookmarkNodeForWidget(GtkWidget* widget);

// Set the colors on |label| as per the theme.
void SetButtonTextColors(GtkWidget* label, GtkThemeProvider* provider);

// Drag and drop. --------------------------------------------------------------

// Pickle a node into a GtkSelection.
void WriteBookmarkToSelection(const BookmarkNode* node,
                              GtkSelectionData* selection_data,
                              guint target_type,
                              Profile* profile);

// Pickle a vector of nodes into a GtkSelection.
void WriteBookmarksToSelection(const std::vector<const BookmarkNode*>& nodes,
                               GtkSelectionData* selection_data,
                               guint target_type,
                               Profile* profile);

// Un-pickle node(s) from a GtkSelection.
// The last two arguments are out parameters.
std::vector<const BookmarkNode*> GetNodesFromSelection(
    GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type,
    Profile* profile,
    gboolean* delete_selection_data,
    gboolean* dnd_success);

// Unpickle a new bookmark of the CHROME_NAMED_URL drag type, and put it in
// the appropriate location in the model.
bool CreateNewBookmarkFromNamedUrl(
    GtkSelectionData* selection_data,
    BookmarkModel* model,
    const BookmarkNode* parent,
    int idx);

// Add the URIs in |selection_data| into the model at the given position. They
// will be added whether or not the URL is valid.
bool CreateNewBookmarksFromURIList(
    GtkSelectionData* selection_data,
    BookmarkModel* model,
    const BookmarkNode* parent,
    int idx);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_H_
