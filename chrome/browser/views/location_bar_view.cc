// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/views/location_bar_view.h"

#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/first_run_bubble.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/win_util.h"
#include "chrome/views/background.h"
#include "chrome/views/border.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_container.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "generated_resources.h"

using ChromeViews::View;

const int LocationBarView::kTextVertMargin = 2;

const COLORREF LocationBarView::kBackgroundColorByLevel[] = {
  RGB(255, 245, 195),  // SecurityLevel SECURE: Yellow.
  RGB(255, 255, 255),  // SecurityLevel NORMAL: White.
  RGB(255, 255, 255),  // SecurityLevel INSECURE: White.
};

// The margins around the solid color we draw.
static const int kBackgroundVertMargin = 2;
static const int kBackgroundHoriMargin = 0;

// Padding on the right and left of the entry field.
static const int kEntryPadding = 3;

// Padding between the entry and the leading/trailing views.
static const int kInnerPadding = 3;

static const SkBitmap* kBackground = NULL;

static const SkBitmap* kPopupBackgroundLeft = NULL;
static const SkBitmap* kPopupBackgroundCenter = NULL;
static const SkBitmap* kPopupBackgroundRight = NULL;
static const int kPopupBackgroundVertMargin = 2;
static const int kPopupBackgroundHorzMargin = 2;

// The delay the mouse has to be hovering over the lock/warning icon before the
// info bubble is shown.
static const int kInfoBubbleHoverDelayMs = 500;

// The tab key image.
static const SkBitmap* kTabButtonBitmap = NULL;

// Returns the description for a keyword.
static std::wstring GetKeywordDescription(Profile* profile,
                                          const std::wstring& keyword) {
  // Make sure the TemplateURL still exists.
  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  return template_url ? template_url->short_name() : std::wstring();
}

LocationBarView::LocationBarView(Profile* profile,
                                 CommandController* controller,
                                 ToolbarModel* model,
                                 Delegate* delegate,
                                 bool popup_window_mode)
    : profile_(profile),
      controller_(controller),
      model_(model),
      delegate_(delegate),
      disposition_(CURRENT_TAB),
      location_entry_view_(NULL),
      selected_keyword_view_(profile),
      keyword_hint_view_(profile),
      type_to_search_view_(l10n_util::GetString(IDS_OMNIBOX_EMPTY_TEXT)),
      security_image_view_(profile, model),
      popup_window_mode_(popup_window_mode),
      first_run_bubble_(this) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (!kBackground) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    kBackground = rb.GetBitmapNamed(IDR_LOCATIONBG);
    kPopupBackgroundLeft =
      rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_LEFT);
    kPopupBackgroundCenter =
      rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_CENTER);
    kPopupBackgroundRight =
      rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_RIGHT);
  }
}

bool LocationBarView::IsInitialized() const {
  return location_entry_view_ != NULL;
}

