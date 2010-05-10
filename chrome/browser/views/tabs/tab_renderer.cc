// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_renderer.h"

#include <limits>

#include "app/animation_container.h"
#include "app/l10n_util.h"
#include "app/multi_animation.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "app/throb_animation.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "gfx/canvas.h"
#include "gfx/favicon_size.h"
#include "gfx/font.h"
#include "gfx/skbitmap_operations.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

#ifdef WIN32
#include "app/win_util.h"
#endif

static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;
static const int kDropShadowHeight = 2;
static const int kToolbarOverlap = 1;
static const int kFavIconTitleSpacing = 4;
static const int kTitleCloseButtonSpacing = 5;
static const int kStandardTitleWidth = 175;
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;
static const int kSelectedTitleColor = SK_ColorBLACK;

// Vertical adjustment to the favicon when the tab has a large icon.
static const int kAppTapFaviconVerticalAdjustment = 2;

// When a non-mini-tab becomes a mini-tab the width of the tab animates. If
// the width of a mini-tab is >= kMiniTabRendererAsNormalTabWidth then the tab
// is rendered as a normal tab. This is done to avoid having the title
// immediately disappear when transitioning a tab from normal to mini-tab.
static const int kMiniTabRendererAsNormalTabWidth =
    browser_defaults::kMiniTabWidth + 30;

// How long the hover state takes.
static const int kHoverDurationMs = 90;

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;

// TODO(beng): (Cleanup) This stuff should move onto the class.
static gfx::Font* title_font = NULL;
static int title_font_height = 0;
static SkBitmap* close_button_n = NULL;
static SkBitmap* close_button_m = NULL;
static SkBitmap* close_button_h = NULL;
static SkBitmap* close_button_p = NULL;
static int close_button_height = 0;
static int close_button_width = 0;

static SkBitmap* waiting_animation_frames = NULL;
static SkBitmap* loading_animation_frames = NULL;
static SkBitmap* crashed_fav_icon = NULL;
static int loading_animation_frame_count = 0;
static int waiting_animation_frame_count = 0;
static int waiting_to_loading_frame_count_ratio = 0;

// Used when |render_as_new_tab| is true.
static SkBitmap* new_tab_mask = NULL;
static SkBitmap* new_tab_shadow = NULL;

TabRenderer::TabImage TabRenderer::tab_alpha = {0};
TabRenderer::TabImage TabRenderer::tab_active = {0};
TabRenderer::TabImage TabRenderer::tab_active_nano = {0};
TabRenderer::TabImage TabRenderer::tab_inactive = {0};
TabRenderer::TabImage TabRenderer::tab_inactive_nano = {0};
TabRenderer::TabImage TabRenderer::tab_alpha_nano = {0};

// Durations for the various parts of the mini tab title animation.
static const int kMiniTitleChangeAnimationDuration1MS = 1000;
static const int kMiniTitleChangeAnimationDuration2MS = 500;
static const int kMiniTitleChangeAnimationDuration3MS = 800;

// Offset from the right edge for the start of the mini title change animation.
static const int kMiniTitleChangeInitialXOffset = 6;

// Radius of the radial gradient used for mini title change animation.
static const int kMiniTitleChangeGradientRadius = 20;

// Colors of the gradient used during the mini title change animation.
static const SkColor kMiniTitleChangeGradientColor1 = SK_ColorWHITE;
static const SkColor kMiniTitleChangeGradientColor2 =
    SkColorSetARGB(0, 255, 255, 255);

