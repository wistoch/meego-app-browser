// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the bookmark bubble, the dialog box
// presented to create or edit a bookmark.  There can only ever be a single
// bubble open, so the class presents only static methods, and handles the
// singleton behavior for you.  It also handles the object and widget
// lifetimes, destroying everything and possibly committing any changes when
// the bubble is closed.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class BookmarkNode;
class Profile;
namespace gfx {
class Rect;
}

class BookmarkBubbleGtk : public InfoBubbleGtkDelegate,
                          public NotificationObserver {
 public:
  // Shows the bookmark bubble, pointing at |rect|.
  static void Show(GtkWindow* toplevel_window,
                   const gfx::Rect& rect,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);

  // Implements the InfoBubbleGtkDelegate.  We are notified when the bubble
  // is about to be closed, so we have a chance to save any state / input in
  // our widgets before they are destroyed.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  BookmarkBubbleGtk(GtkWindow* toplevel_window,
                    const gfx::Rect& rect,
                    Profile* profile,
                    const GURL& url,
                    bool newly_bookmarked);
  ~BookmarkBubbleGtk();

  static void HandleDestroyThunk(GtkWidget* widget,
                                 gpointer userdata) {
    reinterpret_cast<BookmarkBubbleGtk*>(userdata)->
        HandleDestroy();
  }
  // Notified when |content_| is destroyed so we can delete our instance.
  void HandleDestroy();

  static void HandleNameActivateThunk(GtkWidget* widget,
                                      gpointer user_data) {
    reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleNameActivate();
  }
  void HandleNameActivate();

  static void HandleFolderChangedThunk(GtkWidget* widget,
                                       gpointer user_data) {
    reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleFolderChanged();
  }
  void HandleFolderChanged();

  static void HandleFolderPopupShownThunk(GObject* object,
                                          GParamSpec* property,
                                          gpointer user_data) {
    return reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleFolderPopupShown();
  }
  void HandleFolderPopupShown();

  static void HandleEditButtonThunk(GtkWidget* widget,
                                    gpointer user_data) {
    reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleEditButton();
  }
  void HandleEditButton();

  static void HandleCloseButtonThunk(GtkWidget* widget,
                                     gpointer user_data) {
    reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleCloseButton();
  }
  void HandleCloseButton();

  static void HandleRemoveButtonThunk(GtkWidget* widget,
                                      gpointer user_data) {
    reinterpret_cast<BookmarkBubbleGtk*>(user_data)->
        HandleRemoveButton();
  }
  void HandleRemoveButton();

  // Update the bookmark with any edits that have been made.
  void ApplyEdits();

  // Open the bookmark editor for the current url and close the bubble.
  void ShowEditor();

  // Return the UTF8 encoded title for the current |url_|.
  std::string GetTitle();

  // The URL of the bookmark.
  GURL url_;
  // Our current profile (used to access the bookmark system).
  Profile* profile_;

  // Provides colors and stuff.
  GtkThemeProvider* theme_provider_;

  // The toplevel window our dialogs should be transient for.
  GtkWindow* toplevel_window_;

  // We let the InfoBubble own our content, and then we delete ourself
  // when the widget is destroyed (when the InfoBubble is destroyed).
  GtkWidget* content_;

  // The button that removes the bookmark.
  GtkWidget* remove_button_;

  // The various labels in the interface. We keep track of them for theme
  // changes.
  std::vector<GtkWidget*> labels_;

  // The GtkEntry for editing the bookmark name / title.
  GtkWidget* name_entry_;

  // The combo box for selecting the bookmark folder.
  GtkWidget* folder_combo_;

  // The bookmark nodes in |folder_combo_|.
  std::vector<const BookmarkNode*> folder_nodes_;

  InfoBubbleGtk* bubble_;

  // We need to push some things on the back of the message loop, so we have
  // a factory attached to our instance to manage task lifetimes.
  ScopedRunnableMethodFactory<BookmarkBubbleGtk> factory_;

  // Whether the bubble is creating or editing an existing bookmark.
  bool newly_bookmarked_;
  // When closing the window, whether we should update or remove the bookmark.
  bool apply_edits_;
  bool remove_bookmark_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleGtk);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
