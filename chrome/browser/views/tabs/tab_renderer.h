// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__
#define CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__

#include "app/animation.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "gfx/point.h"
#include "views/controls/button/image_button.h"
#include "views/view.h"

class AnimationContainer;
class SlideAnimation;
class TabContents;
class ThrobAnimation;

///////////////////////////////////////////////////////////////////////////////
//
// TabRenderer
//
//  A View that renders a Tab, either in a TabStrip or in a DraggedTabView.
//
///////////////////////////////////////////////////////////////////////////////
class TabRenderer : public views::View,
                    public views::ButtonListener,
                    public AnimationDelegate {
 public:
  // Possible animation states.
  enum AnimationState {
    ANIMATION_NONE,
    ANIMATION_WAITING,
    ANIMATION_LOADING
  };

  TabRenderer();
  virtual ~TabRenderer();

  // Sizes the renderer to the size of the new tab images. This is used when
  // during the new tab animation. See TabStrip's description of AnimationType
  // for details.
  void SizeToNewTabButtonImages();

  // Overridden from views:
  void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  ThemeProvider* GetThemeProvider();

  // Updates the data the Tab uses to render itself from the specified
  // TabContents.
  //
  // See TabStripModel::TabChangedAt documentation for what loading_only means.
  void UpdateData(TabContents* contents, bool phantom, bool loading_only);

  // Sets the blocked state of the tab.
  void SetBlocked(bool blocked);
  bool blocked() const { return data_.blocked; }

  // Sets the mini-state of the tab.
  void set_mini(bool mini) { data_.mini = mini; }
  bool mini() const { return data_.mini; }

  // Sets the phantom state of the tab.
  void set_phantom(bool phantom) { data_.phantom = phantom; }
  bool phantom() const { return data_.phantom; }

  // Used during new tab animation to force the tab to render a new tab like
  // animation.
  void set_render_as_new_tab(bool value) { data_.render_as_new_tab = value; }

  // Sets the alpha value to render the tab at. This is used during the new
  // tab animation.
  void set_alpha(double value) { data_.alpha = value; }

  // Forces the tab to render unselected even though it is selected.
  void set_render_unselected(bool value) { data_.render_unselected = value; }
  bool render_unselected() const { return data_.render_unselected; }

  // Are we in the process of animating a mini tab state change on this tab?
  void set_animating_mini_change(bool value);

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Advance the Loading Animation to the next frame, or hide the animation if
  // the tab isn't loading.
  void ValidateLoadingAnimation(AnimationState animation_state);

  // Starts/Stops a pulse animation.
  void StartPulse();
  void StopPulse();

  // Start/stop the mini-tab title animation.
  void StartMiniTabTitleAnimation();
  void StopMiniTabTitleAnimation();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void SetBackgroundOffset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Set the theme provider - because we get detached, we are frequently
  // outside of a hierarchy with a theme provider at the top. This should be
  // called whenever we're detached or attached to a hierarchy.
  void SetThemeProvider(ThemeProvider* provider) {
    theme_provider_ = provider;
  }

  // Sets the container all animations run from.
  void SetAnimationContainer(AnimationContainer* container);

  // Paints the icon. Most of the time you'll want to invoke Paint directly, but
  // in certain situations this invoked outside of Paint.
  void PaintIcon(gfx::Canvas* canvas);

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for mini-tabs. Mini-tabs always have this width.
  static int GetMiniWidth();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

 protected:
  views::ImageButton* close_button() const { return close_button_; }
  const gfx::Rect& title_bounds() const { return title_bounds_; }

  // Returns the title of the Tab.
  std::wstring GetTitle() const;

  // Overridden from views::View:
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) {}

 private:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ThemeChanged();

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Starts/Stops the crash animation.
  void StartCrashAnimation();
  void StopCrashAnimation();

  // Return true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Set the temporary offset for the favicon. This is used during animation.
  void SetFavIconHidingOffset(int offset);

  void DisplayCrashedFavIcon();
  void ResetCrashedFavIcon();

  // Paint various portions of the Tab
  void PaintTitle(SkColor title_color, gfx::Canvas* canvas);
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas);
  void PaintLoadingAnimation(gfx::Canvas* canvas);
  void PaintAsNewTab(gfx::Canvas* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // Gets the throb value for the tab. When a tab is not selected the
  // active background is drawn at |GetThrobValue()|%. This is used for hover,
  // mini tab title change and pulsing.
  double GetThrobValue();

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect title_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  // Current state of the animation.
  AnimationState animation_state_;

  // The current index into the Animation image strip.
  int animation_frame_;

  // Close Button.
  views::ImageButton* close_button_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  // Pulse animation.
  scoped_ptr<ThrobAnimation> pulse_animation_;

  // Animation used when the title of an inactive mini tab changes.
  scoped_ptr<ThrobAnimation> mini_title_animation_;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    TabData()
        : loading(false),
          crashed(false),
          off_the_record(false),
          show_icon(true),
          mini(false),
          blocked(false),
          animating_mini_change(false),
          phantom(false),
          render_as_new_tab(false),
          render_unselected(false),
          alpha(1) {
    }

    SkBitmap favicon;
    string16 title;
    bool loading;
    bool crashed;
    bool off_the_record;
    bool show_icon;
    bool mini;
    bool blocked;
    bool animating_mini_change;
    bool phantom;
    bool render_as_new_tab;
    bool render_unselected;
    double alpha;
  };
  TabData data_;

  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active;
  static TabImage tab_inactive;
  static TabImage tab_alpha;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

  // The current color of the close button.
  SkColor close_button_color_;

  // The animation object used to swap the favicon with the sad tab icon.
  class FavIconCrashAnimation;
  FavIconCrashAnimation* crash_animation_;

  bool should_display_crashed_favicon_;

  ThemeProvider* theme_provider_;

  scoped_refptr<AnimationContainer> container_;

  static void InitClass();
  static bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(TabRenderer);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__