namespace {

void InitResources() {
  static bool initialized = false;
  if (!initialized) {
    // TODO(glen): Allow theming of these.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    title_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
    title_font_height = title_font->height();

    close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    close_button_m = rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK);
    close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
    close_button_width = close_button_n->width();
    close_button_height = close_button_n->height();

    TabRenderer::LoadTabImages();

    // The loading animation image is a strip of states. Each state must be
    // square, so the height must divide the width evenly.
    loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
    DCHECK(loading_animation_frames);
    DCHECK(loading_animation_frames->width() %
           loading_animation_frames->height() == 0);
    loading_animation_frame_count =
        loading_animation_frames->width() / loading_animation_frames->height();

    // We get a DIV0 further down when the throbber is replaced by an image
    // which is taller than wide. In this case we cannot deduce an animation
    // sequence from it since we assume that each animation frame has the width
    // of the image's height.
    if (loading_animation_frame_count == 0) {
#ifdef WIN32
      // TODO(idanan): Remove this when we have a way to handle theme errors.
      // See: http://code.google.com/p/chromium/issues/detail?id=12531
      // For now, this is Windows-specific because some users have downloaded
      // a DLL from outside of Google to override the theme.
      std::wstring text = l10n_util::GetString(IDS_RESOURCE_ERROR);
      std::wstring caption = l10n_util::GetString(IDS_RESOURCE_ERROR_CAPTION);
      UINT flags = MB_OK | MB_ICONWARNING | MB_TOPMOST;
      win_util::MessageBox(NULL, text, caption, flags);
#endif
      CHECK(loading_animation_frame_count) << "Invalid throbber size. Width = "
          << loading_animation_frames->width() << ", height = "
          << loading_animation_frames->height();
    }

    waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
    DCHECK(waiting_animation_frames);
    DCHECK(waiting_animation_frames->width() %
           waiting_animation_frames->height() == 0);
    waiting_animation_frame_count =
        waiting_animation_frames->width() / waiting_animation_frames->height();

    waiting_to_loading_frame_count_ratio =
        waiting_animation_frame_count / loading_animation_frame_count;
    // TODO(beng): eventually remove this when we have a proper themeing system.
    //             themes not supporting IDR_THROBBER_WAITING are causing this
    //             value to be 0 which causes DIV0 crashes. The value of 5
    //             matches the current bitmaps in our source.
    if (waiting_to_loading_frame_count_ratio == 0)
      waiting_to_loading_frame_count_ratio = 5;

    crashed_fav_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

    initialized = true;
  }
}

int GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, title_font_height);
  return std::max(content_height, close_button_height);
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseButton
//
//  This is a Button subclass that causes middle clicks to be forwarded to the
//  parent View by explicitly not handling them in OnMousePressed.
class TabCloseButton : public views::ImageButton {
 public:
  explicit TabCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
  }
  virtual ~TabCloseButton() {}

  virtual bool OnMousePressed(const views::MouseEvent& event) {
    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return event.IsOnlyMiddleMouseButton() ? false : handled;
  }

  // We need to let the parent know about mouse state so that it
  // can highlight itself appropriately. Note that Exit events
  // fire before Enter events, so this works.
  virtual void OnMouseEntered(const views::MouseEvent& event) {
    CustomButton::OnMouseEntered(event);
    GetParent()->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) {
    CustomButton::OnMouseExited(event);
    GetParent()->OnMouseExited(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class TabRenderer::FavIconCrashAnimation : public LinearAnimation,
                                           public AnimationDelegate {
 public:
  explicit FavIconCrashAnimation(TabRenderer* target)
      : ALLOW_THIS_IN_INITIALIZER_LIST(LinearAnimation(1000, 25, this)),
        target_(target) {
  }
  virtual ~FavIconCrashAnimation() {}

  // Animation overrides:
  virtual void AnimateToState(double state) {
    const double kHidingOffset = 27;

    if (state < .5) {
      target_->SetFavIconHidingOffset(
          static_cast<int>(floor(kHidingOffset * 2.0 * state)));
    } else {
      target_->DisplayCrashedFavIcon();
      target_->SetFavIconHidingOffset(
          static_cast<int>(
              floor(kHidingOffset - ((state - .5) * 2.0 * kHidingOffset))));
    }
  }

  // AnimationDelegate overrides:
  virtual void AnimationCanceled(const Animation* animation) {
    target_->SetFavIconHidingOffset(0);
  }

 private:
  TabRenderer* target_;

  DISALLOW_COPY_AND_ASSIGN(FavIconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, public:

TabRenderer::TabRenderer()
    : animation_state_(ANIMATION_NONE),
      animation_frame_(0),
      throbber_disabled_(false),
      showing_icon_(false),
      showing_close_button_(false),
      fav_icon_hiding_offset_(0),
      close_button_color_(NULL),
      crash_animation_(NULL),
      should_display_crashed_favicon_(false),
      theme_provider_(NULL) {
  InitResources();

  // Add the Close Button.
  close_button_ = new TabCloseButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  AddChildView(close_button_);

  hover_animation_.reset(new SlideAnimation(this));
  hover_animation_->SetSlideDuration(kHoverDurationMs);

  pulse_animation_.reset(new ThrobAnimation(this));
  pulse_animation_->SetSlideDuration(kPulseDurationMs);
}

TabRenderer::~TabRenderer() {
  delete crash_animation_;
}

void TabRenderer::SizeToNewTabButtonImages() {
  SetBounds(x(), y(), new_tab_shadow->width(), new_tab_shadow->height());
}

void TabRenderer::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (parent->GetThemeProvider())
    SetThemeProvider(parent->GetThemeProvider());
}

ThemeProvider* TabRenderer::GetThemeProvider() {
  ThemeProvider* tp = View::GetThemeProvider();
  if (tp)
    return tp;

  if (theme_provider_)
    return theme_provider_;

  return NULL;
}

void TabRenderer::UpdateData(TabContents* contents,
                             bool phantom,
                             bool loading_only) {
  DCHECK(contents);
  if (data_.phantom != phantom || !loading_only) {
    data_.title = contents->GetTitle();
    data_.off_the_record = contents->profile()->IsOffTheRecord();
    data_.crashed = contents->is_crashed();
    data_.app = contents->is_app();
    SkBitmap* app_icon = contents->GetExtensionAppIcon();
    if (app_icon && data_.app)
      data_.favicon = *app_icon;
    else
      data_.favicon = contents->GetFavIcon();
    data_.phantom = phantom;
    if (phantom) {
      data_.crashed = false;  // Phantom tabs can never crash.
      StopMiniTabTitleAnimation();
    }

    // Sets the accessible name for the tab.
    SetAccessibleName(UTF16ToWide(data_.title));
  }

  // If this is an extension app and a command line flag is set,
  // then disable the throbber.
  throbber_disabled_ = data_.app &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableThrobberForExtensionApps);

  // TODO(glen): Temporary hax.
  theme_provider_ = contents->profile()->GetThemeProvider();

  // Loading state also involves whether we show the favicon, since that's where
  // we display the throbber.
  data_.loading = contents->is_loading();
  data_.show_icon = contents->ShouldDisplayFavIcon();
}

void TabRenderer::UpdateFromModel() {
  // Force a layout, since the tab may have grown a favicon.
  Layout();
  SchedulePaint();

  if (data_.crashed) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation())
      StartCrashAnimation();
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavIcon();
  }
}

void TabRenderer::set_animating_mini_change(bool value) {
  data_.animating_mini_change = value;
}

bool TabRenderer::IsSelected() const {
  return true;
}

void TabRenderer::ValidateLoadingAnimation(AnimationState animation_state) {
  if (throbber_disabled_)
    return;

  if (animation_state_ != animation_state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (animation_state_ == ANIMATION_WAITING &&
        animation_state == ANIMATION_LOADING) {
      animation_frame_ = loading_animation_frame_count -
          (animation_frame_ / waiting_to_loading_frame_count_ratio);
    }
    animation_state_ = animation_state;
  }

  if (animation_state_ != ANIMATION_NONE) {
    animation_frame_ = ++animation_frame_ %
                       ((animation_state_ == ANIMATION_WAITING) ?
                         waiting_animation_frame_count :
                         loading_animation_frame_count);
  } else {
    animation_frame_ = 0;
  }

  SchedulePaint();
}

