// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/infobars.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/infobars/infobar_container.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/background.h"
#include "chrome/views/button.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"

#include "generated_resources.h"

const double kInfoBarHeight = 37.0;

static const int kVerticalPadding = 3;
static const int kHorizontalPadding = 3;
static const int kIconLabelSpacing = 5;
static const int kButtonSpacing = 5;

static const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
static const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

static const int kSeparatorLineHeight = 1;
static const SkColor kSeparatorColor = SkColorSetRGB(165, 165, 165);

namespace {
int OffsetY(views::View* parent, const gfx::Size prefsize) {
  return std::max((parent->height() - prefsize.height()) / 2, 0);
}
}

// InfoBarBackground -----------------------------------------------------------

class InfoBarBackground : public views::Background {
 public:
  InfoBarBackground() {
    gradient_background_.reset(
        views::Background::CreateVerticalGradientBackground(
            kBackgroundColorTop, kBackgroundColorBottom));
  }

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas, views::View* view) const {
    // First paint the gradient background.
    gradient_background_->Paint(canvas, view);

    // Now paint the separator line.
    canvas->FillRectInt(kSeparatorColor, 0,
                        view->height() - kSeparatorLineHeight, view->width(),
                        kSeparatorLineHeight);
  }

 private:
  scoped_ptr<views::Background> gradient_background_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarBackground);
};

// InfoBar, public: ------------------------------------------------------------

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : delegate_(delegate),
      close_button_(new views::Button) {
  set_background(new InfoBarBackground);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::Button::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::Button::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::Button::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetListener(this, 0);
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  animation_.reset(new SlideAnimation(this));
  animation_->SetTweenType(SlideAnimation::NONE);
}

InfoBar::~InfoBar() {
}

void InfoBar::AnimateOpen() {
  animation_->Show();
}

void InfoBar::Open() {
  animation_->Reset(1.0);
  animation_->Show();
}

void InfoBar::AnimateClose() {
  animation_->Hide();
}

void InfoBar::Close() {
  GetParent()->RemoveChildView(this);
  if (delegate())
    delegate()->InfoBarClosed();
  delete this;
}

// InfoBar, views::View overrides: ---------------------------------------------

gfx::Size InfoBar::GetPreferredSize() {
  int height = static_cast<int>(kInfoBarHeight * animation_->GetCurrentValue());
  return gfx::Size(0, height);
}

void InfoBar::Layout() {
  gfx::Size button_ps = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - kHorizontalPadding - button_ps.width(),
                           OffsetY(this, button_ps), button_ps.width(),
                           button_ps.height());

}

// InfoBar, protected: ---------------------------------------------------------

int InfoBar::GetAvailableWidth() const {
  return close_button_->x() - kIconLabelSpacing;
}

// InfoBar, views::BaseButton::ButtonListener implementation: ------------------

void InfoBar::ButtonPressed(views::BaseButton* sender) {
  if (sender == close_button_)
    container_->RemoveDelegate(delegate());
}

// InfoBar, AnimationDelegate implementation: ----------------------------------

void InfoBar::AnimationProgressed(const Animation* animation) {
  container_->InfoBarAnimated(true);
}

void InfoBar::AnimationEnded(const Animation* animation) {
  container_->InfoBarAnimated(false);

  if (!animation_->IsShowing())
    Close();
}

// AlertInfoBar, public: -------------------------------------------------------

AlertInfoBar::AlertInfoBar(AlertInfoBarDelegate* delegate)
    : InfoBar(delegate) {
  label_ = new views::Label(
      delegate->GetMessageText(),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);

  icon_ = new views::ImageView;
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);
}

AlertInfoBar::~AlertInfoBar() {

}

// AlertInfoBar, views::View overrides: ----------------------------------------

void AlertInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon and text.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  gfx::Size text_ps = label_->GetPreferredSize();
  int text_width =
      GetAvailableWidth() - icon_->bounds().right() - kIconLabelSpacing;
  label_->SetBounds(icon_->bounds().right() + kIconLabelSpacing,
                    OffsetY(this, text_ps), text_width, text_ps.height());
}

// AlertInfoBar, private: ------------------------------------------------------

AlertInfoBarDelegate* AlertInfoBar::GetDelegate() {
  return delegate()->AsAlertInfoBarDelegate();
}

// ConfirmInfoBar, public: -----------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : ok_button_(NULL),
      cancel_button_(NULL),
      initialized_(false),
      AlertInfoBar(delegate) {
}

ConfirmInfoBar::~ConfirmInfoBar() {
}

// ConfirmInfoBar, views::View overrides: --------------------------------------

void ConfirmInfoBar::Layout() {
  InfoBar::Layout();
  int available_width = InfoBar::GetAvailableWidth();
  int ok_button_width = 0;
  int cancel_button_width = 0;
  gfx::Size ok_ps = ok_button_->GetPreferredSize();
  gfx::Size cancel_ps = cancel_button_->GetPreferredSize();

  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK)
    ok_button_width = ok_ps.width();
  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL)
   cancel_button_width = cancel_ps.width();

  cancel_button_->SetBounds(available_width - cancel_button_width,
                            OffsetY(this, cancel_ps), cancel_ps.width(),
                            cancel_ps.height());
  int spacing = cancel_button_width > 0 ? kButtonSpacing : 0;
  ok_button_->SetBounds(cancel_button_->x() - spacing - ok_button_width,
                        OffsetY(this, ok_ps), ok_ps.width(), ok_ps.height());

  AlertInfoBar::Layout();
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this && !initialized_) {
    Init();
    initialized_ = true;
  }
}

// ConfirmInfoBar, views::NativeButton::Listener implementation: ---------------

void ConfirmInfoBar::ButtonPressed(views::NativeButton* sender) {
  if (sender == ok_button_) {
    GetDelegate()->Accept();
  } else if (sender == cancel_button_) {
    GetDelegate()->Cancel();
  } else {
    NOTREACHED();
  }
}

// ConfirmInfoBar, InfoBar overrides: ------------------------------------------

int ConfirmInfoBar::GetAvailableWidth() const {
  if (ok_button_)
    return ok_button_->x() - kButtonSpacing;
  if (cancel_button_)
    return cancel_button_->x() - kButtonSpacing;
  return InfoBar::GetAvailableWidth();
}

// ConfirmInfoBar, private: ----------------------------------------------------

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

void ConfirmInfoBar::Init() {
  ok_button_ = new views::NativeButton(
      GetDelegate()->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  ok_button_->SetListener(this);
  AddChildView(ok_button_);

  cancel_button_ = new views::NativeButton(
      GetDelegate()->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  cancel_button_->SetListener(this);
  AddChildView(cancel_button_);
}

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