void LocationBarView::Init() {
  if (popup_window_mode_) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = font_.DeriveFont(3);
  }

  // URL edit field.
  ChromeViews::ViewContainer* vc = GetViewContainer();
  DCHECK(vc) << "LocationBarView::Init - vc is NULL!";
  location_entry_.reset(new AutocompleteEdit(font_, this, model_, this,
                                             vc->GetHWND(),
                                             profile_, controller_,
                                             popup_window_mode_));

  // View container for URL edit field.
  location_entry_view_ = new ChromeViews::HWNDView;
  DCHECK(location_entry_view_) << "LocationBarView::Init - OOM!";
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  AddChildView(location_entry_view_);
  location_entry_view_->SetAssociatedFocusView(this);
  location_entry_view_->Attach(location_entry_->m_hWnd);

  AddChildView(&selected_keyword_view_);
  selected_keyword_view_.SetFont(font_);
  selected_keyword_view_.SetVisible(false);
  selected_keyword_view_.SetParentOwned(false);

  DWORD sys_color = GetSysColor(COLOR_GRAYTEXT);
  SkColor gray = SkColorSetRGB(GetRValue(sys_color), GetGValue(sys_color),
                               GetBValue(sys_color));

  AddChildView(&type_to_search_view_);
  type_to_search_view_.SetVisible(false);
  type_to_search_view_.SetFont(font_);
  type_to_search_view_.SetColor(gray);
  type_to_search_view_.SetParentOwned(false);

  AddChildView(&keyword_hint_view_);
  keyword_hint_view_.SetVisible(false);
  keyword_hint_view_.SetFont(font_);
  keyword_hint_view_.SetColor(gray);
  keyword_hint_view_.SetParentOwned(false);

  AddChildView(&security_image_view_);
  security_image_view_.SetVisible(false);
  security_image_view_.SetParentOwned(false);

  AddChildView(&info_label_);
  info_label_.SetVisible(false);
  info_label_.SetParentOwned(false);

  // Notify us when any ancestor is resized.  In this case we want to tell the
  // AutocompleteEdit to close its popup.
  SetNotifyWhenVisibleBoundsInRootChanges(true);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);

  OnChanged();
}

void LocationBarView::Update(const TabContents* tab_for_state_restoring) {
  SetSecurityIcon(model_->GetIcon());
  std::wstring info_text, info_tooltip;
  SkColor text_color;
  model_->GetInfoText(&info_text, &text_color, &info_tooltip);
  SetInfoText(info_text, text_color, info_tooltip);
  location_entry_->Update(tab_for_state_restoring);
  Layout();
  SchedulePaint();
}

void LocationBarView::Focus() {
  ::SetFocus(location_entry_->m_hWnd);
}

void LocationBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ != profile) {
    profile_ = profile;
    location_entry_->SetProfile(profile);
    selected_keyword_view_.set_profile(profile);
    keyword_hint_view_.set_profile(profile);
    security_image_view_.set_profile(profile);
  }
}

void LocationBarView::GetPreferredSize(CSize *out) {
  CSize size;
  security_image_view_.GetPreferredSize(&size);
  out->cx = 0;

  out->cy = std::max(
      (popup_window_mode_ ? kPopupBackgroundCenter : kBackground)->height(),
      static_cast<int>(size.cy));
}

void LocationBarView::DidChangeBounds(const CRect& previous,
                                      const CRect& current) {
  Layout();
}

void LocationBarView::Layout() {
  DoLayout(true);
}

void LocationBarView::Paint(ChromeCanvas* canvas) {
  View::Paint(canvas);

  SkColor bg = SkColorSetRGB(
      GetRValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]),
      GetGValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]),
      GetBValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]));

  if (popup_window_mode_ == false) {
    int bh = kBackground->height();

    canvas->TileImageInt(*kBackground, 0, (GetHeight() - bh) / 2, GetWidth(),
                         bh);

    canvas->FillRectInt(bg, kBackgroundHoriMargin, kBackgroundVertMargin,
                        GetWidth() - 2 * kBackgroundHoriMargin,
                        bh - kBackgroundVertMargin * 2);
  } else {
    canvas->TileImageInt(*kPopupBackgroundLeft, 0, 0,
                         kPopupBackgroundLeft->width(),
                         kPopupBackgroundLeft->height());
    canvas->TileImageInt(*kPopupBackgroundCenter,
                         kPopupBackgroundLeft->width(), 0,
                         GetWidth() -
                            kPopupBackgroundLeft->width() -
                            kPopupBackgroundRight->width(),
                         kPopupBackgroundCenter->height());
    canvas->TileImageInt(*kPopupBackgroundRight,
                         GetWidth() - kPopupBackgroundRight->width(),
                         0, kPopupBackgroundRight->width(),
                         kPopupBackgroundRight->height());

    canvas->FillRectInt(bg, kPopupBackgroundHorzMargin,
                        kPopupBackgroundVertMargin,
                        GetWidth() - kPopupBackgroundHorzMargin * 2,
                        kPopupBackgroundCenter->height() -
                        kPopupBackgroundVertMargin * 2);
  }
}