void TabRenderer::StartPulse() {
  pulse_animation_->Reset();
  pulse_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void TabRenderer::StopPulse() {
  pulse_animation_->Stop();
}

void TabRenderer::StartMiniTabTitleAnimation() {
  if (!mini_title_animation_.get()) {
    MultiAnimation::Parts parts;
    parts.push_back(MultiAnimation::Part(kMiniTitleChangeAnimationDuration1MS,
                                         Tween::EASE_OUT));
    parts.push_back(MultiAnimation::Part(kMiniTitleChangeAnimationDuration2MS,
                                         Tween::ZERO));
    parts.push_back(MultiAnimation::Part(kMiniTitleChangeAnimationDuration3MS,
                                         Tween::EASE_IN));
    mini_title_animation_.reset(new MultiAnimation(parts));
    mini_title_animation_->SetContainer(container_.get());
    mini_title_animation_->set_delegate(this);
  }
  mini_title_animation_->Start();
}

void TabRenderer::StopMiniTabTitleAnimation() {
  if (mini_title_animation_.get())
    mini_title_animation_->Stop();
}

void TabRenderer::SetAnimationContainer(AnimationContainer* container) {
  container_ = container;
  pulse_animation_->SetContainer(container);
}

void TabRenderer::PaintIcon(gfx::Canvas* canvas) {
  if (animation_state_ != ANIMATION_NONE) {
    PaintLoadingAnimation(canvas);
  } else {
    canvas->save();
    canvas->ClipRectInt(0, 0, width(), height());
    if (should_display_crashed_favicon_) {
      canvas->DrawBitmapInt(*crashed_fav_icon, 0, 0,
                            crashed_fav_icon->width(),
                            crashed_fav_icon->height(),
                            favicon_bounds_.x(),
                            favicon_bounds_.y() + fav_icon_hiding_offset_,
                            kFavIconSize, kFavIconSize,
                            true);
    } else {
      if (!data_.favicon.isNull()) {
        // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
        // to using that class to render the favicon).
        int x = favicon_bounds_.x();
        int y = favicon_bounds_.y() + fav_icon_hiding_offset_;
        int size = data_.favicon.width();
        canvas->DrawBitmapInt(data_.favicon, 0, 0,
                              data_.favicon.width(),
                              data_.favicon.height(),
                              x, y, size, size,
                              true);
      }
    }
    canvas->restore();
  }
}

// static
gfx::Size TabRenderer::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active.image_l->height());
  return minimum_size;
}

// static
gfx::Size TabRenderer::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabRenderer::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFavIconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

// static
int TabRenderer::GetMiniWidth() {
  return browser_defaults::kMiniTabWidth;
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, protected:

std::wstring TabRenderer::GetTitle() const {
  return UTF16ToWideHack(data_.title);
}

void TabRenderer::OnMouseEntered(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(Tween::EASE_OUT);
  hover_animation_->Show();
}

void TabRenderer::OnMouseExited(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(Tween::EASE_IN);
  hover_animation_->Hide();
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, views::View overrides:

void TabRenderer::Paint(gfx::Canvas* canvas) {
  if (data_.render_as_new_tab) {
    if (UILayoutIsRightToLeft()) {
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
    }
    PaintAsNewTab(canvas);
    return;
  }

  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !mini())
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon() && !phantom();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ ||
      show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          BrowserThemeProvider::COLOR_TAB_TEXT :
          BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);

  if (!mini() || width() > kMiniTabRendererAsNormalTabWidth)
    PaintTitle(title_color, canvas);

  if (show_icon)
    PaintIcon(canvas);

  // If the close button color has changed, generate a new one.
  if (!close_button_color_ || title_color != close_button_color_) {
    close_button_color_ = title_color;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    close_button_->SetBackground(close_button_color_,
        rb.GetBitmapNamed(IDR_TAB_CLOSE),
        rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK));
  }
}

