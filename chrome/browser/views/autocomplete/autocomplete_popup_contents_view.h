// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/view.h"

class AutocompleteEditModel;
class AutocompleteEditViewWin;
class AutocompletePopupWin;
class Profile;

// Interface to retrieve the position of the popup.
class AutocompletePopupPositioner {
 public:
  // Returns the bounds at which the popup should be shown, in screen
  // coordinates. The height is ignored, since the popup is sized to its
  // contents automatically.
  virtual gfx::Rect GetPopupBounds() const = 0;
};

// An interface implemented by an object that provides data to populate
// individual result views.
class AutocompleteResultViewModel {
 public:
  // Returns true if the index is selected.
  virtual bool IsSelectedIndex(size_t index) = 0;

  // Returns the type of match that the row corresponds to.
  virtual const AutocompleteMatch& GetMatchAtIndex(size_t index) = 0;

  // Called when the line at the specified index should be opened with the
  // provided disposition.
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition) = 0;

  // Called when the line at the specified index should be shown as hovered.
  virtual void SetHoveredLine(size_t index) = 0;

  // Called when the line at the specified index should be shown as selected.
  virtual void SetSelectedLine(size_t index, bool revert_to_default) = 0;
};

// A view representing the contents of the autocomplete popup.
class AutocompletePopupContentsView : public views::View,
                                      public AutocompleteResultViewModel,
                                      public AutocompletePopupView {
 public:
  AutocompletePopupContentsView(const ChromeFont& font,
                                AutocompleteEditViewWin* edit_view,
                                AutocompleteEditModel* edit_model,
                                Profile* profile,
                                AutocompletePopupPositioner* popup_positioner);
  virtual ~AutocompletePopupContentsView() {}

  // Update the presentation with the latest result.
  void SetAutocompleteResult(const AutocompleteResult& result);

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void OnHoverEnabledOrDisabled(bool disabled);
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

  // Overridden from AutocompleteResultViewModel:
  virtual bool IsSelectedIndex(size_t index);
  virtual const AutocompleteMatch& GetMatchAtIndex(size_t index);
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition);
  virtual void SetHoveredLine(size_t index);
  virtual void SetSelectedLine(size_t index, bool revert_to_default);

  // Overridden from views::View:
  virtual void PaintChildren(ChromeCanvas* canvas);
  virtual void Layout();

 private:
  // Fill a path for the contents' roundrect. |bounding_rect| is the rect that
  // bounds the path.
  void MakeContentsPath(gfx::Path* path, const gfx::Rect& bounding_rect);

  // Updates the window's blur region for the current size.
  void UpdateBlurRegion();

  // Makes the contents of the canvas slightly transparent.
  void MakeCanvasTransparent(ChromeCanvas* canvas);

  // The popup that contains this view.
  AutocompletePopupWin* popup_;

  // The provider of our result set.
  scoped_ptr<AutocompletePopupModel> model_;

  // The edit view that invokes us.
  AutocompleteEditViewWin* edit_view_;

  // An object that tells the popup how to position itself.
  AutocompletePopupPositioner* popup_positioner_;

  // The font used by the edit that created us. This is used by the result
  // views to synthesize a suitable display font.
  ChromeFont edit_font_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupContentsView);
};


#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