bool LocationBarView::CanProcessTabKeyEvents() {
  // We want to receive tab key events when the hint is showing.
  return keyword_hint_view_.IsVisible();
}

void LocationBarView::VisibleBoundsInRootChanged() {
  location_entry_->ClosePopup();
}

bool LocationBarView::OnMousePressed(const ChromeViews::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = (event.GetFlags() & ChromeViews::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
  } else if (event.IsMiddleMouseButton()) {
    msg = (event.GetFlags() & ChromeViews::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_MBUTTONDBLCLK : WM_MBUTTONDOWN;
  } else if (event.IsRightMouseButton()) {
    msg = (event.GetFlags() & ChromeViews::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_RBUTTONDBLCLK : WM_RBUTTONDOWN;
  } else {
    NOTREACHED();
    return false;
  }
  OnMouseEvent(event, msg);
  return true;
}

bool LocationBarView::OnMouseDragged(const ChromeViews::MouseEvent& event) {
  OnMouseEvent(event, WM_MOUSEMOVE);
  return true;
}

void LocationBarView::OnMouseReleased(const ChromeViews::MouseEvent& event,
                                      bool canceled) {
  UINT msg;
  if (canceled) {
    msg = WM_CAPTURECHANGED;
  } else if (event.IsLeftMouseButton()) {
    msg = WM_LBUTTONUP;
  } else if (event.IsMiddleMouseButton()) {
    msg = WM_MBUTTONUP;
  } else if (event.IsRightMouseButton()) {
    msg = WM_RBUTTONUP;
  } else {
    NOTREACHED();
    return;
  }
  OnMouseEvent(event, msg);
}

void LocationBarView::OnAutocompleteAccept(
    const std::wstring& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const std::wstring& alternate_nav_url) {
  if (url.empty())
    return;

  location_input_ = url;
  disposition_ = disposition;
  transition_ = transition;

  if (controller_) {
    if (alternate_nav_url.empty()) {
      controller_->ExecuteCommand(IDC_OPENURL);
      return;
    }

    scoped_ptr<AlternateNavURLFetcher> fetcher(
        new AlternateNavURLFetcher(alternate_nav_url));
    // The AlternateNavURLFetcher will listen for the next navigation state
    // update notification (expecting it to be a new page load) and hook
    // itself in to that loading process.
    controller_->ExecuteCommand(IDC_OPENURL);
    if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
      // I'm not sure this should be reachable, but I'm not also sure enough
      // that it shouldn't to stick in a NOTREACHED().  In any case, this is
      // harmless; we can simply let the fetcher get deleted here and it will
      // clean itself up properly.
    } else {
      fetcher.release();  // The navigation controller will delete the fetcher.
    }
  }
}

void LocationBarView::OnChanged() {
  DoLayout(false);
}

SkBitmap LocationBarView::GetFavIcon() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return delegate_->GetTabContents()->GetFavIcon();
}

std::wstring LocationBarView::GetTitle() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return delegate_->GetTabContents()->GetTitle();
}