void TabRenderer::Layout() {
  gfx::Rect lb = GetLocalBounds(false);
  if (lb.IsEmpty())
    return;
  lb.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // First of all, figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    // Use the size of the favicon as apps use a bigger favicon size.
    int favicon_size =
        !data_.favicon.empty() ? data_.favicon.width() : kFavIconSize;
    int favicon_top = kTopPadding + content_height / 2 - favicon_size / 2;
    int favicon_left = lb.x();
    if (favicon_size != kFavIconSize) {
      favicon_left -= (favicon_size - kFavIconSize) / 2;
      favicon_top -= kAppTapFaviconVerticalAdjustment;
    }
    favicon_bounds_.SetRect(favicon_left, favicon_top,
                            favicon_size, favicon_size);
    if ((mini() || data_.animating_mini_change) &&
        width() < kMiniTabRendererAsNormalTabWidth) {
      // Adjust the location of the favicon when transitioning from a normal
      // tab to a mini-tab.
      int mini_delta = kMiniTabRendererAsNormalTabWidth - GetMiniWidth();
      int ideal_delta = width() - GetMiniWidth();
      if (ideal_delta < mini_delta) {
        int ideal_x = (GetMiniWidth() - favicon_size) / 2;
        int x = favicon_bounds_.x() + static_cast<int>(
            (1 - static_cast<float>(ideal_delta) /
             static_cast<float>(mini_delta)) *
            (ideal_x - favicon_bounds_.x()));
        favicon_bounds_.set_x(x);
      }
    }
  } else {
    favicon_bounds_.SetRect(lb.x(), lb.y(), 0, 0);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_height) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    close_button_->SetBounds(lb.width() + kCloseButtonHorzFuzz,
                             close_button_top, close_button_width,
                             close_button_height);
    close_button_->SetVisible(true);
  } else {
    close_button_->SetBounds(0, 0, 0, 0);
    close_button_->SetVisible(false);
  }

  int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - title_font_height) / 2;
  // Size the Title text to fill the remaining space.
  if (!mini() || width() >= kMiniTabRendererAsNormalTabWidth) {
    // If the user has big fonts, the title will appear rendered too far down
    // on the y-axis if we use the regular top padding, so we need to adjust it
    // so that the text appears centered.
    gfx::Size minimum_size = GetMinimumUnselectedSize();
    int text_height = title_top + title_font_height + kBottomPadding;
    if (text_height > minimum_size.height())
      title_top -= (text_height - minimum_size.height()) / 2;

    int title_width;
    if (close_button_->IsVisible()) {
      title_width = std::max(close_button_->x() -
                             kTitleCloseButtonSpacing - title_left, 0);
    } else {
      title_width = std::max(lb.width() - title_left, 0);
    }
    title_bounds_.SetRect(title_left, title_top, title_width,
                          title_font_height);
  } else {
    title_bounds_.SetRect(title_left, title_top, 0, 0);
  }

  // Certain UI elements within the Tab (the favicon, etc.) are not represented
  // as child Views (which is the preferred method).  Instead, these UI elements
  // are drawn directly on the canvas from within Tab::Paint(). The Tab's child
  // Views (for example, the Tab's close button which is a views::Button
  // instance) are automatically mirrored by the mirroring infrastructure in
  // views. The elements Tab draws directly on the canvas need to be manually
  // mirrored if the View's layout is right-to-left.
  favicon_bounds_.set_x(MirroredLeftPointForRect(favicon_bounds_));
  title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));
}

