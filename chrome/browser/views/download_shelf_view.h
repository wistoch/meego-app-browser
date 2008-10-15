// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H__
#define CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H__

#include "chrome/common/slide_animation.h"
#include "chrome/views/button.h"
#include "chrome/views/link.h"

namespace ChromeViews {
  class ImageView;
}

class DownloadItem;
class TabContents;

class DownloadAnimation;

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// To add a view representing a download to DownloadShelfView, invoke
// AddDownloadView. AddDownloadView takes ownership of the passed in View.
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached. As such,
// the remove method is private.
class DownloadShelfView : public ChromeViews::View,
                          public ChromeViews::BaseButton::ButtonListener,
                          public ChromeViews::LinkController,
                          public AnimationDelegate {
 public:
  DownloadShelfView(TabContents* tab_contents);

  // A new download has started, so add it to our shelf.
  void AddDownload(DownloadItem* download);

  virtual gfx::Size GetPreferredSize();

  virtual void Layout();

  // Invokes the following methods to do painting:
  //   PaintBackground, PaintBorder and PaintSeparators.
  virtual void Paint(ChromeCanvas* canvas);

  void DidChangeBounds(const CRect& previous, const CRect& current);

  // AnimationDelegate implementations
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Invoked when the user clicks the 'show all downloads' link button.
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // Invoked when the user clicks the close button. Asks the browser to
  // hide the download shelf.
  virtual void ButtonPressed(ChromeViews::BaseButton* button);

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(ChromeViews::View* view);

  // Adds a View representing a download to this DownloadShelfView.
  // DownloadShelfView takes ownership of the View, and will delete it as
  // necessary.
  void AddDownloadView(ChromeViews::View* view);

  // Invoked when the download shelf is migrated from one tab contents to a new
  // one.
  void ChangeTabContents(TabContents* old_contents, TabContents* new_contents);

 private:
  void Init();

  // Paints the border.
  void PaintBorder(ChromeCanvas* canvas);

  // Paints the separators. This invokes PaintSeparator to paint a particular
  // separator.
  void PaintSeparators(ChromeCanvas* canvas);

  // Paints the separator between the two views.
  void PaintSeparator(ChromeCanvas* canvas,
                      ChromeViews::View* v1,
                      ChromeViews::View* v2);

  TabContents* tab_contents_;

  // The animation for adding new items to the shelf.
  scoped_ptr<SlideAnimation> new_item_animation_;

  // The show/hide animation for the shelf itself.
  scoped_ptr<SlideAnimation> shelf_animation_;

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  std::vector<View*> download_views_;

  // An image displayed on the right of the "Show all downloads..." link.
  ChromeViews::ImageView* arrow_image_;

  // Link for showing all downloads. This is contained as a child, and deleted
  // by View.
  ChromeViews::Link* show_all_view_;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  ChromeViews::Button* close_button_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadShelfView);
};

#endif // CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H__