void LocationBarView::DoLayout(const bool force_layout) {
  if (!location_entry_.get())
    return;

  RECT formatting_rect;
  location_entry_->GetRect(&formatting_rect);
  RECT edit_bounds;
  location_entry_->GetClientRect(&edit_bounds);

  int entry_width = GetWidth() - kEntryPadding - kEntryPadding;
  CSize security_image_size;
  if (security_image_view_.IsVisible()) {
    security_image_view_.GetPreferredSize(&security_image_size);
    entry_width -= security_image_size.cx;
  }
  CSize info_label_size;
  if (info_label_.IsVisible()) {
    info_label_.GetPreferredSize(&info_label_size);
    entry_width -= (info_label_size.cx + kInnerPadding);
  }

  const int max_edit_width = entry_width - formatting_rect.left -
                             (edit_bounds.right - formatting_rect.right);
  if (max_edit_width < 0)
    return;
  const int text_width = TextDisplayWidth();
  bool needs_layout = force_layout;
  needs_layout |= AdjustHints(text_width, max_edit_width);

  if (!needs_layout)
    return;

  // TODO(sky): baseline layout.
  const SkBitmap* background = popup_window_mode_ ? kPopupBackgroundCenter
                                                  : kBackground;
  int bh = background->height();
  int location_y = ((GetHeight() - bh) / 2) + kTextVertMargin;
  int location_height = bh - (2 * kTextVertMargin);
  if (info_label_.IsVisible()) {
    info_label_.SetBounds(GetWidth() - kEntryPadding - info_label_size.cx,
                          location_y,
                          info_label_size.cx, location_height);
  }
  if (security_image_view_.IsVisible()) {
    const int info_label_width = info_label_size.cx ?
        info_label_size.cx + kInnerPadding : 0;
    security_image_view_.SetBounds(GetWidth() - kEntryPadding -
                                      info_label_width  -
                                      security_image_size.cx,
                                   location_y,
                                   security_image_size.cx, location_height);
  }
  gfx::Rect location_bounds(kEntryPadding, location_y, entry_width,
                            location_height);
  if (selected_keyword_view_.IsVisible()) {
    LayoutView(true, &selected_keyword_view_, text_width, max_edit_width,
               &location_bounds);
  } else if (keyword_hint_view_.IsVisible()) {
    LayoutView(false, &keyword_hint_view_, text_width, max_edit_width,
               &location_bounds);
  } else if (type_to_search_view_.IsVisible()) {
    LayoutView(false, &type_to_search_view_, text_width, max_edit_width,
               &location_bounds);
  }

  location_entry_view_->SetBounds(location_bounds.x(),
                                  location_bounds.y(),
                                  location_bounds.width(),
                                  location_bounds.height());
  if (!force_layout) {
    // If force_layout is false and we got this far it means one of the views
    // was added/removed or changed in size. We need to paint ourselves.
    SchedulePaint();
  }
}

int LocationBarView::TextDisplayWidth() {
  POINT last_char_position =
      location_entry_->PosFromChar(location_entry_->GetTextLength());
  POINT scroll_position;
  location_entry_->GetScrollPos(&scroll_position);
  const int position_x = last_char_position.x + scroll_position.x;
  return UILayoutIsRightToLeft() ? GetWidth() - position_x : position_x;
}

bool LocationBarView::UsePref(int pref_width, int text_width, int max_width) {
  return (pref_width + kInnerPadding + text_width <= max_width);
}

bool LocationBarView::NeedsResize(View* view, int text_width, int max_width) {
  CSize size;
  view->GetPreferredSize(&size);
  if (!UsePref(size.cx, text_width, max_width))
    view->GetMinimumSize(&size);
  return (view->GetWidth() != size.cx);
}

