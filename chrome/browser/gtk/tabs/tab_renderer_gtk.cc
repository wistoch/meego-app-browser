// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_renderer_gtk.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"

namespace {

const int kLeftPadding = 16;
const int kTopPadding = 6;
const int kRightPadding = 15;
const int kBottomPadding = 5;
const int kFavIconTitleSpacing = 4;
const int kTitleCloseButtonSpacing = 5;
const int kStandardTitleWidth = 175;
const int kFavIconSize = 16;
const int kSelectedTitleColor = SK_ColorBLACK;
const int kUnselectedTitleColor = SkColorSetRGB(64, 64, 64);

// The vertical and horizontal offset used to position the close button
// in the tab. TODO(jhawkins): Ask pkasting what the Fuzz is about.
const int kCloseButtonVertFuzz = 0;
const int kCloseButtonHorzFuzz = 5;

// How opaque to make the hover state (out of 1).
const double kHoverOpacity = 0.33;

// TODO(jhawkins): Move this code into ChromeFont and allow pulling out a
// GdkFont*.
GdkFont* load_default_font() {
  GtkSettings* settings = gtk_settings_get_default();

  GValue value = {0};
  g_value_init(&value, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(settings), "gtk-font-name", &value);

  gchar* font_name = g_strdup_value_contents(&value);
  PangoFontDescription* font_description =
      pango_font_description_from_string(font_name);

  GdkFont* font = gdk_font_from_description(font_description);
  g_free(font_name);
  return font;
}

TabRendererGtk::LoadingAnimation::Data loading_animation_data;

// Loads the loading animation images and data.
void InitializeLoadingAnimationData(
    ResourceBundle* rb, TabRendererGtk::LoadingAnimation::Data* data) {
  // The loading animation image is a strip of states. Each state must be
  // square, so the height must divide the width evenly.
  data->loading_animation_frames = rb->GetBitmapNamed(IDR_THROBBER);
  DCHECK(data->loading_animation_frames);
  DCHECK_EQ(data->loading_animation_frames->width() %
            data->loading_animation_frames->height(), 0);
  data->loading_animation_frame_count =
      data->loading_animation_frames->width() /
      data->loading_animation_frames->height();

  data->waiting_animation_frames =
      rb->GetBitmapNamed(IDR_THROBBER_WAITING);
  DCHECK(data->waiting_animation_frames);
  DCHECK_EQ(data->waiting_animation_frames->width() %
            data->waiting_animation_frames->height(), 0);
  data->waiting_animation_frame_count =
      data->waiting_animation_frames->width() /
      data->waiting_animation_frames->height();

  data->waiting_to_loading_frame_count_ratio =
      data->waiting_animation_frame_count /
      data->loading_animation_frame_count;
  // TODO(beng): eventually remove this when we have a proper themeing system.
  //             themes not supporting IDR_THROBBER_WAITING are causing this
  //             value to be 0 which causes DIV0 crashes. The value of 5
  //             matches the current bitmaps in our source.
  if (data->waiting_to_loading_frame_count_ratio == 0)
    data->waiting_to_loading_frame_count_ratio = 5;
}

}  // namespace

bool TabRendererGtk::initialized_ = false;
TabRendererGtk::TabImage TabRendererGtk::tab_active_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_inactive_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_inactive_otr_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_hover_ = {0};
TabRendererGtk::ButtonImage TabRendererGtk::close_button_ = {0};
ChromeFont* TabRendererGtk::title_font_ = NULL;
int TabRendererGtk::title_font_height_ = 0;
SkBitmap* TabRendererGtk::download_icon_ = NULL;
int TabRendererGtk::download_icon_width_ = 0;
int TabRendererGtk::download_icon_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk::LoadingAnimation, public:
//
TabRendererGtk::LoadingAnimation::LoadingAnimation(const Data* data)
    : data_(data), animation_state_(ANIMATION_NONE), animation_frame_(0) {
}

