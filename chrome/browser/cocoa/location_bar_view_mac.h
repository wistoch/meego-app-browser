// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/location_bar.h"

class AutocompleteEditViewMac;
class CommandUpdater;
class ToolbarModel;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an AutocompleteEditViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public AutocompleteEditController,
                           public LocationBar {
 public:
  LocationBarViewMac(CommandUpdater* command_updater,
		     ToolbarModel* toolbar_model);
  virtual ~LocationBarViewMac();

  void Init();

  void SetField(NSTextField* field);

  virtual void ShowFirstRunBubble() { NOTIMPLEMENTED(); }
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput() { NOTIMPLEMENTED(); }
  virtual void AcceptInputWithDisposition(WindowOpenDisposition disposition)
      { NOTIMPLEMENTED(); }
  virtual void FocusLocation();
  virtual void FocusSearch() { NOTIMPLEMENTED(); }
  virtual void UpdateFeedIcon() { /* http://crbug.com/8832 */ }
  virtual void SaveStateToContents(TabContents* contents);

  virtual void OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

 private:
  scoped_ptr<AutocompleteEditViewMac> edit_view_;

  // TODO(shess): Determine ownership of these.  We definitely
  // shouldn't.
  CommandUpdater* command_updater_;  // weak
  ToolbarModel* toolbar_model_;  // weak

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation
  PageTransition::Type transition_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};
