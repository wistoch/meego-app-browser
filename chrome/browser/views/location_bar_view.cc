// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar_view.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "app/drag_drop_types.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/i18n/rtl.h"
#include "base/stl_util-inl.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/content_blocked_bubble_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/platform_util.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/drag_utils.h"

#if defined(OS_WIN)
#include "chrome/browser/views/first_run_bubble.h"
#endif

using views::View;

// static
const int LocationBarView::kVertMargin = 2;

// Padding between items in the location bar.
static const int kViewPadding = 3;

// Padding before the start of a bubble.
static const int kBubblePadding = kViewPadding - 1;

// Padding between the location icon and the edit, if they're adjacent.
static const int kLocationIconEditPadding = kViewPadding - 1;

static const int kEVBubbleBackgroundImages[] = {
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_L,
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_C,
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_R,
};

static const int kSelectedKeywordBackgroundImages[] = {
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_L,
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_C,
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_R,
};

static const SkBitmap* kBackground = NULL;

static const SkBitmap* kPopupBackground = NULL;

// The tab key image.
static const SkBitmap* kTabButtonBitmap = NULL;

// Returns the short name for a keyword.
static std::wstring GetKeywordName(Profile* profile,
                                   const std::wstring& keyword) {
  // Make sure the TemplateURL still exists.
  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url)
    return template_url->AdjustedShortNameForLocaleDirection();
  return std::wstring();
}


// PageActionWithBadgeView ----------------------------------------------------

// A container for the PageActionImageView plus its badge.
class LocationBarView::PageActionWithBadgeView : public views::View {
 public:
  explicit PageActionWithBadgeView(PageActionImageView* image_view);

  PageActionImageView* image_view() { return image_view_; }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(Extension::kPageActionIconMaxSize,
                     Extension::kPageActionIconMaxSize);
  }

  void UpdateVisibility(TabContents* contents, const GURL& url);

 private:
  virtual void Layout();

  // The button this view contains.
  PageActionImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(PageActionWithBadgeView);
};

LocationBarView::PageActionWithBadgeView::PageActionWithBadgeView(
    PageActionImageView* image_view) {
  image_view_ = image_view;
  AddChildView(image_view_);
}

void LocationBarView::PageActionWithBadgeView::Layout() {
  // We have 25 pixels of vertical space in the Omnibox to play with, so even
  // sized icons (such as 16x16) have either a 5 or a 4 pixel whitespace
  // (padding) above and below. It looks better to have the extra pixel above
  // the icon than below it, so we add a pixel. http://crbug.com/25708.
  const SkBitmap& image = image_view()->GetImage();
  int y = (image.height() + 1) % 2;  // Even numbers: 1px padding. Odd: 0px.
  image_view_->SetBounds(0, y, width(), height());
}

void LocationBarView::PageActionWithBadgeView::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  image_view_->UpdateVisibility(contents, url);
  SetVisible(image_view_->IsVisible());
}


// LocationBarView -----------------------------------------------------------

LocationBarView::LocationBarView(Profile* profile,
                                 CommandUpdater* command_updater,
                                 ToolbarModel* model,
                                 Delegate* delegate,
                                 bool popup_window_mode)
    : profile_(profile),
      command_updater_(command_updater),
      model_(model),
      delegate_(delegate),
      disposition_(CURRENT_TAB),
      ALLOW_THIS_IN_INITIALIZER_LIST(location_icon_view_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ev_bubble_view_(
          kEVBubbleBackgroundImages, IDR_OMNIBOX_HTTPS_VALID,
          GetColor(ToolbarModel::EV_SECURE, SECURITY_TEXT), this)),
      location_entry_view_(NULL),
      selected_keyword_view_(kSelectedKeywordBackgroundImages,
                             IDR_OMNIBOX_SEARCH, SK_ColorBLACK, profile),
      keyword_hint_view_(profile),
      star_view_(command_updater),
      popup_window_mode_(popup_window_mode),
      ALLOW_THIS_IN_INITIALIZER_LIST(first_run_bubble_(this)) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (!kBackground) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    kBackground = rb.GetBitmapNamed(IDR_LOCATIONBG);
    kPopupBackground = rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_CENTER);
  }
}

LocationBarView::~LocationBarView() {
}

void LocationBarView::Init() {
  if (popup_window_mode_) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = font_.DeriveFont(3);
  }

  AddChildView(&location_icon_view_);
  location_icon_view_.SetVisible(true);
  location_icon_view_.SetDragController(this);
  location_icon_view_.set_parent_owned(false);

  AddChildView(&ev_bubble_view_);
  ev_bubble_view_.SetVisible(false);
  ev_bubble_view_.SetDragController(this);
  ev_bubble_view_.set_parent_owned(false);

  // URL edit field.
  // View container for URL edit field.
#if defined(OS_WIN)
  location_entry_.reset(new AutocompleteEditViewWin(font_, this, model_, this,
      GetWidget()->GetNativeView(), profile_, command_updater_,
      popup_window_mode_, this));
#else
  location_entry_.reset(new AutocompleteEditViewGtk(this, model_, profile_,
      command_updater_, popup_window_mode_, this));
  location_entry_->Init();
  // Make all the children of the widget visible. NOTE: this won't display
  // anything, it just toggles the visible flag.
  gtk_widget_show_all(location_entry_->GetNativeView());
  // Hide the widget. NativeViewHostGtk will make it visible again as
  // necessary.
  gtk_widget_hide(location_entry_->GetNativeView());
