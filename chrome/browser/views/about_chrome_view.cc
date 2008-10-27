// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/about_chrome_view.h"

#include <commdlg.h>

#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "base/word_iterator.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/views/text_field.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "webkit/glue/webkit_glue.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {
// The pixel width of the version text field. Ideally, we'd like to have the
// bounds set to the edge of the icon. However, the icon is not a view but a
// part of the background, so we have to hard code the width to make sure
// the version field doesn't overlap it.
const int kVersionFieldWidth = 195;

// The URLs that you navigate to when clicking the links in the About dialog.
const wchar_t* const kChromiumUrl = L"http://www.chromium.org/";
const wchar_t* const kAcknowledgements = L"about:credits";
const wchar_t* const kTOS = L"about:terms";

// These are used as placeholder text around the links in the text in the about
// dialog.
const wchar_t* kBeginLink = L"BEGIN_LINK";
const wchar_t* kEndLink = L"END_LINK";
const wchar_t* kBeginLinkChr = L"BEGIN_LINK_CHR";
const wchar_t* kBeginLinkOss = L"BEGIN_LINK_OSS";
const wchar_t* kEndLinkChr = L"END_LINK_CHR";
const wchar_t* kEndLinkOss = L"END_LINK_OSS";

// Returns a substring from |text| between start and end.
std::wstring StringSubRange(const std::wstring& text, size_t start,
                            size_t end) {
  DCHECK(end > start);
  return text.substr(start, end - start);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, public:

AboutChromeView::AboutChromeView(Profile* profile)
    : profile_(profile),
      about_dlg_background_(NULL),
      about_title_label_(NULL),
      version_label_(NULL),
      copyright_label_(NULL),
      main_text_label_(NULL),
      main_text_label_height_(0),
      terms_of_service_url_(NULL),
      chromium_url_(NULL),
      open_source_url_(NULL),
      chromium_url_appears_first_(true),
      check_button_status_(CHECKBUTTON_HIDDEN) {
  DCHECK(profile);
  Init();

  google_updater_ = new GoogleUpdate();
  google_updater_->AddStatusChangeListener(this);
}

AboutChromeView::~AboutChromeView() {
  // The Google Updater will hold a pointer to us until it reports status, so we
  // need to let it know that we will no longer be listening.
  if (google_updater_)
    google_updater_->RemoveStatusChangeListener();
}

void AboutChromeView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get() == NULL) {
    NOTREACHED() << L"Failed to initialize about window";
    return;
  }

  current_version_ = version_info->file_version();

  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_.reset(new views::Throbber(50, true));
  throbber_->SetParentOwned(false);
  throbber_->SetVisible(false);

  SkBitmap* success_image = rb.GetBitmapNamed(IDR_UPDATE_UPTODATE);
  success_indicator_.SetImage(*success_image);
  success_indicator_.SetParentOwned(false);

  SkBitmap* update_available_image = rb.GetBitmapNamed(IDR_UPDATE_AVAILABLE);
  update_available_indicator_.SetImage(*update_available_image);
  update_available_indicator_.SetParentOwned(false);

  SkBitmap* timeout_image = rb.GetBitmapNamed(IDR_UPDATE_FAIL);
  timeout_indicator_.SetImage(*timeout_image);
  timeout_indicator_.SetParentOwned(false);

  update_label_.SetVisible(false);
  update_label_.SetParentOwned(false);

  // Regular view controls we draw by ourself. First, we add the background
  // image for the dialog. We have two different background bitmaps, one for
  // LTR UIs and one for RTL UIs. We load the correct bitmap based on the UI
  // layout of the view.
  about_dlg_background_ = new views::ImageView();
  SkBitmap* about_background;
  if (UILayoutIsRightToLeft())
    about_background = rb.GetBitmapNamed(IDR_ABOUT_BACKGROUND_RTL);
  else
    about_background = rb.GetBitmapNamed(IDR_ABOUT_BACKGROUND);

  about_dlg_background_->SetImage(*about_background);
  AddChildView(about_dlg_background_);

  // Add the dialog labels.
  about_title_label_ = new views::Label(
      l10n_util::GetString(IDS_PRODUCT_NAME));
  about_title_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(18, BOLD_FONTTYPE));
  AddChildView(about_title_label_);

  // This is a text field so people can copy the version number from the dialog.
  version_label_ = new views::TextField();
  version_label_->SetText(current_version_);
  version_label_->SetReadOnly(true);
  version_label_->RemoveBorder();
  version_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, BOLD_FONTTYPE));
  AddChildView(version_label_);

  // The copyright URL portion of the main label.
  copyright_label_ = new views::Label(
      l10n_util::GetString(IDS_ABOUT_VERSION_COPYRIGHT));
  copyright_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(copyright_label_);

  main_text_label_ = new views::Label(L"");

  // Figure out what to write in the main label of the About box.
  std::wstring text = l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE);

  chromium_url_appears_first_ =
      text.find(kBeginLinkChr) < text.find(kBeginLinkOss);

  size_t link1 = text.find(kBeginLink);
  DCHECK(link1 != std::wstring::npos);
  size_t link1_end = text.find(kEndLink, link1);
  DCHECK(link1_end != std::wstring::npos);
  size_t link2 = text.find(kBeginLink, link1_end);
  DCHECK(link2 != std::wstring::npos);
  size_t link2_end = text.find(kEndLink, link2);
  DCHECK(link1_end != std::wstring::npos);

  main_label_chunk1_ = text.substr(0, link1);
  main_label_chunk2_ = StringSubRange(text, link1_end + wcslen(kEndLinkOss),
                                      link2);
  main_label_chunk3_ = text.substr(link2_end + wcslen(kEndLinkOss));

  // The Chromium link within the main text of the dialog.
  chromium_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkChr) + wcslen(kBeginLinkChr),
                     text.find(kEndLinkChr)));
  AddChildView(chromium_url_);
  chromium_url_->SetController(this);

  // The Open Source link within the main text of the dialog.
  open_source_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkOss) + wcslen(kBeginLinkOss),
                     text.find(kEndLinkOss)));
  AddChildView(open_source_url_);
  open_source_url_->SetController(this);