bool LocationBarView::AdjustHints(int text_width, int max_width) {
  const std::wstring keyword(location_entry_->keyword());
  const bool is_keyword_hint(location_entry_->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;
  bool show_search_hint(location_entry_->show_search_hint());
  DCHECK(keyword.empty() || !show_search_hint);

  if (show_search_hint) {
    // Only show type to search if all the text fits.
    CSize view_pref;
    type_to_search_view_.GetPreferredSize(&view_pref);
    show_search_hint = UsePref(view_pref.cx, text_width, max_width);
  }

  // NOTE: This isn't just one big || statement as ToggleVisibility MUST be
  // invoked for each view.
  bool needs_layout = false;
  needs_layout |= ToggleVisibility(show_selected_keyword,
                                   &selected_keyword_view_);
  needs_layout |= ToggleVisibility(show_keyword_hint, &keyword_hint_view_);
  needs_layout |= ToggleVisibility(show_search_hint, &type_to_search_view_);
  if (show_selected_keyword) {
    if (selected_keyword_view_.keyword() != keyword) {
      needs_layout = true;
      selected_keyword_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&selected_keyword_view_, text_width, max_width);
  } else if (show_keyword_hint) {
    if (keyword_hint_view_.keyword() != keyword) {
      needs_layout = true;
      keyword_hint_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&keyword_hint_view_, text_width, max_width);
  }

  return needs_layout;
}

void LocationBarView::LayoutView(bool leading, ChromeViews::View* view,
                                 int text_width, int max_width,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  CSize view_size(0, 0);
  view->GetPreferredSize(&view_size);
  if (!UsePref(view_size.cx, text_width, max_width))
    view->GetMinimumSize(&view_size);
  if (view_size.cx + kInnerPadding < bounds->width()) {
    view->SetVisible(true);
    if (leading) {
      view->SetBounds(bounds->x(), bounds->y(), view_size.cx, bounds->height());
      bounds->Offset(view_size.cx + kInnerPadding, 0);
    } else {
      view->SetBounds(bounds->right() - view_size.cx, bounds->y(),
                      view_size.cx, bounds->height());
    }
    bounds->set_width(bounds->width() - view_size.cx - kInnerPadding);
  } else {
    view->SetVisible(false);
  }
}

void LocationBarView::SetSecurityIcon(ToolbarModel::Icon icon) {
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      security_image_view_.SetImageShown(SecurityImageView::LOCK);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::WARNING_ICON:
      security_image_view_.SetImageShown(SecurityImageView::WARNING);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::NO_ICON:
      security_image_view_.SetVisible(false);
      break;
    default:
      NOTREACHED();
      security_image_view_.SetVisible(false);
      break;
  }
}

void LocationBarView::SetInfoText(const std::wstring& text,
                                  SkColor text_color,
                                  const std::wstring& tooltip_text) {
  info_label_.SetVisible(!text.empty());
  info_label_.SetText(text);
  info_label_.SetColor(text_color);
  info_label_.SetTooltipText(tooltip_text);
}

bool LocationBarView::ToggleVisibility(bool new_vis, View* view) {
  DCHECK(view);
  if (view->IsVisible() != new_vis) {
    view->SetVisible(new_vis);
    return true;
  }
  return false;
}

void LocationBarView::OnMouseEvent(const ChromeViews::MouseEvent& event,
                                   UINT msg) {
  UINT flags = 0;
  if (event.IsControlDown())
    flags |= MK_CONTROL;
  if (event.IsShiftDown())
    flags |= MK_SHIFT;
  if (event.IsLeftMouseButton())
    flags |= MK_LBUTTON;
  if (event.IsMiddleMouseButton())
    flags |= MK_MBUTTON;
  if (event.IsRightMouseButton())
    flags |= MK_RBUTTON;

  CPoint screen_point(event.GetX(), event.GetY());
  ConvertPointToScreen(this, &screen_point);

  location_entry_->HandleExternalMsg(msg, flags, screen_point);
}

bool LocationBarView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_GROUPING;
  return true;
}

// SelectedKeywordView -------------------------------------------------------

// The background is drawn using ImagePainter3. This is the left/center/right
// image names.
static const int kBorderImages[] = {
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_L,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_C,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_R };

// Insets around the label.
static const int kTopInset = 0;
static const int kBottomInset = 0;
static const int kLeftInset = 4;
static const int kRightInset = 4;

// Offset from the top the background is drawn at.
static const int kBackgroundYOffset = 2;

LocationBarView::SelectedKeywordView::SelectedKeywordView(Profile* profile)
    : background_painter_(kBorderImages),
      profile_(profile) {
  AddChildView(&full_label_);
  AddChildView(&partial_label_);
  // Full_label and partial_label are deleted by us, make sure View doesn't
  // delete them too.
  full_label_.SetParentOwned(false);
  partial_label_.SetParentOwned(false);
  full_label_.SetVisible(false);
  partial_label_.SetVisible(false);
  full_label_.SetBorder(
      ChromeViews::Border::CreateEmptyBorder(kTopInset, kLeftInset,
                                             kBottomInset, kRightInset));
  partial_label_.SetBorder(
      ChromeViews::Border::CreateEmptyBorder(kTopInset, kLeftInset,
                                             kBottomInset, kRightInset));
}