#endif
  location_entry_view_ = new views::NativeViewHost;
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->GetNativeView());

  AddChildView(&selected_keyword_view_);
  selected_keyword_view_.SetFont(font_);
  selected_keyword_view_.SetVisible(false);
  selected_keyword_view_.set_parent_owned(false);

  SkColor dimmed_text = GetColor(ToolbarModel::NONE, DEEMPHASIZED_TEXT);

  AddChildView(&keyword_hint_view_);
  keyword_hint_view_.SetVisible(false);
  keyword_hint_view_.SetFont(font_);
  keyword_hint_view_.SetColor(dimmed_text);
  keyword_hint_view_.set_parent_owned(false);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_blocked_view = new ContentSettingImageView(
        static_cast<ContentSettingsType>(i), this, profile_);
    content_setting_views_.push_back(content_blocked_view);
    AddChildView(content_blocked_view);
    content_blocked_view->SetVisible(false);
  }

  if (!popup_window_mode_) {
    AddChildView(&star_view_);
    star_view_.SetVisible(true);
    star_view_.set_parent_owned(false);
  }

  // Notify us when any ancestor is resized.  In this case we want to tell the
  // AutocompleteEditView to close its popup.
  SetNotifyWhenVisibleBoundsInRootChanges(true);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);

  OnChanged();
}

bool LocationBarView::IsInitialized() const {
  return location_entry_view_ != NULL;
}

// static
SkColor LocationBarView::GetColor(ToolbarModel::SecurityLevel security_level,
                                  ColorKind kind) {
  switch (kind) {
#if defined(OS_WIN)
    case BACKGROUND:    return color_utils::GetSysSkColor(COLOR_WINDOW);
    case TEXT:          return color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    case SELECTED_TEXT: return color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
#else
    // TODO(beng): source from theme provider.
    case BACKGROUND:    return SK_ColorWHITE;
    case TEXT:          return SK_ColorBLACK;
    case SELECTED_TEXT: return SK_ColorWHITE;
#endif

    case DEEMPHASIZED_TEXT:
      return color_utils::AlphaBlend(GetColor(security_level, TEXT),
                                     GetColor(security_level, BACKGROUND), 128);

    case SECURITY_TEXT: {
      SkColor color;
      switch (security_level) {
        case ToolbarModel::EV_SECURE:
        case ToolbarModel::SECURE:
          color = SkColorSetRGB(7, 149, 0);
          break;

        case ToolbarModel::SECURITY_WARNING:
          return GetColor(security_level, DEEMPHASIZED_TEXT);
          break;

        case ToolbarModel::SECURITY_ERROR:
          color = SkColorSetRGB(162, 0, 0);
          break;

        default:
          NOTREACHED();
          return GetColor(security_level, TEXT);
      }
      return color_utils::GetReadableColor(color, GetColor(security_level,
                                                           BACKGROUND));
    }

    default:
      NOTREACHED();
      return GetColor(security_level, TEXT);
  }
}

void LocationBarView::Update(const TabContents* tab_for_state_restoring) {
  RefreshContentSettingViews();
  RefreshPageActionViews();
  location_entry_->Update(tab_for_state_restoring);
  OnChanged();
}

void LocationBarView::UpdateContentSettingsIcons() {
  RefreshContentSettingViews();

  Layout();
  SchedulePaint();
}

void LocationBarView::UpdatePageActions() {
  size_t count_before = page_action_views_.size();
  RefreshPageActionViews();
  if (page_action_views_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }

  Layout();
  SchedulePaint();
}

void LocationBarView::InvalidatePageActions() {
  size_t count_before = page_action_views_.size();
  DeletePageActionViews();
  if (page_action_views_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarView::Focus() {
  // Focus the location entry native view.
  location_entry_->SetFocus();
}

void LocationBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ != profile) {
    profile_ = profile;
    location_entry_->model()->SetProfile(profile);
    selected_keyword_view_.set_profile(profile);
    keyword_hint_view_.set_profile(profile);
    for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
         i != content_setting_views_.end(); ++i)
      (*i)->set_profile(profile);
  }
}

TabContents* LocationBarView::GetTabContents() const {
  return delegate_->GetTabContents();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (popup_window_mode_)
    return;

  DCHECK(page_action);
  TabContents* contents = delegate_->GetTabContents();

  RefreshPageActionViews();
  PageActionWithBadgeView* page_action_view =
      static_cast<PageActionWithBadgeView*>(GetPageActionView(page_action));
  DCHECK(page_action_view);
  if (!page_action_view)
    return;

  page_action_view->image_view()->set_preview_enabled(preview_enabled);
  page_action_view->UpdateVisibility(contents,
      GURL(WideToUTF8(model_->GetText())));
  Layout();
  SchedulePaint();
}

views::View* LocationBarView::GetPageActionView(
    ExtensionAction *page_action) {
  DCHECK(page_action);
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->image_view()->page_action() == page_action)
      return *i;
  }
  return NULL;
}

void LocationBarView::SetStarToggled(bool on) {
  star_view_.SetToggled(on);
}

