// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_installed_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"
#include "views/view.h"

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 270;

// The InfoBubble uses a BubbleBorder which adds about 6 pixels of whitespace
// around the content view. We compensate by reducing our outer borders by this
// amount.
const int kBubbleBorderInsert = 6;
const int kHorizOuterMargin = kPanelHorizMargin - kBubbleBorderInsert;
const int kVertOuterMargin = kPanelVertMargin - kBubbleBorderInsert;

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubble. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View {
 public:
  InstalledBubbleContent(Extension* extension,
                         ExtensionInstalledBubble::BubbleType type,
                         SkBitmap* icon)
      : type_(type),
        info_(NULL) {
    const gfx::Font& font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

    // Scale down to 43x43, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(
        l10n_util::GetStringF(IDS_EXTENSION_INSTALLED_HEADING,
                              UTF8ToWide(extension->name())));
    heading_->SetFont(font.DeriveFont(3, gfx::Font::NORMAL));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      info_ = new views::Label(l10n_util::GetString(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO));
      info_->SetFont(font);
      info_->SetMultiLine(true);
      info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(info_);
    }

    manage_ = new views::Label(l10n_util::GetString(
      IDS_EXTENSION_INSTALLED_MANAGE_INFO));
    manage_->SetFont(font);
    manage_->SetMultiLine(true);
    manage_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(manage_);
  }

 private:
  virtual gfx::Size GetPreferredSize() {
    int width = kRightColumnWidth + kHorizOuterMargin + kHorizOuterMargin;
    width += kIconSize;
    width += kPanelHorizMargin;

    int height = kVertOuterMargin * 2;
    height += heading_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;
    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      height += info_->GetHeightForWidth(kRightColumnWidth);
      height += kPanelVertMargin;
    }
    height += manage_->GetHeightForWidth(kRightColumnWidth);

    return gfx::Size(width, std::max(height, kIconSize + kVertOuterMargin * 2));
  }

  virtual void Layout() {
    int x = kHorizOuterMargin;
    int y = kVertOuterMargin;

    icon_->SetBounds(x, y, kIconSize, kIconSize);
    x += kIconSize;
    x += kPanelHorizMargin;

    heading_->SizeToFit(kRightColumnWidth);
    heading_->SetX(x);
    heading_->SetY(y);
    y += heading_->height();
    y += kPanelVertMargin;

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      info_->SizeToFit(kRightColumnWidth);
      info_->SetX(x);
      info_->SetY(y);
      y += info_->height();
      y += kPanelVertMargin;
    }

    manage_->SizeToFit(kRightColumnWidth);
    manage_->SetX(x);
    manage_->SetY(y);
  }

  ExtensionInstalledBubble::BubbleType type_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* info_;
  views::Label* manage_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

}  // namespace

void ExtensionInstalledBubble::Show(Extension *extension, Browser *browser,
                                    SkBitmap icon) {
  new ExtensionInstalledBubble(extension, browser, icon);
}

ExtensionInstalledBubble::ExtensionInstalledBubble(Extension *extension,
                                                   Browser *browser,
                                                   SkBitmap icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon) {
  AddRef();  // Balanced in InfoBubbleClosing.

  if (extension_->browser_action()) {
    type_ = BROWSER_ACTION;
  } else if (extension->page_action() &&
             !extension->page_action()->default_icon_path().empty()) {
    type_ = PAGE_ACTION;
  } else {
    type_ = GENERIC;
  }

  // |extension| has been initialized but not loaded at this point. We need
  // to wait on showing the Bubble until not only the EXTENSION_LOADED gets
  // fired, but all of the EXTENSION_LOADED Observers have run. Only then can we
  // be sure that a BrowserAction or PageAction has had views created which we
  // can inspect for the purpose of previewing of pointing to them.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
      NotificationService::AllSources());
}

void ExtensionInstalledBubble::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED) {
    Extension* extension = Details<Extension>(details).ptr();
    if (extension == extension_) {
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &ExtensionInstalledBubble::ShowInternal));
    }
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionInstalledBubble::ShowInternal() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_->window()->GetNativeHandle());

  views::View* reference_view = NULL;
  if (type_ == BROWSER_ACTION) {
    reference_view = browser_view->GetToolbarView()->browser_actions()
        ->GetBrowserActionView(extension_);
    DCHECK(reference_view);
  } else if (type_ == PAGE_ACTION) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);  // preview_enabled
    reference_view = location_bar_view->GetPageActionView(
        extension_->page_action());
    DCHECK(reference_view);
  }

  // Default case.
  if (reference_view == NULL)
    reference_view = browser_view->GetToolbarView()->app_menu();

  gfx::Point origin;
  views::View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect bounds = reference_view->bounds();
  bounds.set_x(origin.x());
  bounds.set_y(origin.y());

  views::View* bubble_content = new InstalledBubbleContent(extension_, type_,
      &icon_);
  InfoBubble::Show(browser_view->GetWindow(), bounds, bubble_content, this);
}

// InfoBubbleDelegate
void ExtensionInstalledBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                                 bool closed_by_escape) {
  if (extension_->page_action()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        browser_->window()->GetNativeHandle());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extension_->page_action(),
        false);  // preview_enabled
  }
  Release();  // Balanced in ctor.
}