#if defined(GOOGLE_CHROME_BUILD)
  std::vector<size_t> url_offsets;
  text = l10n_util::GetStringF(IDS_ABOUT_TERMS_OF_SERVICE,
                               std::wstring(),
                               std::wstring(),
                               &url_offsets);

  main_label_chunk4_ = text.substr(0, url_offsets[0]);
  main_label_chunk5_ = text.substr(url_offsets[0]);

  // The Terms of Service URL at the bottom.
  terms_of_service_url_ =
      new views::Link(l10n_util::GetString(IDS_TERMS_OF_SERVICE));
  AddChildView(terms_of_service_url_);
  terms_of_service_url_->SetController(this);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::View implementation:

gfx::Size AboutChromeView::GetPreferredSize() {
  gfx::Size prefsize(views::Window::GetLocalizedContentsSize(
      IDS_ABOUT_DIALOG_WIDTH_CHARS,
      IDS_ABOUT_DIALOG_HEIGHT_LINES));
  // We compute the height of the dialog based on the size of the image (it
  // would be nice to not hard code this), the text in the about dialog and the
  // margins around the text.
  prefsize.Enlarge(0, 145 + (kPanelVertMargin * 2));
  // TODO(beng): Eventually the image should be positioned such that hard-
  //             coding the width isn't necessary.  This breaks with fonts
  //             that are large and cause wrapping.
  prefsize.set_width(422);
  return prefsize;
}