void LocationBarView::ShowStarBubble(const GURL& url, bool newly_bookmarked) {
  gfx::Rect screen_bounds(star_view_.GetImageBounds());
  // Compensate for some built-in padding in the Star image.
  screen_bounds.Inset(1, 1, 1, 2);
  gfx::Point origin(screen_bounds.origin());
  views::View::ConvertPointToScreen(&star_view_, &origin);
  screen_bounds.set_origin(origin);
  browser::ShowBookmarkBubbleView(GetWindow(), screen_bounds, &star_view_,
                                  profile_, url, newly_bookmarked);
}

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0,
      (popup_window_mode_ ? kPopupBackground : kBackground)->height());
}

void LocationBarView::Layout() {
  if (!location_entry_.get())
    return;

  int entry_width = width() - kViewPadding;

  // |location_icon_view_| is visible except when |ev_bubble_view_| or
  // |selected_keyword_view_| are visible.
  int location_icon_width = 0;
  int ev_bubble_width = 0;
  location_icon_view_.SetVisible(false);
  ev_bubble_view_.SetVisible(false);
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  if (show_selected_keyword) {
    entry_width -= kViewPadding;  // Assume the keyword might be hidden.
  } else if (model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    ev_bubble_view_.SetVisible(true);
    ev_bubble_view_.SetLabel(model_->GetEVCertName());
    ev_bubble_width = ev_bubble_view_.GetPreferredSize().width();
    entry_width -= kBubblePadding + ev_bubble_width + kViewPadding;
  } else {
    location_icon_view_.SetVisible(true);
    location_icon_width = location_icon_view_.GetPreferredSize().width();
    entry_width -=
        kViewPadding + location_icon_width + kLocationIconEditPadding;
  }

  entry_width -= star_view_.GetPreferredSize().width() + kViewPadding;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kViewPadding;
  }
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kViewPadding;
  }

#if defined(OS_WIN)
  RECT formatting_rect;
  location_entry_->GetRect(&formatting_rect);
  RECT edit_bounds;
  location_entry_->GetClientRect(&edit_bounds);
  int max_edit_width = entry_width - formatting_rect.left -
                       (edit_bounds.right - formatting_rect.right);
#else
  int max_edit_width = entry_width;
#endif

  if (max_edit_width < 0)
    return;
  const int available_width = AvailableWidth(max_edit_width);

  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;
  selected_keyword_view_.SetVisible(show_selected_keyword);
  keyword_hint_view_.SetVisible(show_keyword_hint);
  if (show_selected_keyword) {
    if (selected_keyword_view_.keyword() != keyword)
      selected_keyword_view_.SetKeyword(keyword);
  } else if (show_keyword_hint) {
    if (keyword_hint_view_.keyword() != keyword)
      keyword_hint_view_.SetKeyword(keyword);
  }

  // TODO(sky): baseline layout.
  int location_y = TopMargin();
  int location_height = std::max(height() - location_y - kVertMargin, 0);

  // Lay out items to the right of the edit field.
  int offset = width() - kViewPadding;
  int star_width = star_view_.GetPreferredSize().width();
  offset -= star_width;
  star_view_.SetBounds(offset, location_y, star_width, location_height);
  offset -= kViewPadding;

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible()) {
      int page_action_width = (*i)->GetPreferredSize().width();
      offset -= page_action_width;
      (*i)->SetBounds(offset, location_y, page_action_width, location_height);
      offset -= kViewPadding;
    }
  }
  // We use a reverse_iterator here because we're laying out the views from
  // right to left but in the vector they're ordered left to right.
  for (ContentSettingViews::const_reverse_iterator
       i(content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->IsVisible()) {
      int content_blocked_width = (*i)->GetPreferredSize().width();
      offset -= content_blocked_width;
      (*i)->SetBounds(offset, location_y, content_blocked_width,
                      location_height);
      offset -= kViewPadding;
    }
  }

  // Now lay out items to the left of the edit field.
  if (location_icon_view_.IsVisible()) {
    location_icon_view_.SetBounds(kViewPadding, location_y, location_icon_width,
                                  location_height);
    offset = location_icon_view_.bounds().right() + kLocationIconEditPadding;
  } else if (ev_bubble_view_.IsVisible()) {
    ev_bubble_view_.SetBounds(kBubblePadding, location_y, ev_bubble_width,
                              location_height);
    offset = ev_bubble_view_.bounds().right() + kViewPadding;
  } else {
    offset = show_selected_keyword ? kBubblePadding : kViewPadding;
  }

  // Now lay out the edit field and views that autocollapse to give it more
  // room.
  gfx::Rect location_bounds(offset, location_y, entry_width, location_height);
  if (show_selected_keyword) {
    LayoutView(true, &selected_keyword_view_, available_width,
               &location_bounds);
    if (!selected_keyword_view_.IsVisible()) {
      location_bounds.set_x(
          location_bounds.x() + kViewPadding - kBubblePadding);
    }
  } else if (show_keyword_hint) {
    LayoutView(false, &keyword_hint_view_, available_width,
               &location_bounds);
  }

  location_entry_view_->SetBounds(location_bounds);
}

void LocationBarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

  const SkBitmap* background =
      popup_window_mode_ ?
          kPopupBackground :
          GetThemeProvider()->GetBitmapNamed(IDR_LOCATIONBG);

  canvas->TileImageInt(*background, 0, 0, 0, 0, width(), height());
  int top_margin = TopMargin();
  canvas->FillRectInt(GetColor(ToolbarModel::NONE, BACKGROUND), 0,
                      top_margin, width(),
                      std::max(height() - top_margin - kVertMargin, 0));
}

