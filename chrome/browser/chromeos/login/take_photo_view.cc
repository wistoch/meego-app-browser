// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/take_photo_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/layout/grid_layout.h"

namespace {

// Padding between horizontally neighboring elements.
const int kHorizontalPadding = 10;
// Padding between vertically neighboring elements.
const int kVerticalPadding = 10;

// IDs of column sets for grid layout manager.
enum ColumnSets {
  kTitleRow,    // Column set for screen title.
  kImageRow,    // Column set for image from camera and snapshot button.
};

}  // namespace

namespace chromeos {

// Image view that can show center throbber above itself or a message at its
// bottom.
class CameraImageView : public views::ImageView {
 public:
  CameraImageView()
      : throbber_(NULL),
        message_(NULL) {}

  ~CameraImageView() {}

  void Init() {
    DCHECK(NULL == throbber_);
    DCHECK(NULL == message_);

    throbber_ = CreateDefaultSmoothedThrobber();
    throbber_->SetVisible(false);
    AddChildView(throbber_);

    message_ = new views::Label();
    message_->SetMultiLine(true);
    message_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message_->SetVisible(false);
    CorrectLabelFontSize(message_);
    AddChildView(message_);
  }

  void SetInitializingState() {
    ShowThrobber();
    SetMessage(std::wstring());
    SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_INITIALIZING));
  }

  void SetNormalState() {
    HideThrobber();
    SetMessage(std::wstring());
  }

  void SetErrorState() {
    HideThrobber();
    SetMessage(UTF16ToWide(l10n_util::GetStringUTF16(IDS_USER_IMAGE_NO_VIDEO)));
    SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_NO_VIDEO));
  }

 private:
  void ShowThrobber() {
    DCHECK(throbber_);
    throbber_->SetVisible(true);
    throbber_->Start();
  }

  void HideThrobber() {
    DCHECK(throbber_);
    throbber_->Stop();
    throbber_->SetVisible(false);
  }

  void SetMessage(const std::wstring& message) {
    DCHECK(message_);
    message_->SetText(message);
    message_->SetVisible(!message.empty());
    Layout();
  }

  // views::View override:
  virtual void Layout() {
    gfx::Size size = GetPreferredSize();
    if (throbber_->IsVisible()) {
      gfx::Size throbber_size = throbber_->GetPreferredSize();
      int throbber_x = (size.width() - throbber_size.width()) / 2;
      int throbber_y = (size.height() - throbber_size.height()) / 2;
      throbber_->SetBounds(throbber_x,
                           throbber_y,
                           throbber_size.width(),
                           throbber_size.height());
    }
    if (message_->IsVisible()) {
      message_->SizeToFit(size.width() - kHorizontalPadding * 2);
      gfx::Size message_size = message_->GetPreferredSize();
      int message_y = size.height() - kVerticalPadding - message_size.height();
      message_->SetBounds(kHorizontalPadding,
                          message_y,
                          message_size.width(),
                          message_size.height());
    }
  }

  // Throbber centered within the view.
  views::Throbber* throbber_;

  // Message, multiline, aligned to the bottom of the view.
  views::Label* message_;

  DISALLOW_COPY_AND_ASSIGN(CameraImageView);
};


TakePhotoView::TakePhotoView(Delegate* delegate)
    : title_label_(NULL),
      snapshot_button_(NULL),
      user_image_(NULL),
      is_capturing_(true),
      delegate_(delegate) {
}

TakePhotoView::~TakePhotoView() {
}

void TakePhotoView::Init() {
  title_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_USER_IMAGE_SCREEN_TITLE)));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  CorrectLabelFontSize(title_label_);

  user_image_ = new CameraImageView();
  user_image_->SetImageSize(
      gfx::Size(login::kUserImageSize, login::kUserImageSize));
  user_image_->Init();

  snapshot_button_ = new views::ImageButton(this);
  snapshot_button_->SetFocusable(true);
  snapshot_button_->SetImage(views::CustomButton::BS_NORMAL,
                             ResourceBundle::GetSharedInstance().GetBitmapNamed(
                                 IDR_USER_IMAGE_CAPTURE));
  snapshot_button_->SetImage(views::CustomButton::BS_DISABLED,
                             ResourceBundle::GetSharedInstance().GetBitmapNamed(
                                 IDR_USER_IMAGE_CAPTURE_DISABLED));
  snapshot_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_CHROMEOS_ACC_ACCOUNT_PICTURE));

  InitLayout();
  // Request focus only after the button is added to views hierarchy.
  snapshot_button_->RequestFocus();
  user_image_->SetInitializingState();
}

void TakePhotoView::InitLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(GetInsets());
  SetLayoutManager(layout);

  // The title is left-top aligned.
  views::ColumnSet* column_set = layout->AddColumnSet(kTitleRow);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);

  // User image and snapshot button are centered horizontally.
  column_set = layout->AddColumnSet(kImageRow);
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Fill the layout with rows and views now.
  layout->StartRow(0, kTitleRow);
  layout->AddView(title_label_);
  layout->StartRowWithPadding(0, kImageRow, 0, kVerticalPadding);
  layout->AddView(user_image_);
  layout->StartRowWithPadding(1, kImageRow, 0, kVerticalPadding);
  layout->AddView(snapshot_button_);
}

void TakePhotoView::UpdateVideoFrame(const SkBitmap& frame) {
  if (!is_capturing_)
    return;

  if (!snapshot_button_->IsEnabled()) {
    user_image_->SetNormalState();
    snapshot_button_->SetEnabled(true);
    snapshot_button_->RequestFocus();
  }
  SkBitmap user_image =
      skia::ImageOperations::Resize(
          frame,
          skia::ImageOperations::RESIZE_BOX,
          login::kUserImageSize,
          login::kUserImageSize);

  user_image_->SetImage(&user_image);
}

void TakePhotoView::ShowCameraInitializing() {
  if (!is_capturing_)
    return;
  snapshot_button_->SetEnabled(false);
  user_image_->SetInitializingState();
}

void TakePhotoView::ShowCameraError() {
  if (!is_capturing_)
    return;
  snapshot_button_->SetEnabled(false);
  user_image_->SetErrorState();
}

const SkBitmap& TakePhotoView::GetImage() const {
  return user_image_->GetImage();
}

gfx::Size TakePhotoView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void TakePhotoView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  DCHECK(sender == snapshot_button_);
  is_capturing_ = !is_capturing_;
  if (is_capturing_) {
    snapshot_button_->SetImage(
        views::CustomButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_CAPTURE));
    delegate_->OnCapturingStarted();
  } else {
    snapshot_button_->SetImage(
        views::CustomButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_RECYCLE));
    delegate_->OnCapturingStopped();
  }
  snapshot_button_->SchedulePaint();
}

}  // namespace chromeos