void TabRenderer::ThemeChanged() {
  LoadTabImages();
  View::ThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// TabRenderer, AnimationDelegate implementation:

void TabRenderer::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void TabRenderer::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void TabRenderer::AnimationEnded(const Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, private

void TabRenderer::PaintTitle(SkColor title_color, gfx::Canvas* canvas) {
  // Paint the Title.
  string16 title = data_.title;
  if (title.empty()) {
    title = data_.loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        TabContents::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  canvas->DrawStringInt(UTF16ToWideHack(title), *title_font, title_color,
                        title_bounds_.x(), title_bounds_.y(),
                        title_bounds_.width(), title_bounds_.height());
}

void TabRenderer::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsSelected()) {
    PaintActiveTabBackground(canvas);
  } else {
    if (mini_title_animation_.get() && mini_title_animation_->is_animating())
      PaintInactiveTabBackgroundWithTitleChange(canvas);
    else
      PaintInactiveTabBackground(canvas);

    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      SkRect bounds;
      bounds.set(0, 0, SkIntToScalar(width()), SkIntToScalar(height()));
      canvas->saveLayerAlpha(&bounds, static_cast<int>(throb_value * 0xff),
                             SkCanvas::kARGB_ClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas);
      canvas->restore();
    }
  }
}

void TabRenderer::PaintInactiveTabBackgroundWithTitleChange(
    gfx::Canvas* canvas) {
  // Render the inactive tab background. We'll use this for clipping.
  gfx::Canvas background_canvas(width(), height(), false);
  PaintInactiveTabBackground(&background_canvas);

  SkBitmap background_image = background_canvas.ExtractBitmap();

  // Draw a radial gradient to hover_canvas.
  gfx::Canvas hover_canvas(width(), height(), false);
  int radius = kMiniTitleChangeGradientRadius;
  int x0 = width() + radius - kMiniTitleChangeInitialXOffset;
  int x1 = radius;
  int x2 = -radius;
  int x;
  if (mini_title_animation_->current_part_index() == 0) {
    x = mini_title_animation_->CurrentValueBetween(x0, x1);
  } else if (mini_title_animation_->current_part_index() == 1) {
    x = x1;
  } else {
    x = mini_title_animation_->CurrentValueBetween(x1, x2);
  }
  SkPaint paint;
  SkPoint loc = { SkIntToScalar(x), SkIntToScalar(0) };
  SkColor colors[2];
  colors[0] = kMiniTitleChangeGradientColor1;
  colors[1] = kMiniTitleChangeGradientColor2;
  SkShader* shader = SkGradientShader::CreateRadial(
      loc,
      SkIntToScalar(radius),
      colors,
      NULL,
      2,
      SkShader::kClamp_TileMode);
  paint.setShader(shader);
  shader->unref();
  hover_canvas.FillRectInt(x - radius, -radius, radius * 2, radius * 2, paint);

  // Draw the radial gradient clipped to the background into hover_image.
  SkBitmap hover_image = SkBitmapOperations::CreateMaskedBitmap(
      hover_canvas.ExtractBitmap(), background_image);

  // Draw the tab background to the canvas.
  canvas->DrawBitmapInt(background_image, 0, 0);

  // And then the gradient on top of that.
  if (mini_title_animation_->current_part_index() == 2) {
    canvas->saveLayerAlpha(NULL,
                           mini_title_animation_->CurrentValueBetween(255, 0));
    canvas->DrawBitmapInt(hover_image, 0, 0);
    canvas->restore();
  } else {
    canvas->DrawBitmapInt(hover_image, 0, 0);
  }
}

