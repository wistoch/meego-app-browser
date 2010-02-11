// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/base_tab_strip.h"
#include "chrome/browser/views/tabs/side_tab.h"

class Profile;

class SideTabStripModel {
 public:
  // Returns metadata about the tab at the specified index.
  virtual SkBitmap GetIcon(int index) const = 0;
  virtual string16 GetTitle(int index) const = 0;
  virtual bool IsSelected(int index) const = 0;
};

class SideTabStrip : public BaseTabStrip,
                     public SideTabModel {
 public:
  SideTabStrip();
  virtual ~SideTabStrip();

  // Associate a model with this SideTabStrip. The SideTabStrip owns its model.
  void SetModel(SideTabStripModel* model);

  // Whether or not the browser has been run with the "enable-vertical-tabs"
  // command line flag that allows the SideTabStrip to be optionally shown.
  static bool Available();

  // Whether or not the vertical tabstrip is shown. Only valid if Available()
  // returns true.
  static bool Visible(Profile* profile);

  // Notifies the SideTabStrip that a tab was added in the model at |index|.
  void AddTabAt(int index);

  // Notifies the SideTabStrip that a tab was removed from the model at |index|.
  void RemoveTabAt(int index);

  // Notifies the SideTabStrip that a tab was selected in the model at |index|.
  void SelectTabAt(int index);

  // Notifies the SideTabStrip that the tab at |index| needs to be redisplayed
  // since some of its metadata has changed.
  void UpdateTabAt(int index);

  // SideTabModel implementation:
  virtual string16 GetTitle(SideTab* tab) const;
  virtual SkBitmap GetIcon(SideTab* tab) const;
  virtual bool IsSelected(SideTab* tab) const;

  // BaseTabStrip implementation:
  virtual int GetPreferredHeight();
  virtual void SetBackgroundOffset(const gfx::Point& offset);
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds);
  virtual bool IsDragSessionActive() const;
  virtual void UpdateLoadingAnimations();
  virtual bool IsAnimating() const;
  virtual TabStrip* AsTabStrip();

  // views::View overrides:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();

 private:
  // Returns the model index of the specified |tab|.
  int GetIndexOfSideTab(SideTab* tab) const;

  scoped_ptr<SideTabStripModel> model_;

  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