void LocationBarView::VisibleBoundsInRootChanged() {
  location_entry_->ClosePopup();
}

#if defined(OS_WIN)
bool LocationBarView::OnMousePressed(const views::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
  } else if (event.IsMiddleMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_MBUTTONDBLCLK : WM_MBUTTONDOWN;
  } else if (event.IsRightMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_RBUTTONDBLCLK : WM_RBUTTONDOWN;
  } else {
    NOTREACHED();
    return false;
  }
  OnMouseEvent(event, msg);
  return true;
}

bool LocationBarView::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseEvent(event, WM_MOUSEMOVE);
  return true;
}

void LocationBarView::OnMouseReleased(const views::MouseEvent& event,
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
#endif

void LocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (command_updater_) {
    if (!alternate_nav_url.is_valid()) {
      command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
      return;
    }

    scoped_ptr<AlternateNavURLFetcher> fetcher(
        new AlternateNavURLFetcher(alternate_nav_url));
    // The AlternateNavURLFetcher will listen for the pending navigation
    // notification that will be issued as a result of the "open URL." It
    // will automatically install itself into that navigation controller.
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
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
  location_icon_view_.SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          location_entry_->GetIcon()));
  Layout();
  SchedulePaint();
}

void LocationBarView::OnInputInProgress(bool in_progress) {
  delegate_->OnInputInProgress(in_progress);
}

void LocationBarView::OnKillFocus() {
}

void LocationBarView::OnSetFocus() {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
}

SkBitmap LocationBarView::GetFavIcon() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return delegate_->GetTabContents()->GetFavIcon();
}

std::wstring LocationBarView::GetTitle() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return UTF16ToWideHack(delegate_->GetTabContents()->GetTitle());
}

int LocationBarView::TopMargin() const {
  return std::min(kVertMargin, height());
}

int LocationBarView::AvailableWidth(int location_bar_width) {
#if defined(OS_WIN)
  // Use font_.GetStringWidth() instead of
  // PosFromChar(location_entry_->GetTextLength()) because PosFromChar() is
  // apparently buggy. In both LTR UI and RTL UI with left-to-right layout,
  // PosFromChar(i) might return 0 when i is greater than 1.
  return std::max(
      location_bar_width - font_.GetStringWidth(location_entry_->GetText()), 0);
#else
  return location_bar_width - location_entry_->TextWidth();
#endif
}

bool LocationBarView::UsePref(int pref_width, int available_width) {
  return (pref_width + kViewPadding <= available_width);
}

void LocationBarView::LayoutView(bool leading,
                                 views::View* view,
                                 int available_width,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  gfx::Size view_size = view->GetPreferredSize();
  if (!UsePref(view_size.width(), available_width))
    view_size = view->GetMinimumSize();
  if (view_size.width() + kViewPadding >= bounds->width()) {
    view->SetVisible(false);
    return;
  }
  if (leading) {
    view->SetBounds(bounds->x(), bounds->y(), view_size.width(),
                    bounds->height());
    bounds->Offset(view_size.width() + kViewPadding, 0);
  } else {
    view->SetBounds(bounds->right() - view_size.width(), bounds->y(),
                    view_size.width(), bounds->height());
  }
  bounds->set_width(bounds->width() - view_size.width() - kViewPadding);
  view->SetVisible(true);
}

void LocationBarView::RefreshContentSettingViews() {
  const TabContents* tab_contents = delegate_->GetTabContents();
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->UpdateFromTabContents(
        model_->input_in_progress() ? NULL : tab_contents);
  }
}

void LocationBarView::DeletePageActionViews() {
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    RemoveChildView(*i);
  STLDeleteElements(&page_action_views_);
}

void LocationBarView::RefreshPageActionViews() {
  if (popup_window_mode_)
    return;

  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  std::map<ExtensionAction*, bool> old_visibility;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    old_visibility[(*i)->image_view()->page_action()] = (*i)->IsVisible();

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  std::vector<ExtensionAction*> page_actions;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions.size() != page_action_views_.size()) {
    DeletePageActionViews();  // Delete the old views (if any).

    page_action_views_.resize(page_actions.size());

    for (size_t i = 0; i < page_actions.size(); ++i) {
      page_action_views_[i] = new PageActionWithBadgeView(
          new PageActionImageView(this, profile_, page_actions[i]));
      page_action_views_[i]->SetVisible(false);
      AddChildView(page_action_views_[i]);
    }
  }

  TabContents* contents = delegate_->GetTabContents();
  if (!page_action_views_.empty() && contents) {
    GURL url = GURL(WideToUTF8(model_->GetText()));

    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(contents, url);

      // Check if the visibility of the action changed and notify if it did.
      ExtensionAction* action = (*i)->image_view()->page_action();
      if (old_visibility.find(action) == old_visibility.end() ||
          old_visibility[action] != (*i)->IsVisible()) {
        NotificationService::current()->Notify(
            NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
            Source<ExtensionAction>(action),
            Details<TabContents>(contents));
      }
    }
  }
}