void TabRenderer::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  bool is_otr = data_.off_the_record;

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();

  int tab_id;
  if (GetWidget() &&
      GetWidget()->GetWindow()->GetNonClientView()->UseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else {
    tab_id = is_otr ? IDR_THEME_TAB_BACKGROUND_INCOGNITO :
                      IDR_THEME_TAB_BACKGROUND;
  }

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(tab_id);

  // App tabs are drawn slightly differently (as nano tabs).
  TabImage* tab_image = data_.app ? &tab_active_nano : &tab_active;
  TabImage* tab_inactive_image = data_.app ? &tab_inactive_nano :
                                             &tab_inactive;
  TabImage* alpha = data_.app ? &tab_alpha_nano : &tab_alpha;

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int bg_offset_y = GetThemeProvider()->HasCustomImage(tab_id) ?
      0 : background_offset_.y();

  // Draw left edge.  Don't draw over the toolbar, as we're not the foreground
  // tab.
  SkBitmap tab_l = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, bg_offset_y, tab_image->l_width, height());
  SkBitmap theme_l =
      SkBitmapOperations::CreateMaskedBitmap(tab_l, *alpha->image_l);
  canvas->DrawBitmapInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  SkBitmap tab_r = SkBitmapOperations::CreateTiledBitmap(*tab_bg,
      offset + width() - tab_image->r_width, bg_offset_y,
      tab_image->r_width, height());
  SkBitmap theme_r =
      SkBitmapOperations::CreateMaskedBitmap(tab_r, *alpha->image_r);
  canvas->DrawBitmapInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over
  // it by incrementing by kDropShadowHeight, since it's a simple rectangle.
  // And again, don't draw over the toolbar.
  canvas->TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     bg_offset_y + kDropShadowHeight + tab_image->y_offset,
     tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - kDropShadowHeight - kToolbarOverlap - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_inactive_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_image->image_c,
                       tab_inactive_image->l_width, 0,
                       width() - tab_inactive_image->l_width -
                           tab_inactive_image->r_width,
                       height());
  canvas->DrawBitmapInt(*tab_inactive_image->image_r,
                        width() - tab_inactive_image->r_width, 0);
}

void TabRenderer::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();
  ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    NOTREACHED() << "Unable to get theme provider";

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // App tabs are drawn slightly differently (as nano tabs).
  TabImage* tab_image = data_.app ? &tab_active_nano : &tab_active;
  TabImage* alpha = data_.app ? &tab_alpha_nano : &tab_alpha;

  // Draw left edge.
  SkBitmap tab_l = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_image->l_width, height());
  SkBitmap theme_l =
      SkBitmapOperations::CreateMaskedBitmap(tab_l, *alpha->image_l);
  canvas->DrawBitmapInt(theme_l, 0, 0);

  // Draw right edge.
  SkBitmap tab_r = SkBitmapOperations::CreateTiledBitmap(*tab_bg,
      offset + width() - tab_image->r_width, 0, tab_image->r_width, height());
  SkBitmap theme_r =
      SkBitmapOperations::CreateMaskedBitmap(tab_r, *alpha->image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_image->r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.
  canvas->TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - kDropShadowHeight - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_image->image_c, tab_image->l_width, 0,
      width() - tab_image->l_width - tab_image->r_width, height());
  canvas->DrawBitmapInt(*tab_image->image_r, width() - tab_image->r_width, 0);
}

void TabRenderer::PaintLoadingAnimation(gfx::Canvas* canvas) {
  SkBitmap* frames = (animation_state_ == ANIMATION_WAITING) ?
                      waiting_animation_frames : loading_animation_frames;
  int image_size = frames->height();
  int image_offset = animation_frame_ * image_size;
  int dst_y = (height() - image_size) / 2;

  // Just like with the Tab's title and favicon, the position for the page
  // loading animation also needs to be mirrored if the View's UI layout is
  // right-to-left.
  int dst_x;
  if (mini()) {
    dst_x = favicon_bounds_.x();
    if (favicon_bounds_.width() != kFavIconSize)
      dst_x += (favicon_bounds_.width() - kFavIconSize) / 2;
  } else {
    if (UILayoutIsRightToLeft()) {
      dst_x = width() - kLeftPadding - image_size;
    } else {
      dst_x = kLeftPadding;
    }
  }
  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                        image_size, dst_x, dst_y, image_size, image_size,
                        false);
}

