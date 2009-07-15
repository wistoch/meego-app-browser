// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_

#include <gtk/gtk.h>

#include "app/animation.h"
#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/string16.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}  // namespace gfx

class CustomDrawButton;
class TabContents;
class ThemeProvider;

class TabRendererGtk : public AnimationDelegate {
 public:
  // Possible animation states.
  enum AnimationState {
    ANIMATION_NONE,
    ANIMATION_WAITING,
    ANIMATION_LOADING
  };

  class LoadingAnimation : public NotificationObserver {
   public:
    struct Data {
      explicit Data(ThemeProvider* theme_provider);

      SkBitmap* waiting_animation_frames;
      SkBitmap* loading_animation_frames;
      int loading_animation_frame_count;
      int waiting_animation_frame_count;
      int waiting_to_loading_frame_count_ratio;
    };

    explicit LoadingAnimation(ThemeProvider* theme_provider);

    // Advance the loading animation to the next frame, or hide the animation if
    // the tab isn't loading.
    void ValidateLoadingAnimation(AnimationState animation_state);

    AnimationState animation_state() const { return animation_state_; }
    int animation_frame() const { return animation_frame_; }

    const SkBitmap* waiting_animation_frames() const {
      return data_->waiting_animation_frames;
    }
    const SkBitmap* loading_animation_frames() const {
      return data_->loading_animation_frames;
    }

    // Provide NotificationObserver implementation.
    virtual void Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details);

   private:
    scoped_ptr<Data> data_;

    // Used to listen for theme change notifications.
    NotificationRegistrar registrar_;

    // Gives us our throbber images.
    ThemeProvider* theme_provider_;

    // Current state of the animation.
    AnimationState animation_state_;

    // The current index into the Animation image strip.
    int animation_frame_;

    DISALLOW_COPY_AND_ASSIGN(LoadingAnimation);
  };

  explicit TabRendererGtk(ThemeProvider* theme_provider);
  virtual ~TabRendererGtk();

  // TabContents. If only the loading state was updated, the loading_only flag
  // should be specified. If other things change, set this flag to false to
  // update everything.
  virtual void UpdateData(TabContents* contents, bool loading_only);

  // Sets the pinned state of the tab.
  void set_pinned(bool pinned);
  bool is_pinned() const;

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Returns true if the Tab is visible, false otherwise.
  virtual bool IsVisible() const;

  // Sets the visibility of the Tab.
  virtual void SetVisible(bool visible) const;

  // Paints the tab into |canvas|.
  virtual void Paint(gfx::Canvas* canvas);

  // Paints the tab into a SkBitmap.
  virtual SkBitmap PaintBitmap();

  // There is no PaintNow available, so the fastest we can do is schedule a
  // paint with the windowing system.
  virtual void SchedulePaint();

  // Notifies the Tab that the close button has been clicked.
  virtual void CloseButtonClicked();

  // Advance the loading animation to the next frame, or hide the animation if
  // the tab isn't loading.
  void ValidateLoadingAnimation(AnimationState animation_state);

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for pinned tabs. Pinned tabs always have this width.
  static int GetPinnedWidth();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // Sets the colors used for painting text on the tabs.
  static void SetSelectedTitleColor(SkColor color);
  static void SetUnselectedTitleColor(SkColor color);

  // Returns the bounds of the Tab.
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }

  gfx::Rect bounds() const { return bounds_; }

  // Sets the bounds of the tab.
  void SetBounds(const gfx::Rect& bounds);

  GtkWidget* widget() const { return tab_.get(); }

 protected:
  const gfx::Rect& title_bounds() const { return title_bounds_; }
  const gfx::Rect& close_button_bounds() const { return close_button_bounds_; }

  // Returns the title of the Tab.
  std::wstring GetTitle() const;

  // Called by TabGtk to notify the renderer that the tab is being hovered.
  void OnMouseEntered();

  // Called by TabGtk to notify the renderer that the tab is no longer being
  // hovered.
  void OnMouseExited();

 private:
  class FavIconCrashAnimation;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    SkBitmap favicon;
    string16 title;
    bool loading;
    bool crashed;
    bool off_the_record;
    bool show_icon;
    bool pinned;
  };

  // TODO(jhawkins): Move into TabResources class.
  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
  };

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

  // Generates the bounds for the interior items of the tab.
  void Layout();

  // Returns the local bounds of the tab.  This returns the rect
  // {0, 0, width(), height()} for now, as we don't yet support borders.
  gfx::Rect GetLocalBounds();

  // Moves the close button widget within the GtkFixed container.
  void MoveCloseButtonWidget();

  // Returns the largest of the favicon, title text, and the close button.
  static int GetContentHeight();

  // Paints the tab, minus the close button.
  void PaintTab(GdkEventExpose* event);

  // Paint various portions of the Tab
  void PaintTitle(gfx::Canvas* canvas);
  void PaintIcon(gfx::Canvas* canvas);
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas);
  void PaintPinnedTabBackground(gfx::Canvas* canvas);
  void PaintLoadingAnimation(gfx::Canvas* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  CustomDrawButton* MakeCloseButton();

  // Handles the clicked signal for the close button.
  static void OnCloseButtonClicked(GtkWidget* widget, TabRendererGtk* tab);

  // Handles middle clicking the close button.
  static gboolean OnCloseButtonMouseRelease(GtkWidget* widget,
                                            GdkEventButton* event,
                                            TabRendererGtk* tab);

  // expose-event handler that redraws the tab.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabRendererGtk* tab);

  // TODO(jhawkins): Move to TabResources.
  static void InitResources();
  static bool initialized_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect close_button_bounds_;

  TabData data_;

  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha;

  static gfx::Font* title_font_;
  static int title_font_height_;

  static int close_button_width_;
  static int close_button_height_;

  static SkColor selected_title_color_;
  static SkColor unselected_title_color_;

  // Preferred width of pinned tabs.
  static int pinned_tab_pref_width_;

  // When a non-pinned tab is pinned the width of the tab animates. If the
  // width of a pinned tab is >= pinned_tab_renderer_as_tab_width then the
  // tab is rendered as a normal tab. This is done to avoid having the title
  // immediately disappear when transitioning a tab from normal to pinned.
  static int pinned_tab_renderer_as_tab_width_;

  // The GtkDrawingArea we draw the tab on.
  OwnedWidgetGtk tab_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

  // The animation object used to swap the favicon with the sad tab icon.
  scoped_ptr<FavIconCrashAnimation> crash_animation_;

  // Set when the crashed favicon should be displayed.
  bool should_display_crashed_favicon_;

  // The bounds of this Tab.
  gfx::Rect bounds_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  // Contains the loading animation state.
  LoadingAnimation loading_animation_;

  // TODO(jhawkins): If the theme is changed after the tab is created, we'll
  // still render the old theme for this tab.
  ThemeProvider* theme_provider_;

  // The close button.
  scoped_ptr<CustomDrawButton> close_button_;

  DISALLOW_COPY_AND_ASSIGN(TabRendererGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_