#if defined(OS_WIN)
void LocationBarView::OnMouseEvent(const views::MouseEvent& event, UINT msg) {
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

  gfx::Point screen_point(event.location());
  ConvertPointToScreen(this, &screen_point);
  location_entry_->HandleExternalMsg(msg, flags, screen_point.ToPOINT());
}
#endif

void LocationBarView::ShowFirstRunBubbleInternal(
    FirstRun::BubbleType bubble_type) {
#if defined(OS_WIN)  // First run bubble doesn't make sense for Chrome OS.
  // If the browser is no longer active, let's not show the info bubble, as this
  // would make the browser the active window again.
  if (!location_entry_view_ || !location_entry_view_->GetWidget()->IsActive())
    return;

  // Point at the start of the edit control; adjust to look as good as possible.
  const int kXOffset = 1;   // Text looks like it actually starts 1 px in.
  const int kYOffset = -4;  // Point into the omnibox, not just at its edge.
  gfx::Point origin(location_entry_view_->bounds().x() + kXOffset,
                    y() + height() + kYOffset);
  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (UILayoutIsRightToLeft())
    origin.set_x(width() - origin.x());
  views::View::ConvertPointToScreen(this, &origin);
  FirstRunBubble::Show(profile_, GetWindow(), gfx::Rect(origin, gfx::Size()),
                       bubble_type);
#endif
}

bool LocationBarView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  if (keyword_hint_view_.IsVisible() &&
      views::FocusManager::IsTabTraversalKeyEvent(e)) {
    // We want to receive tab key events when the hint is showing.
    return true;
  }

#if defined(OS_WIN)
  return location_entry_->SkipDefaultKeyEventProcessing(e);
#else
  // TODO(jcampan): We need to refactor the code of
  // AutocompleteEditViewWin::SkipDefaultKeyEventProcessing into this class so
  // it can be shared between Windows and Linux.
  // For now, we just override back-space as it is the accelerator for back
  // navigation.
  if (e.GetKeyCode() == base::VKEY_BACK)
    return true;
  return false;
#endif
}

bool LocationBarView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}


void LocationBarView::WriteDragData(views::View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) {
  DCHECK(GetDragOperations(sender, press_pt) != DragDropTypes::DRAG_NONE);

  TabContents* tab_contents = delegate_->GetTabContents();
  DCHECK(tab_contents);
  drag_utils::SetURLAndDragImage(tab_contents->GetURL(),
                                 UTF16ToWideHack(tab_contents->GetTitle()),
                                 tab_contents->GetFavIcon(), data);
}

int LocationBarView::GetDragOperations(views::View* sender,
                                       const gfx::Point& p) {
  DCHECK((sender == &location_icon_view_) || (sender == &ev_bubble_view_));
  TabContents* tab_contents = delegate_->GetTabContents();
  return (tab_contents && tab_contents->GetURL().is_valid() &&
          !location_entry()->IsEditingOrEmpty()) ?
      (DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK) :
      DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDrag(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) {
  return true;
}

// ClickHandler ----------------------------------------------------------------

LocationBarView::ClickHandler::ClickHandler(const views::View* owner,
                                            const LocationBarView* location_bar)
    : owner_(owner),
      location_bar_(location_bar) {
}

void LocationBarView::ClickHandler::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  if (canceled || !owner_->HitTest(event.location()))
    return;

  // Do not show page info if the user has been editing the location
  // bar, or the location bar is at the NTP.
  if (location_bar_->location_entry()->IsEditingOrEmpty())
    return;

  TabContents* tab = location_bar_->GetTabContents();
  NavigationEntry* nav_entry = tab->controller().GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return;
  }
  tab->ShowPageInfo(nav_entry->url(), nav_entry->ssl(), true);
}

// LocationIconView ------------------------------------------------------------

LocationBarView::LocationIconView::LocationIconView(
    const LocationBarView* location_bar)
    : ALLOW_THIS_IN_INITIALIZER_LIST(click_handler_(this, location_bar)) {
}

LocationBarView::LocationIconView::~LocationIconView() {
}

bool LocationBarView::LocationIconView::OnMousePressed(
    const views::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationBarView::LocationIconView::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  click_handler_.OnMouseReleased(event, canceled);
}

// IconLabelBubbleView ---------------------------------------------------------

// Amount to offset the image.
static const int kImageOffset = 1;

// Amount to offset the label from the image.
static const int kLabelOffset = 3;

// Amount of padding after the label.
static const int kLabelPadding = 4;

LocationBarView::IconLabelBubbleView::IconLabelBubbleView(
    const int background_images[],
    int contained_image,
    const SkColor& color)
    : background_painter_(background_images) {
  AddChildView(&image_);
  image_.set_parent_owned(false);
  image_.SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(contained_image));
  AddChildView(&label_);
  label_.set_parent_owned(false);
  label_.SetColor(color);
}

LocationBarView::IconLabelBubbleView::~IconLabelBubbleView() {
}

void LocationBarView::IconLabelBubbleView::SetFont(const gfx::Font& font) {
  label_.SetFont(font);
}

void LocationBarView::IconLabelBubbleView::SetLabel(const std::wstring& label) {
  label_.SetText(label);
}

void LocationBarView::IconLabelBubbleView::Paint(gfx::Canvas* canvas) {
  int y_offset = (GetParent()->height() - height()) / 2;
  canvas->TranslateInt(0, y_offset);
  background_painter_.Paint(width(), height(), canvas);
  canvas->TranslateInt(0, -y_offset);
}

