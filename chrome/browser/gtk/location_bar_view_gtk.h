// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditViewGtk;
class AutocompletePopupPositioner;
class CommandUpdater;
class Profile;
class SkBitmap;
class TabContents;
class ToolbarModel;

class LocationBarViewGtk : public AutocompleteEditController,
                           public LocationBar {
 public:
  LocationBarViewGtk(CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     AutocompletePopupPositioner* popup_positioner);
  ~LocationBarViewGtk();

  void Init();

  void SetProfile(Profile* profile);

  // Returns the widget the caller should host.  You must call Init() first.
  GtkWidget* widget() { return alignment_.get(); }

  // Updates the location bar.  We also reset the bar's permanent text and
  // security style, and, if |tab_for_state_restoring| is non-NULL, also
  // restore saved state that the tab holds.
  void Update(const TabContents* tab_for_state_restoring);

  // Implement the AutocompleteEditController interface.
  virtual void OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // Implement the LocationBar interface.
  virtual void ShowFirstRunBubble(bool use_OEM_bubble);
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual AutocompleteEditView* location_entry() {
    return location_entry_.get();
  }

  // Translation between a security level and the background color.  Both the
  // location bar and edit have to manage and match the background color.
  static const GdkColor kBackgroundColorByLevel[3];

 private:
  static gboolean HandleExposeThunk(GtkWidget* widget, GdkEventExpose* event,
                                    gpointer userdata) {
    return reinterpret_cast<LocationBarViewGtk*>(userdata)->
        HandleExpose(widget, event);
  }
  gboolean HandleExpose(GtkWidget* widget, GdkEventExpose* event);

  // Calculate and set what the padding should be around the location entry.
  // For example, we will increase the right padding to make room for an icon.
  void UpdateAlignmentPadding();

  // Set the SSL icon we should be showing.
  void SetSecurityIcon(ToolbarModel::Icon icon);

  // The outermost widget we want to be hosted.
  OwnedWidgetGtk alignment_;

  // The current SSL icon we are showing, or NULL.
  GdkPixbuf* security_icon_;

  scoped_ptr<AutocompleteEditViewGtk> location_entry_;

  Profile* profile_;
  CommandUpdater* command_updater_;
  ToolbarModel* toolbar_model_;

  // We need to hold on to this just to it pass to the edit.
  AutocompletePopupPositioner* popup_positioner_;

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation
  PageTransition::Type transition_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewGtk);
};

#endif  // CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_