LocationBarView::SelectedKeywordView::~SelectedKeywordView() {
}

void LocationBarView::SelectedKeywordView::SetFont(const ChromeFont& font) {
  full_label_.SetFont(font);
  partial_label_.SetFont(font);
}

void LocationBarView::SelectedKeywordView::Paint(ChromeCanvas* canvas) {
  canvas->TranslateInt(0, kBackgroundYOffset);
  background_painter_.Paint(GetWidth(), GetHeight() - kTopInset, canvas);
  canvas->TranslateInt(0, -kBackgroundYOffset);
}

void LocationBarView::SelectedKeywordView::GetPreferredSize(CSize* size) {
  full_label_.GetPreferredSize(size);
}

void LocationBarView::SelectedKeywordView::GetMinimumSize(CSize* size) {
  partial_label_.GetMinimumSize(size);
}

void LocationBarView::SelectedKeywordView::DidChangeBounds(
    const CRect& previous,
    const CRect& current) {
  Layout();
}

void LocationBarView::SelectedKeywordView::Layout() {
  CSize pref;
  GetPreferredSize(&pref);
  bool at_pref = (GetWidth() == pref.cx);
  if (at_pref)
    full_label_.SetBounds(0, 0, GetWidth(), GetHeight());
  else
    partial_label_.SetBounds(0, 0, GetWidth(), GetHeight());
  full_label_.SetVisible(at_pref);
  partial_label_.SetVisible(!at_pref);
}

void LocationBarView::SelectedKeywordView::SetKeyword(
    const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring description = GetKeywordDescription(profile_, keyword);
  full_label_.SetText(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                            description));
  const std::wstring min_string = CalculateMinString(description);
  if (!min_string.empty()) {
    partial_label_.SetText(
        l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, min_string));
  } else {
    partial_label_.SetText(full_label_.GetText());
  }
}

std::wstring LocationBarView::SelectedKeywordView::CalculateMinString(
    const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    return l10n_util::TruncateString(description, 3);
  }
  return description.substr(0, chop_index);
}

// KeywordHintView -------------------------------------------------------------

// Amount of space to offset the tab image from the top of the view by.
static const int kTabImageYOffset = 4;

LocationBarView::KeywordHintView::KeywordHintView(Profile* profile)
    : profile_(profile) {
  AddChildView(&leading_label_);
  AddChildView(&trailing_label_);

  if (!kTabButtonBitmap) {
    kTabButtonBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
  }
}

LocationBarView::KeywordHintView::~KeywordHintView() {
  // Labels are freed by us. Remove them so that View doesn't
  // try to free them too.
  RemoveChildView(&leading_label_);
  RemoveChildView(&trailing_label_);
}

void LocationBarView::KeywordHintView::SetFont(const ChromeFont& font) {
  leading_label_.SetFont(font);
  trailing_label_.SetFont(font);
}

void LocationBarView::KeywordHintView::SetColor(const SkColor& color) {
  leading_label_.SetColor(color);
  trailing_label_.SetColor(color);
}

void LocationBarView::KeywordHintView::SetKeyword(const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  const std::wstring keyword_hint(l10n_util::GetStringF(
      IDS_OMNIBOX_KEYWORD_HINT, std::wstring(),
      GetKeywordDescription(profile_, keyword), &content_param_offsets));
  if (content_param_offsets.size() == 2) {
    leading_label_.SetText(keyword_hint.substr(0,
                                               content_param_offsets.front()));
    trailing_label_.SetText(keyword_hint.substr(content_param_offsets.front()));
  } else {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
  }
}