gfx::Size LocationBarView::IconLabelBubbleView::GetPreferredSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(label_.GetPreferredSize().width(), 0);
  return size;
}

void LocationBarView::IconLabelBubbleView::Layout() {
  image_.SetBounds(kImageOffset, 0, image_.GetPreferredSize().width(),
                   height());
  gfx::Size label_size(label_.GetPreferredSize());
  label_.SetBounds(image_.x() + image_.width() + kLabelOffset,
                   (height() - label_size.height()) / 2, label_size.width(),
                   label_size.height());
}

gfx::Size LocationBarView::IconLabelBubbleView::GetNonLabelSize() {
  return gfx::Size(kImageOffset + image_.GetPreferredSize().width() +
      kLabelOffset + kLabelPadding, background_painter_.height());
}

// EVBubbleView ----------------------------------------------------------------

LocationBarView::EVBubbleView::EVBubbleView(const int background_images[],
                                            int contained_image,
                                            const SkColor& color,
                                            const LocationBarView* location_bar)
    : IconLabelBubbleView(background_images, contained_image, color),
      ALLOW_THIS_IN_INITIALIZER_LIST(click_handler_(this, location_bar)) {
}

LocationBarView::EVBubbleView::~EVBubbleView() {
}

bool LocationBarView::EVBubbleView::OnMousePressed(
    const views::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationBarView::EVBubbleView::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  click_handler_.OnMouseReleased(event, canceled);
}

// SelectedKeywordView ---------------------------------------------------------

LocationBarView::SelectedKeywordView::SelectedKeywordView(
    const int background_images[],
    int contained_image,
    const SkColor& color,
    Profile* profile)
    : IconLabelBubbleView(background_images, contained_image, color),
      profile_(profile) {
  full_label_.SetVisible(false);
  partial_label_.SetVisible(false);
}

LocationBarView::SelectedKeywordView::~SelectedKeywordView() {
}

void LocationBarView::SelectedKeywordView::SetFont(const gfx::Font& font) {
  IconLabelBubbleView::SetFont(font);
  full_label_.SetFont(font);
  partial_label_.SetFont(font);
}

gfx::Size LocationBarView::SelectedKeywordView::GetPreferredSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(full_label_.GetPreferredSize().width(), 0);
  return size;
}

gfx::Size LocationBarView::SelectedKeywordView::GetMinimumSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(partial_label_.GetMinimumSize().width(), 0);
  return size;
}

void LocationBarView::SelectedKeywordView::Layout() {
  SetLabel((width() == GetPreferredSize().width()) ?
      full_label_.GetText() : partial_label_.GetText());
  IconLabelBubbleView::Layout();
}

void LocationBarView::SelectedKeywordView::SetKeyword(
    const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring short_name = GetKeywordName(profile_, keyword);
  full_label_.SetText(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                            short_name));
  const std::wstring min_string = CalculateMinString(short_name);
  partial_label_.SetText(min_string.empty() ?
      full_label_.GetText() :
      l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, min_string));
}

std::wstring LocationBarView::SelectedKeywordView::CalculateMinString(
    const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  std::wstring min_string;
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = l10n_util::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(min_string, &min_string);
  return min_string;
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

void LocationBarView::KeywordHintView::SetFont(const gfx::Font& font) {
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
      GetKeywordName(profile_, keyword), &content_param_offsets));
  if (content_param_offsets.size() == 2) {
    leading_label_.SetText(keyword_hint.substr(0,
                                               content_param_offsets.front()));
    trailing_label_.SetText(keyword_hint.substr(content_param_offsets.front()));
  } else {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
  }
}

void LocationBarView::KeywordHintView::Paint(gfx::Canvas* canvas) {
  int image_x = leading_label_.IsVisible() ? leading_label_.width() : 0;

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

gfx::Size LocationBarView::KeywordHintView::GetPreferredSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  gfx::Size prefsize = leading_label_.GetPreferredSize();
  int width = prefsize.width();
  width += kTabButtonBitmap->width();
  prefsize = trailing_label_.GetPreferredSize();
  width += prefsize.width();
  return gfx::Size(width, prefsize.height());
}

gfx::Size LocationBarView::KeywordHintView::GetMinimumSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  return gfx::Size(kTabButtonBitmap->width(), 0);
}

void LocationBarView::KeywordHintView::Layout() {
  // TODO(sky): baseline layout.
  bool show_labels = (width() != kTabButtonBitmap->width());

  leading_label_.SetVisible(show_labels);
  trailing_label_.SetVisible(show_labels);
  int x = 0;
  gfx::Size pref;

  if (show_labels) {
    pref = leading_label_.GetPreferredSize();
    leading_label_.SetBounds(x, 0, pref.width(), height());

    x += pref.width() + kTabButtonBitmap->width();
    pref = trailing_label_.GetPreferredSize();
    trailing_label_.SetBounds(x, 0, pref.width(), height());
  }
}

// ContentSettingImageView------------------------------------------------------

LocationBarView::ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    const LocationBarView* parent,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      parent_(parent),
      profile_(profile),
      info_bubble_(NULL) {
}