void AboutChromeView::Layout() {
  gfx::Size panel_size = GetPreferredSize();

  // Background image for the dialog.
  gfx::Size sz = about_dlg_background_->GetPreferredSize();
  // used to position main text below.
  int background_image_height = sz.height();
  about_dlg_background_->SetBounds(0, 0, sz.width(), sz.height());

  // First label goes to the top left corner.
  sz = about_title_label_->GetPreferredSize();
  about_title_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                                sz.width(), sz.height());

  // Then we have the version number right below it.
  sz = version_label_->GetPreferredSize();
  version_label_->SetBounds(kPanelHorizMargin,
                            about_title_label_->y() +
                                about_title_label_->height() +
                                kRelatedControlVerticalSpacing,
                            kVersionFieldWidth,
                            sz.height());

  // For the width of the main text label we want to use up the whole panel
  // width and remaining height, minus a little margin on each side.
  int y_pos = background_image_height + kRelatedControlVerticalSpacing;
  sz.set_width(panel_size.width() - 2 * kPanelHorizMargin);

  // Draw the text right below the background image.
  copyright_label_->SetBounds(kPanelHorizMargin,
                              y_pos,
                              sz.width(),
                              sz.height());

  // Then the main_text_label.
  main_text_label_->SetBounds(kPanelHorizMargin,
                              copyright_label_->y() +
                                  copyright_label_->height(),
                              sz.width(),
                              main_text_label_height_);

  // Get the y-coordinate of our parent so we can position the text left of the
  // buttons at the bottom.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);

  sz = throbber_->GetPreferredSize();
  int throbber_topleft_x = kPanelHorizMargin;
  int throbber_topleft_y = parent_bounds.bottom() - sz.height() -
                           kButtonVEdgeMargin - 3;
  throbber_->SetBounds(throbber_topleft_x, throbber_topleft_y,
                       sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = success_indicator_.GetPreferredSize();
  success_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                               sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = update_available_indicator_.GetPreferredSize();
  update_available_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                                        sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = timeout_indicator_.GetPreferredSize();
  timeout_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                               sz.width(), sz.height());

  // The update label should be at the bottom of the screen, to the right of
  // the throbber. We specify width to the end of the dialog because it contains
  // variable length messages.
  sz = update_label_.GetPreferredSize();
  int update_label_x = throbber_->x() + throbber_->width() +
                       kRelatedControlHorizontalSpacing;
  update_label_.SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  update_label_.SetBounds(update_label_x,
                          throbber_topleft_y + 1,
                          parent_bounds.width() - update_label_x,
                          sz.height());
}


void AboutChromeView::Paint(ChromeCanvas* canvas) {
  views::View::Paint(canvas);

  ChromeFont font =
    ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

  const gfx::Rect label_bounds = main_text_label_->bounds();

  views::Link* link1 =
      chromium_url_appears_first_ ? chromium_url_ : open_source_url_;
  views::Link* link2 =
      chromium_url_appears_first_ ? open_source_url_ : chromium_url_;
  gfx::Rect* rect1 = chromium_url_appears_first_ ?
      &chromium_url_rect_ : &open_source_url_rect_;
  gfx::Rect* rect2 = chromium_url_appears_first_ ?
      &open_source_url_rect_ : &chromium_url_rect_;

  // This struct keeps track of where to write the next word (which x,y
  // pixel coordinate). This struct is updated after drawing text and checking
  // if we need to wrap.
  gfx::Size position;
  // Draw the first text chunk and position the Chromium url.
  DrawTextAndPositionUrl(canvas, main_label_chunk1_, link1,
                         rect1, &position, label_bounds, font);
  // Draw the second text chunk and position the Open Source url.
  DrawTextAndPositionUrl(canvas, main_label_chunk2_, link2,
                         rect2, &position, label_bounds, font);
  // Draw the third text chunk.
  DrawTextStartingFrom(canvas, main_label_chunk3_, &position, label_bounds,
                       font);

#if defined(GOOGLE_CHROME_BUILD)
  // Insert a line break and some whitespace.
  position.set_width(0);
  position.Enlarge(0, font.height() + kRelatedControlVerticalSpacing);

  // And now the Terms of Service and position the TOS url.
  DrawTextAndPositionUrl(canvas, main_label_chunk4_, terms_of_service_url_,
                         &terms_of_service_url_rect_, &position, label_bounds,
                         font);
  // The last text chunk doesn't have a URL associated with it.
  DrawTextStartingFrom(canvas, main_label_chunk5_, &position, label_bounds,
                       font);

  // Position the TOS URL within the main label.
  terms_of_service_url_->SetBounds(terms_of_service_url_rect_.x(),
                                   terms_of_service_url_rect_.y(),
                                   terms_of_service_url_rect_.width(),
                                   terms_of_service_url_rect_.height());
#endif

  // Position the URLs within the main label. First position the Chromium URL
  // within the main label.
  chromium_url_->SetBounds(chromium_url_rect_.x(),
                           chromium_url_rect_.y(),
                           chromium_url_rect_.width(),
                           chromium_url_rect_.height());
  // Then position the Open Source URL within the main label.
  open_source_url_->SetBounds(open_source_url_rect_.x(),
                              open_source_url_rect_.y(),
                              open_source_url_rect_.width(),
                              open_source_url_rect_.height());

  // Save the height so we can set the bounds correctly.
  main_text_label_height_ = position.height() + font.height();
}