void LocationBarView::KeywordHintView::Paint(ChromeCanvas* canvas) {
  int image_x = leading_label_.IsVisible() ? leading_label_.GetWidth() : 0;

  // Since we paint the button image directly on the canvas (instead of using a
  // child view), we must mirror the button's position manually if the locale
  // is right-to-left.
  gfx::Rect tab_button_bounds(image_x,
                              kTabImageYOffset,
                              kTabButtonBitmap->width(),
                              kTabButtonBitmap->height());
  tab_button_bounds.set_x(MirroredLeftPointForRect(tab_button_bounds));
  canvas->DrawBitmapInt(*kTabButtonBitmap,
                        tab_button_bounds.x(),
                        tab_button_bounds.y());
}

void LocationBarView::KeywordHintView::GetPreferredSize(CSize *out) {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  leading_label_.GetPreferredSize(out);
  int width = out->cx;
  width += kTabButtonBitmap->width();
  trailing_label_.GetPreferredSize(out);
  width += out->cx;
  out->cx = width;
}

void LocationBarView::KeywordHintView::GetMinimumSize(CSize* out) {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  out->cx = kTabButtonBitmap->width();
}

void LocationBarView::KeywordHintView::Layout() {
  // TODO(sky): baseline layout.
  bool show_labels = (GetWidth() != kTabButtonBitmap->width());

  leading_label_.SetVisible(show_labels);
  trailing_label_.SetVisible(show_labels);
  int height = GetHeight();
  int x = 0;
  CSize pref;

  if (show_labels) {
    leading_label_.GetPreferredSize(&pref);
    leading_label_.SetBounds(x, 0, pref.cx, height);

    x += pref.cx + kTabButtonBitmap->width();
    trailing_label_.GetPreferredSize(&pref);
    trailing_label_.SetBounds(x, 0, pref.cx, height);
  }
}

void LocationBarView::KeywordHintView::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

// We don't translate accelerators for ALT + numpad digit, they are used for
// entering special characters.
bool LocationBarView::ShouldLookupAccelerators(const ChromeViews::KeyEvent& e) {
  if (!e.IsAltDown())
    return true;

  return !win_util::IsNumPadDigit(e.GetCharacter(), e.IsExtendedKey());
}

// ShowInfoBubbleTask-----------------------------------------------------------

class LocationBarView::ShowInfoBubbleTask : public Task {
 public:
  explicit ShowInfoBubbleTask(LocationBarView::SecurityImageView* image_view);
  virtual void Run();
  void Cancel();

 private:
  LocationBarView::SecurityImageView* image_view_;
  bool cancelled_;

  DISALLOW_EVIL_CONSTRUCTORS(ShowInfoBubbleTask);
};

LocationBarView::ShowInfoBubbleTask::ShowInfoBubbleTask(
    LocationBarView::SecurityImageView* image_view)
    : cancelled_(false),
      image_view_(image_view) {
}

void LocationBarView::ShowInfoBubbleTask::Run() {
  if (cancelled_)
    return;

  if (!image_view_->GetViewContainer()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.  Also makes sure we NULL
    // show_info_bubble_task_ to prevent the SecurityImageView from keeping a
    // dangling pointer.
    image_view_->show_info_bubble_task_ = NULL;
    return;
  }

  image_view_->ShowInfoBubble();
}

void LocationBarView::ShowInfoBubbleTask::Cancel() {
  cancelled_ = true;
}

// -----------------------------------------------------------------------------

void LocationBarView::ShowFirstRunBubbleInternal() {
  if (!location_entry_view_)
    return;
  if (!location_entry_view_->GetViewContainer()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.
    return;
  }

  CPoint location(0, 0);

  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (UILayoutIsRightToLeft())
    location.x += GetWidth();
  ChromeViews::View::ConvertPointToScreen(this, &location);

  // We try to guess that 20 pixels offset is a good place for the first
  // letter in the OmniBox.
  gfx::Rect bounds(location.x, location.y, 20, GetHeight());

  // Moving the bounds "backwards" so that it appears within the location bar
  // if the UI layout is RTL.
  if (UILayoutIsRightToLeft())
    bounds.set_x(location.x - 20);

  FirstRunBubble::Show(
      location_entry_view_->GetRootView()->GetViewContainer()->GetHWND(),
      bounds);
}