LocationBarView::ContentSettingImageView::~ContentSettingImageView() {
  if (info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::ContentSettingImageView::UpdateFromTabContents(
    const TabContents* tab_contents) {
  int old_icon = content_setting_image_model_->get_icon();
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  if (old_icon != content_setting_image_model_->get_icon()) {
    SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
        content_setting_image_model_->get_icon()));
  }
  SetTooltipText(UTF8ToWide(content_setting_image_model_->get_tooltip()));
  SetVisible(true);
}

bool LocationBarView::ContentSettingImageView::OnMousePressed(
    const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationBarView::ContentSettingImageView::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  if (canceled || !HitTest(event.location()))
    return;

  TabContents* tab_contents = parent_->GetTabContents();
  if (!tab_contents)
    return;

  gfx::Rect screen_bounds(GetImageBounds());
  gfx::Point origin(screen_bounds.origin());
  views::View::ConvertPointToScreen(this, &origin);
  screen_bounds.set_origin(origin);
  ContentSettingBubbleContents* bubble_contents =
      new ContentSettingBubbleContents(
          ContentSettingBubbleModel::CreateContentSettingBubbleModel(
              tab_contents, profile_,
              content_setting_image_model_->get_content_settings_type()),
          profile_, tab_contents);
  DCHECK(!info_bubble_);
  info_bubble_ =
      InfoBubble::Show(GetWindow(), screen_bounds, bubble_contents, this);
  bubble_contents->set_info_bubble(info_bubble_);
}

void LocationBarView::ContentSettingImageView::VisibilityChanged(
    View* starting_from,
    bool is_visible) {
  if (!is_visible && info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::ContentSettingImageView::InfoBubbleClosing(
    InfoBubble* info_bubble,
    bool closed_by_escape) {
  info_bubble_ = NULL;
}

bool LocationBarView::ContentSettingImageView::CloseOnEscape() {
  return true;
}

// PageActionImageView----------------------------------------------------------

LocationBarView::PageActionImageView::PageActionImageView(
    LocationBarView* owner,
    Profile* profile,
    ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      current_tab_id_(-1),
      preview_enabled_(false),
      popup_(NULL) {
  Extension* extension = profile->GetExtensionsService()->GetExtensionById(
      page_action->extension_id(), false);
  DCHECK(extension);

  // Load all the icons declared in the manifest. This is the contents of the
  // icons array, plus the default_icon property, if any.
  std::vector<std::string> icon_paths(*page_action->icon_paths());
  if (!page_action_->default_icon_path().empty())
    icon_paths.push_back(page_action_->default_icon_path());

  for (std::vector<std::string>::iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    tracker_.LoadImage(extension, extension->GetResource(*iter),
                       gfx::Size(Extension::kPageActionIconMaxSize,
                                 Extension::kPageActionIconMaxSize),
                       ImageLoadingTracker::DONT_CACHE);
  }
}

LocationBarView::PageActionImageView::~PageActionImageView() {
  if (popup_)
    HidePopup();
}

void LocationBarView::PageActionImageView::ExecuteAction(int button,
    bool inspect_with_devtools) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    // In tests, GetLastActive could return NULL, so we need to have
    // a fallback.
    // TODO(erikkay): Find a better way to get the Browser that this
    // button is in.
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (!browser)
      browser = BrowserList::FindBrowserWithProfile(profile_);
    DCHECK(browser);

    bool popup_showing = popup_ != NULL;

    // Always hide the current popup. Only one popup at a time.
    HidePopup();

    // If we were already showing, then treat this click as a dismiss.
    if (popup_showing)
      return;

    gfx::Rect screen_bounds(GetImageBounds());
    gfx::Point origin(screen_bounds.origin());
    View::ConvertPointToScreen(this, &origin);
    screen_bounds.set_origin(origin);

    popup_ = ExtensionPopup::Show(
        page_action_->GetPopupUrl(current_tab_id_),
        browser,
        browser->profile(),
        browser->window()->GetNativeHandle(),
        screen_bounds,
        BubbleBorder::TOP_RIGHT,
        true,  // Activate the popup window.
        inspect_with_devtools,
        ExtensionPopup::BUBBLE_CHROME,
        this);  // ExtensionPopup::Observer
  } else {
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        profile_, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(), button);
  }
}

bool LocationBarView::PageActionImageView::OnMousePressed(
    const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.  (Also, triggering on mouse press causes bugs like
  // http://crbug.com/33155.)
  return true;
}

void LocationBarView::PageActionImageView::OnMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  if (canceled || !HitTest(event.location()))
    return;

  int button = -1;
  if (event.IsLeftMouseButton()) {
    button = 1;
  } else if (event.IsMiddleMouseButton()) {
    button = 2;
  } else if (event.IsRightMouseButton()) {
    // Get the top left point of this button in screen coordinates.
    gfx::Point menu_origin;
    ConvertPointToScreen(this, &menu_origin);

    // Make the menu appear below the button.
    menu_origin.Offset(0, height());

    Extension* extension = profile_->GetExtensionsService()->GetExtensionById(
        page_action()->extension_id(), false);
    Browser* browser = BrowserView::GetBrowserViewForNativeWindow(
        platform_util::GetTopLevel(GetWidget()->GetNativeView()))->browser();
    context_menu_contents_ =
        new ExtensionContextMenuModel(extension, browser, this);
    context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
    context_menu_menu_->RunContextMenuAt(menu_origin);
    return;
  }

  ExecuteAction(button, false);  // inspect_with_devtools
}