void AboutChromeView::DrawTextAndPositionUrl(ChromeCanvas* canvas,
                                             const std::wstring& text,
                                             views::Link* link,
                                             gfx::Rect* rect,
                                             gfx::Size* position,
                                             const gfx::Rect& bounds,
                                             const ChromeFont& font) {
  DCHECK(canvas && link && rect && position);
  // Draw the text chunk.
  DrawTextStartingFrom(canvas, text, position, bounds, font);

  // And then position the link after it.
  gfx::Size sz = link->GetPreferredSize();
  WrapIfWordDoesntFit(sz.width(), font.height(), position, bounds);
  *rect = gfx::Rect(position->width(), position->height(), sz.width(),
                    sz.height());

  // Going from relative to absolute pixel coordinates again.
  rect->Offset(bounds.x(), bounds.y());
  // And leave some space to draw the link in.
  position->Enlarge(sz.width(), 0);
}

void AboutChromeView::DrawTextStartingFrom(ChromeCanvas* canvas,
                                           const std::wstring& text,
                                           gfx::Size* position,
                                           const gfx::Rect& bounds,
                                           const ChromeFont& font) {
  // Iterate through line breaking opportunities (which in English would be
  // spaces and such. This tells us where to wrap.
  WordIterator iter(text, WordIterator::BREAK_LINE);
  if (!iter.Init())
    return;

  int flags = (UILayoutIsRightToLeft() ?
                   ChromeCanvas::TEXT_ALIGN_RIGHT :
                   ChromeCanvas::TEXT_ALIGN_LEFT) |
              ChromeCanvas::MULTI_LINE |
              ChromeCanvas::HIDE_PREFIX;

  // Iterate over each word in the text, or put in a more locale-neutral way:
  // iterate to the next line breaking opportunity.
  while (iter.Advance()) {
    // Get the word and figure out the dimensions.
    std::wstring word = iter.GetWord();
    int w = font.GetStringWidth(word), h = font.height();
    canvas->SizeStringInt(word, font, &w, &h, flags);

    // If we exceed the boundaries, we need to wrap.
    WrapIfWordDoesntFit(w, font.height(), position, bounds);

    // Draw the word on the screen (mirrored if RTL locale).
    canvas->DrawStringInt(word, font, SK_ColorBLACK,
        main_text_label_->MirroredXCoordinateInsideView(
            position->width() + bounds.x()),
            position->height() + bounds.y(),
            w, h, flags);

    if (word.size() > 0 && word[word.size() - 1] == L'\x0a') {
      // When we come across '\n', we move to the beginning of the next line.
      position->set_width(0);
      position->Enlarge(0, font.height());
    } else {
      // Otherwise, we advance position to the next word.
      position->Enlarge(w, 0);
    }
  }
}

void AboutChromeView::WrapIfWordDoesntFit(int word_width,
                                          int font_height,
                                          gfx::Size* position,
                                          const gfx::Rect& bounds) {
  if (position->width() + word_width > bounds.right()) {
    position->set_width(0);
    position->Enlarge(0, font_height);
  }
}

