// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_view_base.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/controls/separator.h"
#include "views/standard_layout.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

FirstRunViewBase::FirstRunViewBase(Profile* profile, bool homepage_defined,
                                   int import_items, int dont_import_items,
                                   bool search_engine_experiment)
    : preferred_width_(0),
      background_image_(NULL),
      separator_1_(NULL),
      default_browser_(NULL),
      non_default_browser_label_(NULL),
      separator_2_(NULL),
      importer_host_(NULL),
      profile_(profile),
      homepage_defined_(homepage_defined),
      import_items_(import_items),
      dont_import_items_(dont_import_items),
      search_engine_experiment_(search_engine_experiment) {
  DCHECK(profile);
  SetupControls();
}

FirstRunViewBase::~FirstRunViewBase() {
  FirstRun::SetShowFirstRunBubblePref();
  FirstRun::SetShowWelcomePagePref();
}

void FirstRunViewBase::SetupControls() {
  using views::Label;
  using views::ImageView;
  using views::Background;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new views::ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON));
  background_image_->SetHorizontalAlignment(ImageView::TRAILING);

  int color = 0;
  {
    SkAutoLockPixels pixel_loc(background_image_->GetImage());
    uint32_t* pixel = background_image_->GetImage().getAddr32(0, 0);
    color = (0xff & (*pixel));
  }
  Background* bkg = Background::CreateSolidBackground(color, color, color);

  // The bitmap we use as the background contains a clipped logo and therefore
  // we can not automatically mirror it for RTL UIs by simply flipping it. This
  // is why we load a different bitmap if the View is using a right-to-left UI
  // layout.
  //
  // Note that we first load the LTR image and then replace it with the RTL
  // image because the code above derives the background color from the LTR
  // image so we have to use the LTR logo initially and then replace it with
  // the RTL logo if we find out that we are running in a right-to-left locale.
  if (base::i18n::IsRTL())
    background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON_RTL));

  background_image_->set_background(bkg);
  AddChildView(background_image_);

  // The first separator marks the end of the image.
  separator_1_ = new views::Separator;
  AddChildView(separator_1_);

  if (BrowserDistribution::GetDistribution()->CanSetAsDefault()) {
    // The "make us default browser" check box.
    default_browser_ = new views::Checkbox(
        l10n_util::GetString(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER));
    default_browser_->SetMultiLine(true);
    AddChildView(default_browser_);
    default_browser_->set_listener(this);
  } else {
    non_default_browser_label_ = new Label(
        l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_SXS,
                              l10n_util::GetString(IDS_PRODUCT_NAME)));
    non_default_browser_label_->SetMultiLine(true);
    non_default_browser_label_->SetHorizontalAlignment(
        views::Label::ALIGN_LEFT);
    AddChildView(non_default_browser_label_);
  }

  // The second separator marks the start of buttons.
  separator_2_ = new views::Separator;
  AddChildView(separator_2_);
}

void FirstRunViewBase::AdjustDialogWidth(const views::View* sub_view) {
  gfx::Rect sub_view_bounds = sub_view->bounds();
  preferred_width_ =
      std::max(preferred_width_,
               static_cast<int>(sub_view_bounds.right()) + kPanelHorizMargin);
}

void FirstRunViewBase::SetMinimumDialogWidth(int width) {
  preferred_width_ = std::max(preferred_width_, width);
}

