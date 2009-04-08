// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "chrome/common/owned_widget_gtk.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

class Browser;
class CustomContainerButton;
class NineBox;
class PageNavigator;
class Profile;

class BookmarkBarGtk : public BookmarkModelObserver {
 public:
  explicit BookmarkBarGtk(Profile* proifle, Browser* browser);
  virtual ~BookmarkBarGtk();

  // Resets the profile. This removes any buttons for the current profile and
  // recreates the models.
  void SetProfile(Profile* profile);

  // Returns the current profile.
  Profile* GetProfile() { return profile_; }

  // Returns the current browser.
  Browser* browser() const { return browser_; }

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(PageNavigator* navigator);

  // Create the contents of the bookmark bar.
  void Init(Profile* profile);

  // Adds this GTK toolbar into a sizing box.
  void AddBookmarkbarToBox(GtkWidget* box);

  // Whether the current page is the New Tag Page (which requires different
  // rendering).
  bool OnNewTabPage();

  // Change the visibility of the bookmarks bar. (Starts out hidden, per GTK's
  // default behaviour).
  void Show();
  void Hide();

  // Returns true if the bookmarks bar preference is set to 'always show'.
  bool IsAlwaysShown();

 private:
  // Helper function which destroys all the bookmark buttons in
  // |current_bookmark_buttons_|.
  void RemoveAllBookmarkButtons();

  // Overridden from BookmarkModelObserver:

  // Invoked when the bookmark bar model has finished loading. Creates a button
  // for each of the children of the root node from the model.
  virtual void Loaded(BookmarkModel* model);

  // Invoked when the model is being deleted.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {
    NOTIMPLEMENTED();
  }

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {
    NOTIMPLEMENTED();
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    NOTIMPLEMENTED();
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index) {
    NOTIMPLEMENTED();
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {
    NOTIMPLEMENTED();
  }

  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {
    NOTIMPLEMENTED();
  }

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node) {
    NOTIMPLEMENTED();
  }

 private:
  GtkWidget* CreateBookmarkButton(BookmarkNode* node);
  GtkToolItem* CreateBookmarkToolItem(BookmarkNode* node);

  std::string BuildTooltip(BookmarkNode* node);

  void LoadNineboxImages();

  // GtkButton callbacks
  static gboolean OnButtonPressed(GtkWidget* sender,
                                  GdkEventButton* event,
                                  BookmarkBarGtk* bar);
  static gboolean OnButtonReleased(GtkWidget* sender, GdkEventButton* event,
                                   BookmarkBarGtk* bar);
  static gboolean OnButtonExpose(GtkWidget* widget, GdkEventExpose* e,
                                 BookmarkBarGtk* button);
  static void OnButtonDragBegin(GtkWidget* widget,
                                GdkDragContext* drag_context,
                                BookmarkBarGtk* bar);
  static void OnButtonDragEnd(GtkWidget* button,
                              GdkDragContext* drag_context,
                              BookmarkBarGtk* bar);

  // GtkToolbar callbacks
  static gboolean OnToolbarExpose(GtkWidget* widget, GdkEventExpose* event,
                                  BookmarkBarGtk* window);
  static gboolean OnToolbarDragMotion(GtkToolbar* toolbar,
                                      GdkDragContext* context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      BookmarkBarGtk* bar);
  static gboolean OnToolbarDragLeave(GtkToolbar* toolbar,
                                     GdkDragContext* context,
                                     guint time,
                                     BookmarkBarGtk* bar);

  Profile* profile_;

  // Used for opening urls.
  PageNavigator* page_navigator_;

  Browser* browser_;

  // Model providing details as to the starred entries/groups that should be
  // shown. This is owned by the Profile.
  BookmarkModel* model_;

  // Top level container that contains |bookmark_hbox_| and spacers.
  OwnedWidgetGtk container_;

  // Container that has all the individual members of
  // |current_bookmark_buttons_| as children.
  GtkWidget* bookmark_hbox_;

  // A GtkLabel to display when there are no bookmark buttons to display.
  GtkWidget* instructions_;

  // GtkToolbar which contains all the bookmark buttons.
  OwnedWidgetGtk bookmark_toolbar_;

  // The other bookmarks button.
  CustomContainerButton* other_bookmarks_button_;

  // Whether we should ignore the next button release event (because we were
  // dragging).
  bool ignore_button_release_;

  // The BookmarkNode from the model being dragged. NULL when we aren't
  // dragging.
  BookmarkNode* dragged_node_;

  // We create a GtkToolbarItem from |dragged_node_| for display.
  GtkToolItem* toolbar_drop_item_;

  // Whether we should show the instructional text in the bookmark bar.
  bool show_instructions_;

  // The theme graphics for when the mouse is over the button.
  scoped_ptr<NineBox> nine_box_prelight_;
  // The theme graphics for when the button is clicked.
  scoped_ptr<NineBox> nine_box_active_;
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_