void TabRendererGtk::LoadingAnimation::ValidateLoadingAnimation(
    AnimationState animation_state) {
  if (animation_state_ != animation_state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (animation_state_ == ANIMATION_WAITING &&
        animation_state == ANIMATION_LOADING) {
      animation_frame_ = data_->loading_animation_frame_count -
          (animation_frame_ / data_->waiting_to_loading_frame_count_ratio);
    }
    animation_state_ = animation_state;
  }

  if (animation_state_ != ANIMATION_NONE) {
    animation_frame_ = ++animation_frame_ %
                       ((animation_state_ == ANIMATION_WAITING) ?
                         data_->waiting_animation_frame_count :
                         data_->loading_animation_frame_count);
  } else {
    animation_frame_ = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, public:

TabRendererGtk::TabRendererGtk()
    : showing_icon_(false),
      showing_download_icon_(false),
      showing_close_button_(false),
      fav_icon_hiding_offset_(0),
      should_display_crashed_favicon_(false),
      hovering_(false),
      close_button_state_(BS_NORMAL),
      loading_animation_(&loading_animation_data) {
  InitResources();
}

TabRendererGtk::~TabRendererGtk() {
}

void TabRendererGtk::UpdateData(TabContents* contents, bool loading_only) {
  DCHECK(contents);
  if (!loading_only) {
    data_.title = UTF16ToWideHack(contents->GetTitle());
    data_.off_the_record = contents->profile()->IsOffTheRecord();
    data_.show_download_icon = contents->IsDownloadShelfVisible();
    data_.crashed = contents->is_crashed();
    data_.favicon = contents->GetFavIcon();
  }

  // Loading state also involves whether we show the favicon, since that's where
  // we display the throbber.
  data_.loading = contents->is_loading();
  data_.show_icon = contents->ShouldDisplayFavIcon();
}

void TabRendererGtk::UpdateFromModel() {
  // Force a layout, since the tab may have grown a favicon.
  Layout();
}

bool TabRendererGtk::IsSelected() const {
  return true;
}

void TabRendererGtk::ValidateLoadingAnimation(AnimationState animation_state) {
  loading_animation_.ValidateLoadingAnimation(animation_state);
}

// static
gfx::Size TabRendererGtk::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.Enlarge(kFavIconTitleSpacing + kStandardTitleWidth, 0);
  return standard_size;
}

// static
int TabRendererGtk::GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, title_font_height_);
  return std::max(content_height, close_button_.height);
}

// static
void TabRendererGtk::LoadTabImages() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_active_.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_inactive_.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();

  tab_hover_.image_l = rb.GetBitmapNamed(IDR_TAB_HOVER_LEFT);
  tab_hover_.image_c = rb.GetBitmapNamed(IDR_TAB_HOVER_CENTER);
  tab_hover_.image_r = rb.GetBitmapNamed(IDR_TAB_HOVER_RIGHT);

  tab_inactive_otr_.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT_OTR);
  tab_inactive_otr_.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER_OTR);
  tab_inactive_otr_.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT_OTR);

  // tab_[hover,inactive_otr] width are not used and are initialized to 0
  // during static initialization.

  close_button_.normal = rb.GetBitmapNamed(IDR_TAB_CLOSE);
  close_button_.hot = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
  close_button_.pushed = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
  close_button_.width = close_button_.normal->width();
  close_button_.height = close_button_.normal->height();

  download_icon_ = rb.GetBitmapNamed(IDR_DOWNLOAD_ICON);
  download_icon_width_ = download_icon_->width();
  download_icon_height_ = download_icon_->height();
}

void TabRendererGtk::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, protected:

std::wstring TabRendererGtk::GetTitle() const {
  return data_.title;
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, private:

void TabRendererGtk::Paint(ChromeCanvasPaint* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width())
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_download_icon = data_.show_download_icon;
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ ||
      show_download_icon != showing_download_icon_ ||
      show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  if (show_icon) {
    if (loading_animation_.animation_state() != ANIMATION_NONE) {
      PaintLoadingAnimation(canvas);
    } else if (!data_.favicon.isNull()) {
      canvas->DrawBitmapInt(data_.favicon, favicon_bounds_.x(),
                            favicon_bounds_.y() + fav_icon_hiding_offset_);
    }
  }

  if (show_download_icon) {
    canvas->DrawBitmapInt(*download_icon_,
                          download_icon_bounds_.x(), download_icon_bounds_.y());
  }

  // Paint the Title.
  std::wstring title = data_.title;
  if (title.empty()) {
    if (data_.loading) {
      title = l10n_util::GetString(IDS_TAB_LOADING_TITLE);
    } else {
      title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);
    }
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  SkColor title_color = IsSelected() ? kSelectedTitleColor
                                     : kUnselectedTitleColor;
  canvas->DrawStringInt(title, *title_font_, title_color, title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height());

  SkBitmap close_button = *close_button_.normal;
  if (close_button_state_ == BS_HOT)
    close_button = *close_button_.hot;
  else if (close_button_state_ == BS_PUSHED)
    close_button = *close_button_.pushed;

  canvas->DrawBitmapInt(close_button,
                        close_button_bounds_.x(), close_button_bounds_.y());
}

void TabRendererGtk::SetHovering(bool hovering) {
  hovering_ = hovering;

  // If the mouse is not hovering over the tab, the close button can't be
  // highlighted.
  if (!hovering)
    close_button_state_ = BS_NORMAL;
}

void TabRendererGtk::Layout() {
  gfx::Rect local_bounds = bounds_;
  if (local_bounds.IsEmpty())
    return;
  local_bounds.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // Figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    int favicon_top = kTopPadding + (content_height - kFavIconSize) / 2;
    favicon_bounds_.SetRect(local_bounds.x(), favicon_top,
                            kFavIconSize, kFavIconSize);
  } else {
    favicon_bounds_.SetRect(local_bounds.x(), local_bounds.y(), 0, 0);
  }

  // Size the download icon.
  showing_download_icon_ = data_.show_download_icon;
  if (showing_download_icon_) {
    int icon_top = kTopPadding + (content_height - download_icon_height_) / 2;
    download_icon_bounds_.SetRect(local_bounds.width() - download_icon_width_,
                                  icon_top, download_icon_width_,
                                  download_icon_height_);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_.height) / 2;
    close_button_bounds_.SetRect(bounds_.x() +
                                 local_bounds.width() + kCloseButtonHorzFuzz,
                                 bounds_.y() + close_button_top,
                                 close_button_.width, close_button_.height);
  } else {
    close_button_bounds_.SetRect(0, 0, 0, 0);
  }

  // Size the Title text to fill the remaining space.
  int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - title_font_height_) / 2;

  // If the user has big fonts, the title will appear rendered too far down on
  // the y-axis if we use the regular top padding, so we need to adjust it so
  // that the text appears centered.
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  int text_height = title_top + title_font_height_ + kBottomPadding;
  if (text_height > minimum_size.height())
    title_top -= (text_height - minimum_size.height()) / 2;

  int title_width;
  if (close_button_bounds_.width() && close_button_bounds_.height()) {
    title_width = std::max(close_button_bounds_.x() -
                           kTitleCloseButtonSpacing - title_left, 0);
  } else {
    title_width = std::max(local_bounds.width() - title_left, 0);
  }
  if (data_.show_download_icon)
    title_width = std::max(title_width - download_icon_width_, 0);
  title_bounds_.SetRect(title_left, title_top, title_width, title_font_height_);

  // TODO(jhawkins): Handle RTL layout.
}

void TabRendererGtk::PaintTabBackground(ChromeCanvasPaint* canvas) {
  if (IsSelected()) {
    // Sometimes detaching a tab quickly can result in the model reporting it
    // as not being selected, so is_drag_clone_ ensures that we always paint
    // the active representation for the dragged tab.
    PaintActiveTabBackground(canvas);
  } else {
    // Draw our hover state.
    // TODO(jhawkins): Hover animations.
    if (hovering_) {
      PaintHoverTabBackground(canvas, kHoverOpacity);
    } else {
      PaintInactiveTabBackground(canvas);
    }
  }
}