void FirstRunViewBase::Layout() {
  const int kVertSpacing = 8;

  gfx::Size canvas = GetPreferredSize();

  gfx::Size pref_size = background_image_->GetPreferredSize();
  background_image_->SetBounds(0, 0, canvas.width(), pref_size.height());

  int next_v_space = background_image_->y() +
                     background_image_->height() - 2;

  pref_size = separator_1_->GetPreferredSize();
  separator_1_->SetBounds(0, next_v_space, canvas.width() + 1,
                          pref_size.height());

  next_v_space = canvas.height() - kPanelSubVerticalSpacing - 2 * kVertSpacing;
  pref_size = separator_2_->GetPreferredSize();
  separator_2_->SetBounds(kPanelHorizMargin , next_v_space,
                          canvas.width() - 2 * kPanelHorizMargin,
                          pref_size.height());

  next_v_space = separator_2_->y() + separator_2_->height() + kVertSpacing;

  int width = canvas.width() - 2 * kPanelHorizMargin;
  if (default_browser_) {
#if defined(OS_WIN)
    // Add or remove a shield icon before calculating the button width.
    // (If a button has a shield icon, Windows automatically adds the icon width
    // to the button width.)
    views::DialogClientView* client_view = GetDialogClientView();
    if (client_view)
      client_view->ok_button()->SetNeedElevation(default_browser_->checked());
#endif

    int height = default_browser_->GetHeightForWidth(width);
    default_browser_->SetBounds(kPanelHorizMargin, next_v_space, width, height);
    AdjustDialogWidth(default_browser_);
  } else {
    int height = non_default_browser_label_->GetHeightForWidth(width);
    non_default_browser_label_->SetBounds(kPanelHorizMargin, next_v_space,
                                          width, height);
    AdjustDialogWidth(non_default_browser_label_);
  }
}

void FirstRunViewBase::ButtonPressed(views::Button* sender,
                                     const views::Event& event) {
#if defined(OS_WIN)
  if (default_browser_ && sender == default_browser_) {
    // Update the elevation state of the "start chromium" button so we can add
    // a shield icon when we need elevation.
    views::DialogClientView* client_view = GetDialogClientView();
    client_view->ok_button()->SetNeedElevation(default_browser_->checked());
  }
#endif
}

bool FirstRunViewBase::CanResize() const {
  return false;
}

bool FirstRunViewBase::CanMaximize() const {
  return false;
}

bool FirstRunViewBase::IsAlwaysOnTop() const {
  return false;
}

bool FirstRunViewBase::HasAlwaysOnTopMenu() const {
  return false;
}

std::wstring FirstRunViewBase::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (MessageBoxFlags::DIALOGBUTTON_OK == button)
    return search_engine_experiment_ ?
        l10n_util::GetString(IDS_ACCNAME_NEXT) :
        l10n_util::GetString(IDS_FIRSTRUN_DLG_OK);
  // The other buttons get the default text.
  return std::wstring();
}

int FirstRunViewBase::GetImportItems() const {
  // It is best to avoid importing cookies because there is a bug that make
  // the process take way too much time among other issues. So for the time
  // being we say: TODO(CPU): Bug 1196875
  int items = import_items_;
  if (!(dont_import_items_ & importer::HISTORY))
    items = items | importer::HISTORY;
  if (!(dont_import_items_ & importer::FAVORITES))
    items = items | importer::FAVORITES;
  if (!(dont_import_items_ & importer::PASSWORDS))
    items = items | importer::PASSWORDS;
  if (!(dont_import_items_ & importer::SEARCH_ENGINES))
    items = items | importer::SEARCH_ENGINES;
  if (!homepage_defined_)
    items = items | importer::HOME_PAGE;
  return items;
};

void FirstRunViewBase::DisableButtons() {
  window()->EnableClose(false);
  views::DialogClientView* dcv = GetDialogClientView();
  dcv->ok_button()->SetEnabled(false);
  dcv->cancel_button()->SetEnabled(false);
  if (default_browser_)
    default_browser_->SetEnabled(false);
}

bool FirstRunViewBase::CreateDesktopShortcut() {
  return FirstRun::CreateChromeDesktopShortcut();
}

bool FirstRunViewBase::CreateQuickLaunchShortcut() {
  return FirstRun::CreateChromeQuickLaunchShortcut();
}

bool FirstRunViewBase::SetDefaultBrowser() {
  UserMetrics::RecordAction(UserMetricsAction("FirstRun_Do_DefBrowser"),
                            profile_);
  return ShellIntegration::SetAsDefaultBrowser();
}

bool FirstRunViewBase::FirstRunComplete() {
  return FirstRun::CreateSentinel();
}