void AboutChromeView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  // Since we want the some of the controls to show up in the same visual row
  // as the buttons, which are provided by the framework, we must add the
  // buttons to the non-client view, which is the parent of this view.
  // Similarly, when we're removed from the view hierarchy, we must take care
  // to remove these items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(&update_label_);
      parent->AddChildView(throbber_.get());
      parent->AddChildView(&success_indicator_);
      success_indicator_.SetVisible(false);
      parent->AddChildView(&update_available_indicator_);
      update_available_indicator_.SetVisible(false);
      parent->AddChildView(&timeout_indicator_);
      timeout_indicator_.SetVisible(false);

      // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
      // off. So, in this case we just want the About box to not mention
      // on-demand updates. Silent updates (in the background) should still
      // work as before - enabling UAC or installing the latest service pack
      // for Vista is another option.
      int service_pack_major = 0, service_pack_minor = 0;
      win_util::GetServicePackLevel(&service_pack_major, &service_pack_minor);
      if (win_util::UserAccountControlIsEnabled() ||
          win_util::GetWinVersion() == win_util::WINVERSION_XP ||
          (win_util::GetWinVersion() == win_util::WINVERSION_VISTA &&
           service_pack_major >= 1)) {
        UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR);
        google_updater_->CheckForUpdate(false);  // false=don't upgrade yet.
      }
    } else {
      parent->RemoveChildView(&update_label_);
      parent->RemoveChildView(throbber_.get());
      parent->RemoveChildView(&success_indicator_);
      parent->RemoveChildView(&update_available_indicator_);
      parent->RemoveChildView(&timeout_indicator_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::DialogDelegate implementation:

int AboutChromeView::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
}

std::wstring AboutChromeView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_ABOUT_CHROME_UPDATE_CHECK);
  } else if (button == DIALOGBUTTON_CANCEL) {
    // The OK button (which is the default button) has been re-purposed to be
    // 'Check for Updates' so we want the Cancel button should have the label
    // OK but act like a Cancel button in all other ways.
    return l10n_util::GetString(IDS_OK);
  }

  NOTREACHED();
  return L"";
}

bool AboutChromeView::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK && check_button_status_ != CHECKBUTTON_ENABLED)
    return false;

  return true;
}

bool AboutChromeView::IsDialogButtonVisible(DialogButton button) const {
  if (button == DIALOGBUTTON_OK && check_button_status_ == CHECKBUTTON_HIDDEN)
    return false;

  return true;
}

bool AboutChromeView::CanResize() const {
  return false;
}

bool AboutChromeView::CanMaximize() const {
  return false;
}

bool AboutChromeView::IsAlwaysOnTop() const {
  return false;
}

bool AboutChromeView::HasAlwaysOnTopMenu() const {
  return false;
}

bool AboutChromeView::IsModal() const {
  return true;
}

std::wstring AboutChromeView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_ABOUT_CHROME_TITLE);
}

bool AboutChromeView::Accept() {
  UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR);

  // The Upgrade button isn't available until we have received notification
  // that an update is available, at which point this pointer should have been
  // null-ed out.
  DCHECK(!google_updater_);
  google_updater_ = new GoogleUpdate();
  google_updater_->AddStatusChangeListener(this);
  google_updater_->CheckForUpdate(true);  // true=upgrade if new version found.

  return false;  // We never allow this button to close the window.
}

views::View* AboutChromeView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::LinkController implementation:

void AboutChromeView::LinkActivated(views::Link* source,
                                    int event_flags) {
  GURL url;
  if (source == terms_of_service_url_)
    url = GURL(kTOS);
  else if (source == chromium_url_)
    url = GURL(kChromiumUrl);
  else if (source == open_source_url_)
    url = GURL(kAcknowledgements);
  else
    NOTREACHED() << "Unknown link source";

  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(url, GURL(), NEW_WINDOW, PageTransition::LINK);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, GoogleUpdateStatusListener implementation:

void AboutChromeView::OnReportResults(GoogleUpdateUpgradeResult result,
                                      GoogleUpdateErrorCode error_code,
                                      const std::wstring& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  google_updater_ = NULL;

  // Make a note of which version Google Update is reporting is the latest
  // version.
  new_version_available_ = version;
  UpdateStatus(result, error_code);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, private:

void AboutChromeView::UpdateStatus(GoogleUpdateUpgradeResult result,
                                   GoogleUpdateErrorCode error_code) {
  bool show_success_indicator = false;
  bool show_update_available_indicator = false;
  bool show_timeout_indicator = false;
  bool show_throbber = false;
  bool show_update_label = true;  // Always visible, except at start.

  switch (result) {
    case UPGRADE_STARTED:
      UserMetrics::RecordAction(L"Upgrade_Started", profile_);
      check_button_status_ = CHECKBUTTON_DISABLED;
      show_throbber = true;
      update_label_.SetText(l10n_util::GetString(IDS_UPGRADE_STARTED));
      break;
    case UPGRADE_CHECK_STARTED:
      UserMetrics::RecordAction(L"UpgradeCheck_Started", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      show_throbber = true;
      update_label_.SetText(l10n_util::GetString(IDS_UPGRADE_CHECK_STARTED));
      break;
    case UPGRADE_IS_AVAILABLE:
      UserMetrics::RecordAction(L"UpgradeCheck_UpgradeIsAvailable", profile_);
      check_button_status_ = CHECKBUTTON_ENABLED;
      update_label_.SetText(
          l10n_util::GetStringF(IDS_UPGRADE_AVAILABLE,
                                l10n_util::GetString(IDS_PRODUCT_NAME)));
      show_update_available_indicator = true;
      break;
    case UPGRADE_ALREADY_UP_TO_DATE: {
      // Google Update reported that Chrome is up-to-date. Now make sure that we
      // are running the latest version and if not, notify the user by falling
      // into the next case of UPGRADE_SUCCESSFUL.
      scoped_ptr<installer::Version> installed_version(
          InstallUtil::GetChromeVersion(false));
      scoped_ptr<installer::Version> running_version(
          installer::Version::GetVersionFromString(current_version_));
      if (!installed_version.get() ||
          !installed_version->IsHigherThan(running_version.get())) {
        UserMetrics::RecordAction(L"UpgradeCheck_AlreadyUpToDate", profile_);
        check_button_status_ = CHECKBUTTON_HIDDEN;
        update_label_.SetText(
            l10n_util::GetStringF(IDS_UPGRADE_ALREADY_UP_TO_DATE,
                                  l10n_util::GetString(IDS_PRODUCT_NAME),
                                  current_version_));
        show_success_indicator = true;
        break;
      }
      // No break here as we want to notify user about upgrade if there is one.
    }
    case UPGRADE_SUCCESSFUL: {
      if (result == UPGRADE_ALREADY_UP_TO_DATE)
        UserMetrics::RecordAction(L"UpgradeCheck_AlreadyUpgraded", profile_);
      else
        UserMetrics::RecordAction(L"UpgradeCheck_Upgraded", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      const std::wstring& update_string = new_version_available_.empty()
          ? l10n_util::GetStringF(IDS_UPGRADE_SUCCESSFUL_NOVERSION,
                                  l10n_util::GetString(IDS_PRODUCT_NAME))
          : l10n_util::GetStringF(IDS_UPGRADE_SUCCESSFUL,
                                  l10n_util::GetString(IDS_PRODUCT_NAME),
                                  new_version_available_);
      update_label_.SetText(update_string);
      show_success_indicator = true;
      RestartMessageBox::ShowMessageBox(window()->GetHWND());
      break;
    }
    case UPGRADE_ERROR:
      UserMetrics::RecordAction(L"UpgradeCheck_Error", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      update_label_.SetText(l10n_util::GetStringF(IDS_UPGRADE_ERROR,
                                                  IntToWString(error_code)));
      show_timeout_indicator = true;
      break;
    default:
      NOTREACHED();
  }

  success_indicator_.SetVisible(show_success_indicator);
  update_available_indicator_.SetVisible(show_update_available_indicator);
  timeout_indicator_.SetVisible(show_timeout_indicator);
  update_label_.SetVisible(show_update_label);
  throbber_->SetVisible(show_throbber);
  if (show_throbber)
    throbber_->Start();
  else
    throbber_->Stop();

  // We have updated controls on the parent, so we need to update its layout.
  View* parent = GetParent();
  parent->Layout();

  // Check button may have appeared/disappeared. We cannot call this during
  // ViewHierarchyChanged because the |window()| pointer hasn't been set yet.
  if (window())
    GetDialogClientView()->UpdateDialogButtons();
}
