// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_

#include <gtk/gtk.h>

#include "app/animation.h"
#include "app/gfx/chrome_canvas.h"
#include "app/gfx/chrome_font.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/common/owned_widget_gtk.h"
#include "skia/include/SkBitmap.h"

namespace gfx {
class Size;
}  // namespace gfx

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

  class LoadingAnimation {
   public:
    struct Data {
      SkBitmap* waiting_animation_frames;
      SkBitmap* loading_animation_frames;
      int loading_animation_frame_count;
      int waiting_animation_frame_count;
      int waiting_to_loading_frame_count_ratio;
    };

    explicit LoadingAnimation(const Data* data);

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

   private:
    const Data* const data_;

    // Current state of the animation.
    AnimationState animation_state_;

    // The current index into the Animation image strip.
    int animation_frame_;

    DISALLOW_COPY_AND_ASSIGN(LoadingAnimation);
  };

  TabRendererGtk();
  virtual ~TabRendererGtk();

  // TabContents. If only the loading state was updated, the loading_only flag
  // should be specified. If other things change, set this flag to false to
  // update everything.
  void UpdateData(TabContents* contents, bool loading_only);

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Notifies subclasses that the close button has been resized to |bounds|.
  virtual void CloseButtonResized(const gfx::Rect& bounds);

  // Paints the tab into |canvas|.
  virtual void Paint(GdkEventExpose* event);

  // Advance the loading animation to the next frame, or hide the animation if
  // the tab isn't loading.
  void ValidateLoadingAnimation(AnimationState animation_state);

  bool IsVisible();

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

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
  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    SkBitmap favicon;
    std::wstring title;
    bool loading;
    bool crashed;
    bool off_the_record;
    bool show_icon;
    bool show_download_icon;
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

  // Generates the bounds for the interior items of the tab.
  void Layout();

  // Returns the largest of the favicon, title text, and the close button.
  static int GetContentHeight();

  // Paint various portions of the Tab
  void PaintTabBackground(ChromeCanvasPaint* canvas);
  void PaintInactiveTabBackground(ChromeCanvasPaint* canvas);
  void PaintActiveTabBackground(ChromeCanvasPaint* canvas);
  void PaintLoadingAnimation(ChromeCanvasPaint* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // expose-event handler that redraws the tab.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabRendererGtk* tab);

  // TODO(jhawkins): Move to TabResources.
  static void InitResources();
  static bool initialized_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect download_icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect close_button_bounds_;

  TabData data_;

  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha;
  static TabImage tab_hover_;

  static ChromeFont* title_font_;
  static int title_font_height_;

  static SkBitmap* download_icon_;
  static int download_icon_width_;
  static int download_icon_height_;

  static int close_button_width_;
  static int close_button_height_;

  // The GtkDrawingArea we draw the tab on.
  OwnedWidgetGtk tab_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the download icon. Comes from the model.
  bool showing_download_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

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

  DISALLOW_COPY_AND_ASSIGN(TabRendererGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_RENDERER_GTK_H_