void LocationBarView::ShowFirstRunBubble() {
  // We wait 30 milliseconds to open. It allows less flicker.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarView::ShowFirstRunBubbleInternal);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 30);
}

// SecurityImageView------------------------------------------------------------

// static
SkBitmap* LocationBarView::SecurityImageView::lock_icon_ = NULL;
SkBitmap* LocationBarView::SecurityImageView::warning_icon_ = NULL;

LocationBarView::SecurityImageView::SecurityImageView(Profile* profile,
                                                      ToolbarModel* model)
    : profile_(profile),
      model_(model),
      show_info_bubble_task_(NULL),
      info_bubble_(NULL) {
  if (!lock_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    lock_icon_ = rb.GetBitmapNamed(IDR_LOCK);
    warning_icon_ = rb.GetBitmapNamed(IDR_WARNING);
  }
  SetImageShown(LOCK);
}

LocationBarView::SecurityImageView::~SecurityImageView() {
  if (show_info_bubble_task_)
    show_info_bubble_task_->Cancel();

  if (info_bubble_) {
    // We are going to be invalid, make sure the InfoBubble does not keep a
    // pointer to us.
    info_bubble_->SetDelegate(NULL);
  }
}

void LocationBarView::SecurityImageView::SetImageShown(Image image) {
  switch (image) {
    case LOCK:
      ImageView::SetImage(lock_icon_);
      break;
    case WARNING:
      ImageView::SetImage(warning_icon_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LocationBarView::SecurityImageView::ShowInfoBubble() {
  std::wstring text;
  SkColor text_color;
  model_->GetIconHoverText(&text, &text_color);

  CPoint location(0, 0);
  ChromeViews::View::ConvertPointToScreen(this, &location);
  gfx::Rect bounds(location.x, location.y, GetWidth(), GetHeight());

  ChromeViews::Label* label = new ChromeViews::Label(text);
  label->SetMultiLine(true);
  label->SetColor(text_color);
  label->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(2));
  label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  label->SizeToFit(0);
  DCHECK(info_bubble_ == NULL);
  info_bubble_ = InfoBubble::Show(GetRootView()->GetViewContainer()->GetHWND(),
                                  bounds, label, this);
  show_info_bubble_task_ = NULL;
}

void LocationBarView::SecurityImageView::OnMouseMoved(
    const ChromeViews::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_) {
    // If an info bubble is currently showing, nothing to do.
    return;
  }

  show_info_bubble_task_ = new ShowInfoBubbleTask(this);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, show_info_bubble_task_,
                                          kInfoBubbleHoverDelayMs);
}

void LocationBarView::SecurityImageView::OnMouseExited(
    const ChromeViews::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_)
    info_bubble_->Close();
}

bool LocationBarView::SecurityImageView::OnMousePressed(
    const ChromeViews::MouseEvent& event) {
  NavigationEntry* nav_entry =
      BrowserList::GetLastActive()->GetSelectedTabContents()->
          controller()->GetActiveEntry();
  PageInfoWindow::Create(profile_,
                         nav_entry,
                         GetRootView()->GetViewContainer()->GetHWND(),
                         PageInfoWindow::SECURITY);
  return true;
}

void LocationBarView::SecurityImageView::InfoBubbleClosing(
    InfoBubble* info_bubble) {
  info_bubble_ = NULL;
}

bool LocationBarView::OverrideAccelerator(
    const ChromeViews::Accelerator& accelerator)  {
  return location_entry_->OverrideAccelerator(accelerator);
}