void TabRendererGtk::PaintInactiveTabBackground(ChromeCanvasPaint* canvas) {
  bool is_otr = data_.off_the_record;
  const TabImage& image = is_otr ? tab_inactive_otr_ : tab_inactive_;

  canvas->DrawBitmapInt(*image.image_l, bounds_.x(), bounds_.y());
  canvas->TileImageInt(*image.image_c,
                       bounds_.x() + tab_inactive_.l_width, bounds_.y(),
                       width() - tab_inactive_.l_width - tab_inactive_.r_width,
                       height());
  canvas->DrawBitmapInt(*image.image_r,
                       bounds_.x() + width() - tab_inactive_.r_width,
                       bounds_.y());
}

void TabRendererGtk::PaintHoverTabBackground(ChromeCanvasPaint* canvas,
                                             double opacity) {
  bool is_otr = data_.off_the_record;
  const TabImage& image = is_otr ? tab_inactive_otr_ : tab_inactive_;

  SkBitmap left = skia::ImageOperations::CreateBlendedBitmap(
      *image.image_l, *tab_hover_.image_l, opacity);
  SkBitmap center = skia::ImageOperations::CreateBlendedBitmap(
      *image.image_c, *tab_hover_.image_c, opacity);
  SkBitmap right = skia::ImageOperations::CreateBlendedBitmap(
      *image.image_r, *tab_hover_.image_r, opacity);

  canvas->DrawBitmapInt(left, bounds_.x(), bounds_.y());
  canvas->TileImageInt(center, bounds_.x() + tab_active_.l_width, bounds_.y(),
      bounds_.width() - tab_active_.l_width - tab_active_.r_width,
      bounds_.height());
  canvas->DrawBitmapInt(right,
      bounds_.x() + bounds_.width() - tab_active_.r_width, bounds_.y());
}

void TabRendererGtk::PaintActiveTabBackground(ChromeCanvasPaint* canvas) {
  canvas->DrawBitmapInt(*tab_active_.image_l, bounds_.x(), bounds_.y());
  canvas->TileImageInt(*tab_active_.image_c,
                       bounds_.x() + tab_active_.l_width, bounds_.y(),
                       width() - tab_active_.l_width - tab_active_.r_width,
                       height());
  canvas->DrawBitmapInt(*tab_active_.image_r,
                        bounds_.x() + width() - tab_active_.r_width,
                        bounds_.y());
}

void TabRendererGtk::PaintLoadingAnimation(ChromeCanvasPaint* canvas) {
  const SkBitmap* frames =
      (loading_animation_.animation_state() == ANIMATION_WAITING) ?
      loading_animation_.waiting_animation_frames() :
      loading_animation_.loading_animation_frames();
  const int image_size = frames->height();
  const int image_offset = loading_animation_.animation_frame() * image_size;
  const int dst_y = (height() - image_size) / 2;

  // Just like with the Tab's title and favicon, the position for the page
  // loading animation also needs to be mirrored if the UI layout is RTL.
  // TODO(willchan): Handle RTL.
  // dst_x = x() + width() - kLeftPadding - image_size;
  int dst_x = x() + kLeftPadding;


  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                        image_size, dst_x, dst_y, image_size, image_size,
                        false);
}

int TabRendererGtk::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFavIconSize;
}

bool TabRendererGtk::ShouldShowIcon() const {
  if (!data_.show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool TabRendererGtk::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return IsSelected() || IconCapacity() >= 3;
}

// static
void TabRendererGtk::InitResources() {
  if (initialized_)
    return;

  LoadTabImages();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  title_font_ = new ChromeFont(rb.GetFont(ResourceBundle::BaseFont));
  title_font_height_ = title_font_->height();

  InitializeLoadingAnimationData(&rb, &loading_animation_data);

  initialized_ = true;
}