void LocationBarView::PageActionImageView::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  // We loaded icons()->size() icons, plus one extra if the page action had
  // a default icon.
  int total_icons = static_cast<int>(page_action_->icon_paths()->size());
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    if (index < static_cast<int>(page_action_->icon_paths()->size()))
      page_action_icons_[page_action_->icon_paths()->at(index)] = *image;
    else
      page_action_icons_[page_action_->default_icon_path()] = *image;
  }

  owner_->UpdatePageActions();
}

void LocationBarView::PageActionImageView::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = ExtensionTabUtil::GetTabId(contents);
  current_url_ = url;

  bool visible =
      preview_enabled_ || page_action_->GetIsVisible(current_tab_id_);
  if (visible) {
    // Set the tooltip.
    tooltip_ = page_action_->GetTitle(current_tab_id_);
    SetTooltipText(UTF8ToWide(tooltip_));

    // Set the image.
    // It can come from three places. In descending order of priority:
    // - The developer can set it dynamically by path or bitmap. It will be in
    //   page_action_->GetIcon().
    // - The developer can set it dynamically by index. It will be in
    //   page_action_->GetIconIndex().
    // - It can be set in the manifest by path. It will be in page_action_->
    //   default_icon_path().

    // First look for a dynamically set bitmap.
    SkBitmap icon = page_action_->GetIcon(current_tab_id_);
    if (icon.isNull()) {
      int icon_index = page_action_->GetIconIndex(current_tab_id_);
      std::string icon_path;
      if (icon_index >= 0)
        icon_path = page_action_->icon_paths()->at(icon_index);
      else
        icon_path = page_action_->default_icon_path();

      if (!icon_path.empty()) {
        PageActionMap::iterator iter = page_action_icons_.find(icon_path);
        if (iter != page_action_icons_.end())
          icon = iter->second;
      }
    }

    if (!icon.isNull())
      SetImage(&icon);
  }
  SetVisible(visible);
}

void LocationBarView::PageActionImageView::InspectPopup(
    ExtensionAction* action) {
  ExecuteAction(1,  // left-click
                true);  // inspect_with_devtools
}

void LocationBarView::PageActionImageView::ExtensionPopupClosed(
    ExtensionPopup* popup) {
  DCHECK_EQ(popup_, popup);
  // ExtensionPopup is ref-counted, so we don't need to delete it.
  popup_ = NULL;
}

void LocationBarView::PageActionImageView::HidePopup() {
  if (popup_)
    popup_->Close();
}

// StarView---------------------------------------------------------------------

LocationBarView::StarView::StarView(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetID(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
}

LocationBarView::StarView::~StarView() {
}

void LocationBarView::StarView::SetToggled(bool on) {
  SetTooltipText(l10n_util::GetString(
      on ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR));
  // Since StarView is an ImageView, the SetTooltipText changes the accessible
  // name. To keep the accessible name unchanged, we need to set the accessible
  // name right after we modify the tooltip text for this view.
  SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_STAR));
  SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
      on ? IDR_OMNIBOX_STAR_LIT : IDR_OMNIBOX_STAR));
}

bool LocationBarView::StarView::GetAccessibleRole(
    AccessibilityTypes::Role* role) {
  *role = AccessibilityTypes::ROLE_PUSHBUTTON;
  return true;
}

bool LocationBarView::StarView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationBarView::StarView::OnMouseReleased(const views::MouseEvent& event,
                                                bool canceled) {
  if (!canceled && HitTest(event.location()))
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
}

void LocationBarView::StarView::InfoBubbleClosing(InfoBubble* info_bubble,
                                                  bool closed_by_escape) {
}

bool LocationBarView::StarView::CloseOnEscape() {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {
  // We wait 30 milliseconds to open. It allows less flicker.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarView::ShowFirstRunBubbleInternal, bubble_type);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 30);
}

std::wstring LocationBarView::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarView::GetPageTransition() const {
  return transition_;
}

void LocationBarView::AcceptInput() {
  location_entry_->model()->AcceptInput(CURRENT_TAB, false);
}

void LocationBarView::AcceptInputWithDisposition(WindowOpenDisposition disp) {
  location_entry_->model()->AcceptInput(disp, false);
}

void LocationBarView::FocusLocation(bool select_all) {
  location_entry_->SetFocus();
  if (select_all)
    location_entry_->SelectAll(true);
}

void LocationBarView::FocusSearch() {
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarView::SaveStateToContents(TabContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarView::Revert() {
  location_entry_->RevertAll();
}

int LocationBarView::PageActionVisibleCount() {
  int result = 0;
  for (size_t i = 0; i < page_action_views_.size(); i++) {
    if (page_action_views_[i]->IsVisible())
      ++result;
  }
  return result;
}

ExtensionAction* LocationBarView::GetPageAction(size_t index) {
  if (index < page_action_views_.size())
    return page_action_views_[index]->image_view()->page_action();

  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarView::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->IsVisible()) {
      if (current == index)
        return page_action_views_[i]->image_view()->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarView::TestPageActionPressed(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->IsVisible()) {
      if (current == index) {
        const int kLeftMouseButton = 1;
        page_action_views_[i]->image_view()->ExecuteAction(kLeftMouseButton,
            false);  // inspect_with_devtools
        return;
      }
      ++current;
    }
  }

  NOTREACHED();
}