void TabRenderer::PaintAsNewTab(gfx::Canvas* canvas) {
  bool is_otr = data_.off_the_record;

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();

  int tab_id;
  if (GetWidget() &&
      GetWidget()->GetWindow()->GetNonClientView()->UseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else {
    tab_id = is_otr ? IDR_THEME_TAB_BACKGROUND_INCOGNITO :
                      IDR_THEME_TAB_BACKGROUND;
  }

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(tab_id);

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int bg_offset_y = GetThemeProvider()->HasCustomImage(tab_id) ?
      0 : background_offset_.y();

  SkBitmap image = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, bg_offset_y, new_tab_mask->width(),
      new_tab_mask->height());
  image = SkBitmapOperations::CreateMaskedBitmap(image, *new_tab_mask);
  canvas->DrawBitmapInt(image,
      0, 0, image.width(), image.height(),
      0, 0, image.width(), image.height(),
      false);

  canvas->DrawBitmapInt(*new_tab_shadow,
      0, 0, new_tab_shadow->width(), new_tab_shadow->height(),
      0, 0, new_tab_shadow->width(), new_tab_shadow->height(),
      false);
}

int TabRenderer::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFavIconSize;
}

bool TabRenderer::ShouldShowIcon() const {
  if (mini() && height() >= GetMinimumUnselectedSize().height())
    return true;
  if (!data_.show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool TabRenderer::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return !mini() && (IsSelected() || IconCapacity() >= 3);
}

double TabRenderer::GetThrobValue() {
  if (data_.alpha != 1)
    return data_.alpha;

  if (pulse_animation_->is_animating())
    return pulse_animation_->GetCurrentValue() * kHoverOpacity;

  return hover_animation_.get() ?
      kHoverOpacity * hover_animation_->GetCurrentValue() : 0;
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, private:

void TabRenderer::StartCrashAnimation() {
  if (!crash_animation_)
    crash_animation_ = new FavIconCrashAnimation(this);
  crash_animation_->Stop();
  crash_animation_->Start();
}

void TabRenderer::StopCrashAnimation() {
  if (!crash_animation_)
    return;
  crash_animation_->Stop();
}

bool TabRenderer::IsPerformingCrashAnimation() const {
  return crash_animation_ && crash_animation_->is_animating();
}

void TabRenderer::SetFavIconHidingOffset(int offset) {
  fav_icon_hiding_offset_ = offset;
  SchedulePaint();
}

void TabRenderer::DisplayCrashedFavIcon() {
  should_display_crashed_favicon_ = true;
}

void TabRenderer::ResetCrashedFavIcon() {
  should_display_crashed_favicon_ = false;
}

// static
void TabRenderer::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_alpha_nano.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_NANO_LEFT);
  tab_alpha_nano.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_NANO_RIGHT);

  tab_active.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active.l_width = tab_active.image_l->width();
  tab_active.r_width = tab_active.image_r->width();

  // The regular tab is much taller *visually* than the nano tabs.
  // The images are the same height, this is really just the difference
  // in whitespace above the tab image (regular vs nano).
  const int kNanoTabDiffHeight = 13;

  tab_active_nano.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_NANO_LEFT);
  tab_active_nano.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_NANO_CENTER);
  tab_active_nano.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_NANO_RIGHT);
  tab_active_nano.l_width = tab_active_nano.image_l->width();
  tab_active_nano.r_width = tab_active_nano.image_r->width();
  tab_active_nano.y_offset = kNanoTabDiffHeight;

  tab_inactive.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive.l_width = tab_inactive.image_l->width();
  tab_inactive.r_width = tab_inactive.image_r->width();

  tab_inactive_nano.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_NANO_LEFT);
  tab_inactive_nano.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_NANO_CENTER);
  tab_inactive_nano.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_NANO_RIGHT);
  tab_inactive_nano.l_width = tab_inactive_nano.image_l->width();
  tab_inactive_nano.r_width = tab_inactive_nano.image_r->width();
  tab_inactive_nano.y_offset = kNanoTabDiffHeight;

  loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
  waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);

  new_tab_mask = rb.GetBitmapNamed(IDR_TAB_ALPHA_NEW_TAB);
  new_tab_shadow = rb.GetBitmapNamed(IDR_TAB_NEW_TAB_SHADOW);
}

void TabRenderer::SetBlocked(bool blocked) {
  if (data_.blocked == blocked)
    return;
  data_.blocked = blocked;
  if (blocked)
    StartPulse();
  else
    StopPulse();
}
