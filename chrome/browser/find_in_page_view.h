// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIND_IN_PAGE_VIEW_H__
#define CHROME_BROWSER_FIND_IN_PAGE_VIEW_H__

#include "base/gfx/size.h"
#include "chrome/views/button.h"
#include "chrome/views/text_field.h"

class FindInPageController;

namespace ChromeViews {
  class Label;
  class MouseEvent;
  class View;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindInPageView is responsible for drawing the UI controls of the
// FindInPage window, the find text box, the 'Find' button and the 'Close'
// button. It communicates the user search words to the FindInPageController.
//
////////////////////////////////////////////////////////////////////////////////
class FindInPageView : public ChromeViews::View,
                       public ChromeViews::BaseButton::ButtonListener,
                       public ChromeViews::TextField::Controller {
 public:
  // A tag denoting which button the user pressed.
  enum ButtonTag {
    FIND_PREVIOUS_TAG = 0,  // The Find Previous button.
    FIND_NEXT_TAG,          // The Find Next button.
    CLOSE_TAG,              // The Close button (the 'X').
  };

  FindInPageView(FindInPageController* controller);
  virtual ~FindInPageView();

  // Updates the UI to show how many matches were found on the page/frames.
  // This function does nothing if |number_of_matches| is below 0, which can
  // happen during a FindNext operation when a scoping effort is already in
  // progress. |final_update| specifies whether this is the last update message
  // this Find operation will produce or if this is just a preliminary report.
  void UpdateMatchCount(int number_of_matches, bool final_update);

  // Notifies the view of the ordinal for the currently active item on the page.
  void UpdateActiveMatchOrdinal(int ordinal);

  // Updates the label inside the Find text box that shows the ordinal of the
  // active item and how many matches were found.
  void UpdateResultLabel();

  // Resets the UI element that shows how many matches were found.
  void ResetMatchCount();

  // Resets the background for the match count label.
  void ResetMatchCountBackground();

  // View needs to respond to Show to set focus to the find text box
  void OnShow();

  // Updates the view to let it know where the controller is clipping the
  // Find window (while animating the opening or closing of the window).
  void animation_offset(int offset) { animation_offset_ = offset; }

  // Overridden from ChromeViews::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void DidChangeBounds(const CRect& old_bounds,
                               const CRect& new_bounds);
  virtual void GetPreferredSize(CSize* out);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Overridden from ChromeViews::ButtonListener::ButtonPressed:
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

  // Overridden from ChromeViews::TextField::Controller:
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender, UINT message,
                               TCHAR key, UINT repeat_count, UINT flags);

  // Set whether or not we're attempting to blend with the toolbar.
  void SetToolbarBlend(bool toolbar_blend) {toolbar_blend_ = toolbar_blend;}

 private:
  // We use a hidden view to grab mouse clicks and bring focus to the find
  // text box. This is because although the find text box may look like it
  // extends all the way to the find button, it only goes as far as to the
  // match_count label. The user, however, expects being able to click anywhere
  // inside what looks like the find text box (including on or around the
  // match_count label) and have focus brought to the find box.
  class FocusForwarderView : public ChromeViews::View {
   public:
    explicit FocusForwarderView(
        ChromeViews::TextField* view_to_focus_on_mousedown)
      : view_to_focus_on_mousedown_(view_to_focus_on_mousedown) {}

  private:
    virtual bool OnMousePressed(const ChromeViews::MouseEvent& event);

    ChromeViews::TextField* view_to_focus_on_mousedown_;

    DISALLOW_EVIL_CONSTRUCTORS(FocusForwarderView);
  };

  // The controller that maintains the selected model, and performs actions
  // such as handling selected items.
  FindInPageController* controller_;

  // The controls in the window.
  ChromeViews::TextField* find_text_;
  ChromeViews::Label* match_count_text_;
  FocusForwarderView* focus_forwarder_view_;
  ChromeViews::Button* find_previous_button_;
  ChromeViews::Button* find_next_button_;
  ChromeViews::Button* close_button_;

  // Whether or not we're attempting to blend with the toolbar (this is
  // false if the bookmarks bar is visible).
  bool toolbar_blend_;

  // While animating, the controller clips the window and draws only the bottom
  // part of it. The view needs to know the pixel offset at which we are drawing
  // the window so that we can draw the curved edges that attach to the toolbar
  // in the right location.
  int animation_offset_;

  // How many matches were found on the page.
  int match_count_;

  // The ordinal of the currently active match.
  int active_match_ordinal_;

  DISALLOW_EVIL_CONSTRUCTORS(FindInPageView);
};

#endif  // CHROME_BROWSER_FIND_IN_PAGE_VIEW_H__

