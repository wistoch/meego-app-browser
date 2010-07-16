// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/i18n/rtl.h"
#include "base/nsimage_cache_mac.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/content_blocked_bubble_controller.h"
#include "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/cocoa/first_run_bubble_controller.h"
#import "chrome/browser/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/cocoa/location_bar/page_action_decoration.h"
#import "chrome/browser/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/cocoa/location_bar/star_decoration.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/location_bar_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"


// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

LocationBarViewMac::LocationBarViewMac(
    AutocompleteTextField* field,
    CommandUpdater* command_updater,
    ToolbarModel* toolbar_model,
    Profile* profile,
    Browser* browser)
    : edit_view_(new AutocompleteEditViewMac(this, toolbar_model, profile,
                                             command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(
          new SelectedKeywordDecoration([field_ font])),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get(),
                                 [field_ font])),
      star_decoration_(new StarDecoration(command_updater)),
      profile_(profile),
      browser_(browser),
      toolbar_model_(toolbar_model),
      transition_(PageTransition::TYPED),
      first_run_bubble_(this) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_setting_view =
        new ContentSettingImageView(static_cast<ContentSettingsType>(i), this,
                                    profile_);
    content_setting_views_.push_back(content_setting_view);
    content_setting_view->SetVisible(false);
  }

  AutocompleteTextFieldCell* cell = [field_ cell];
  [cell setContentSettingViewsList:&content_setting_views_];

  registrar_.Add(this,
      NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      NotificationService::AllSources());
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];
}

void LocationBarViewMac::ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarViewMac::ShowFirstRunBubbleInternal, bubble_type);
  MessageLoop::current()->PostTask(FROM_HERE, task);
}

void LocationBarViewMac::ShowFirstRunBubbleInternal(
    FirstRun::BubbleType bubble_type) {
  if (!field_ || ![field_ window])
    return;

  // The bubble needs to be just below the Omnibox and slightly to the right
  // of the left omnibox icon, so shift x and y co-ordinates.
  const NSPoint kOffset = NSMakePoint(1, 4);
  [FirstRunBubbleController showForView:field_ offset:kOffset profile:profile_];
}

std::wstring LocationBarViewMac::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarViewMac::GetPageTransition() const {
  return transition_;
}

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  edit_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  edit_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  edit_view_->SetForcedQuery();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
  RefreshContentSettingsViews();
  [field_ updateCursorAndToolTipRects];
  [field_ setNeedsDisplay:YES];
}

void LocationBarViewMac::UpdatePageActions() {
  size_t count_before = page_action_decorations_.size();
  RefreshPageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarViewMac::InvalidatePageActions() {
  size_t count_before = page_action_decorations_.size();
  DeletePageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarViewMac::SaveStateToContents(TabContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  edit_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Update(const TabContents* contents,
                                bool should_restore_state) {
  RefreshPageActionDecorations();
  RefreshContentSettingsViews();
  // AutocompleteEditView restores state if the tab is non-NULL.
  edit_view_->Update(should_restore_state ? contents : NULL);
  OnChanged();
}

void LocationBarViewMac::OnAutocompleteAccept(const GURL& url,
                                              WindowOpenDisposition disposition,
                                              PageTransition::Type transition,
                                              const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (!command_updater_)
    return;

  if (!alternate_nav_url.is_valid()) {
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
    return;
  }

  AlternateNavURLFetcher* fetcher =
      new AlternateNavURLFetcher(alternate_nav_url);
  // The AlternateNavURLFetcher will listen for the pending navigation
  // notification that will be issued as a result of the "open URL." It
  // will automatically install itself into that navigation controller.
  command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
  if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
    // I'm not sure this should be reachable, but I'm not also sure enough
    // that it shouldn't to stick in a NOTREACHED().  In any case, this is
    // harmless.
    delete fetcher;
  } else {
    // The navigation controller will delete the fetcher.
  }
}

void LocationBarViewMac::OnChanged() {
  // Update the location-bar icon.
  const int resource_id = edit_view_->GetIcon();
  NSImage* image = AutocompleteEditViewMac::ImageForResource(resource_id);
  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();
}

void LocationBarViewMac::OnInputInProgress(bool in_progress) {
  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL, false);
}

void LocationBarViewMac::OnSetFocus() {
  // Update the keyword and search hint states.
  OnChanged();
}

void LocationBarViewMac::OnKillFocus() {
  // Do nothing.
}

SkBitmap LocationBarViewMac::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewMac::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewMac::Revert() {
  edit_view_->RevertAll();
}

// TODO(pamg): Change all these, here and for other platforms, to size_t.
int LocationBarViewMac::PageActionCount() {
  return static_cast<int>(page_action_decorations_.size());
}

int LocationBarViewMac::PageActionVisibleCount() {
  int result = 0;
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->IsVisible())
      ++result;
  }
  return result;
}

TabContents* LocationBarViewMac::GetTabContents() const {
  return browser_->GetSelectedTabContents();
}

PageActionDecoration* LocationBarViewMac::GetPageActionDecoration(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->page_action() == page_action)
      return page_action_decorations_[i];
  }
  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::SetPreviewEnabledPageAction(
    ExtensionAction* page_action, bool preview_enabled) {
  DCHECK(page_action);
  TabContents* contents = GetTabContents();
  if (!contents)
    return;
  RefreshPageActionDecorations();
  Layout();

  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  DCHECK(decoration);
  if (!decoration)
    return;

  decoration->set_preview_enabled(preview_enabled);
  decoration->UpdateVisibility(contents,
      GURL(WideToUTF8(toolbar_model_->GetText())));
}

NSPoint LocationBarViewMac::GetPageActionBubblePoint(
    ExtensionAction* page_action) {
  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  if (!decoration)
    return NSZeroPoint;

  AutocompleteTextFieldCell* cell = [field_ cell];
  NSRect frame = [cell frameForDecoration:decoration inFrame:[field_ bounds]];
  DCHECK(!NSIsEmptyRect(frame));
  if (NSIsEmptyRect(frame))
    return NSZeroPoint;

  NSPoint bubble_point = decoration->GetBubblePointInFrame(frame);
  return [field_ convertPoint:bubble_point toView:nil];
}

ExtensionAction* LocationBarViewMac::GetPageAction(size_t index) {
  if (index < page_action_decorations_.size())
    return page_action_decorations_[index]->page_action();
  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarViewMac::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->IsVisible()) {
      if (current == index)
        return page_action_decorations_[i]->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::TestPageActionPressed(size_t index) {
  DCHECK_LT(index, page_action_decorations_.size());
  if (index < page_action_decorations_.size())
    page_action_decorations_[index]->OnMousePressed(NSZeroRect);
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  star_decoration_->SetVisible(editable);
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::SetStarred(bool starred) {
  star_decoration_->SetStarred(starred);

  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  [field_ updateCursorAndToolTipRects];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
}

NSPoint LocationBarViewMac::GetBookmarkBubblePoint() const {
  AutocompleteTextFieldCell* cell = [field_ cell];
  const NSRect frame = [cell frameForDecoration:star_decoration_.get()
                                        inFrame:[field_ bounds]];
  const NSPoint point = star_decoration_->GetBubblePointInFrame(frame);
  return [field_ convertPoint:point toView:nil];
}

NSImage* LocationBarViewMac::GetTabButtonImage() {
  if (!tab_button_image_) {
    SkBitmap* skiaBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
    if (skiaBitmap) {
      tab_button_image_.reset([gfx::SkBitmapToNSImage(*skiaBitmap) retain]);
    }
  }
  return tab_button_image_;
}

NSImage* LocationBarViewMac::GetKeywordImage(const std::wstring& keyword) {
  const TemplateURL* template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url && template_url->IsExtensionKeyword()) {
    const SkBitmap& bitmap = profile_->GetExtensionsService()->
        GetOmniboxIcon(template_url->GetExtensionId());
    return gfx::SkBitmapToNSImage(bitmap);
  }

  return AutocompleteEditViewMac::ImageForResource(IDR_OMNIBOX_SEARCH);
}

void LocationBarViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
      TabContents* contents = GetTabContents();
      if (Details<TabContents>(contents) != details)
        return;

      [field_ updateCursorAndToolTipRects];
      [field_ setNeedsDisplay:YES];
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
}

void LocationBarViewMac::RefreshContentSettingsViews() {
  const TabContents* tab_contents = browser_->GetSelectedTabContents();
  for (ContentSettingViews::iterator it(content_setting_views_.begin());
      it != content_setting_views_.end();
      ++it) {
    (*it)->UpdateFromTabContents(
        toolbar_model_->input_in_progress() ? NULL : tab_contents);
  }
}

// LocationBarImageView---------------------------------------------------------

void LocationBarViewMac::LocationBarImageView::SetImage(NSImage* image) {
  image_.reset([image retain]);
}

void LocationBarViewMac::LocationBarImageView::SetIcon(int resource_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetImage(rb.GetNSImageNamed(resource_id));
}

void LocationBarViewMac::LocationBarImageView::SetLabel(NSString* text,
                                                        NSFont* baseFont,
                                                        NSColor* color) {
  // Create an attributed string for the label, if a label was given.
  label_.reset();
  if (text) {
    DCHECK(color);
    DCHECK(baseFont);
    NSFont* font = [NSFont fontWithDescriptor:[baseFont fontDescriptor]
                                         size:[baseFont pointSize] - 2.0];
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
         color, NSForegroundColorAttributeName,
         font, NSFontAttributeName,
         NULL];
    NSAttributedString* attrStr =
        [[NSAttributedString alloc] initWithString:text attributes:attributes];
    label_.reset(attrStr);
  }
}

void LocationBarViewMac::LocationBarImageView::SetVisible(bool visible) {
  visible_ = visible;
}

NSSize LocationBarViewMac::LocationBarImageView::GetDefaultImageSize() const {
  return NSZeroSize;
}

NSSize LocationBarViewMac::LocationBarImageView::GetImageSize() const {
  NSImage* image = GetImage();
  if (image)
    return [image size];
  return GetDefaultImageSize();
}

// ContentSettingsImageView-----------------------------------------------------
LocationBarViewMac::ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType settings_type,
    LocationBarViewMac* owner,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              settings_type)),
      owner_(owner),
      profile_(profile) {
}

LocationBarViewMac::ContentSettingImageView::~ContentSettingImageView() {}

void LocationBarViewMac::ContentSettingImageView::OnMousePressed(NSRect bounds)
{
  // Get host. This should be shared shared on linux/win/osx medium-term.
  TabContents* tabContents =
      BrowserList::GetLastActive()->GetSelectedTabContents();
  if (!tabContents)
    return;
  GURL url = tabContents->GetURL();
  std::wstring displayHost;
  net::AppendFormattedHost(
      url,
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      &displayHost, NULL, NULL);

  // Transform mouse coordinates to screen space.
  AutocompleteTextField* textField = owner_->GetAutocompleteTextField();
  NSWindow* window = [textField window];
  bounds = [textField convertRect:bounds toView:nil];
  NSPoint anchor = NSMakePoint(NSMidX(bounds) + 1, NSMinY(bounds));
  anchor = [window convertBaseToScreen:anchor];

  // Open bubble.
  ContentSettingBubbleModel* model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          tabContents, profile_,
          content_setting_image_model_->get_content_settings_type());
  [ContentBlockedBubbleController showForModel:model
                                   parentWindow:window
                                     anchoredAt:anchor];
}

NSString* LocationBarViewMac::ContentSettingImageView::GetToolTip() {
  return tooltip_.get();
}

void LocationBarViewMac::ContentSettingImageView::UpdateFromTabContents(
    const TabContents* tab_contents) {
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  if (content_setting_image_model_->is_visible()) {
    // TODO(thakis): We should use pdfs for these icons on OSX.
    // http://crbug.com/35847
    SetIcon(content_setting_image_model_->get_icon());
    SetToolTip(base::SysUTF8ToNSString(
        content_setting_image_model_->get_tooltip()));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

void LocationBarViewMac::ContentSettingImageView::SetToolTip(NSString* tooltip)
{
  tooltip_.reset([tooltip retain]);
}

void LocationBarViewMac::DeletePageActionDecorations() {
  // TODO(shess): Deleting these decorations could result in the cell
  // refering to them before things are laid out again.  Meanwhile, at
  // least fail safe.
  [[field_ cell] clearDecorations];

  page_action_decorations_.reset();
}

void LocationBarViewMac::RefreshPageActionDecorations() {
  if (!IsEditable()) {
    DeletePageActionDecorations();
    return;
  }

  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  std::vector<ExtensionAction*> page_actions;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any Page Actions) load.
  if (page_actions.size() != page_action_decorations_.size()) {
    DeletePageActionDecorations();  // Delete the old views (if any).

    for (size_t i = 0; i < page_actions.size(); ++i) {
      page_action_decorations_.push_back(
          new PageActionDecoration(this, profile_, page_actions[i]));
    }
  }

  if (page_action_decorations_.empty())
    return;

  TabContents* contents = GetTabContents();
  if (!contents)
    return;

  GURL url = GURL(WideToUTF8(toolbar_model_->GetText()));
  for (size_t i = 0; i < page_action_decorations_.size(); ++i)
    page_action_decorations_[i]->UpdateVisibility(contents, url);
}

// TODO(shess): This function should over time grow to closely match
// the views Layout() function.
void LocationBarViewMac::Layout() {
  AutocompleteTextFieldCell* cell = [field_ cell];

  // Reset the left-hand decorations.
  // TODO(shess): Shortly, this code will live somewhere else, like in
  // the constructor.  I am still wrestling with how best to deal with
  // right-hand decorations, which are not a static set.
  [cell clearDecorations];
  [cell addLeftDecoration:location_icon_decoration_.get()];
  [cell addLeftDecoration:selected_keyword_decoration_.get()];
  [cell addLeftDecoration:ev_bubble_decoration_.get()];
  [cell addRightDecoration:star_decoration_.get()];

  // Display order is right to left.
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    [cell addRightDecoration:page_action_decorations_[i]];
  }

  // By default only the location icon is visible.
  location_icon_decoration_->SetVisible(true);
  selected_keyword_decoration_->SetVisible(false);
  ev_bubble_decoration_->SetVisible(false);

  // Get the keyword to use for keyword-search and hinting.
  const std::wstring keyword(edit_view_->model()->keyword());
  std::wstring short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = profile_->GetTemplateURLModel()->
        GetKeywordShortName(keyword, &is_extension_keyword);
  }

  const bool is_keyword_hint = edit_view_->model()->is_keyword_hint();

  if (!keyword.empty() && !is_keyword_hint) {
    // Switch from location icon to keyword mode.
    location_icon_decoration_->SetVisible(false);
    selected_keyword_decoration_->SetVisible(true);
    selected_keyword_decoration_->SetKeyword(short_name, is_extension_keyword);
    selected_keyword_decoration_->SetImage(GetKeywordImage(keyword));

    // TODO(shess): This goes away once the hints are decorations.
    [cell clearHint];
  } else if (toolbar_model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    std::wstring label(toolbar_model_->GetEVCertName());
    ev_bubble_decoration_->SetLabel(base::SysWideToNSString(label));

    // TODO(shess): This goes away once the hints are decorations.
    [cell clearHint];
  } else if (!keyword.empty() && is_keyword_hint) {
    // Keyword is a hint, like "Press [Tab] to search Engine".  [Tab]
    // is a parameter to be replaced by an image.  "Engine" is a
    // parameter to be replaced by text based on the keyword.
    std::vector<size_t> content_param_offsets;
    int message_id = is_extension_keyword ?
        IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
    const std::wstring keyword_hint(
        l10n_util::GetStringF(message_id,
                              std::wstring(), short_name,
                              &content_param_offsets));

    // Should always be 2 offsets, see the comment in
    // location_bar_view.cc after IDS_OMNIBOX_KEYWORD_HINT fetch.
    DCHECK_EQ(content_param_offsets.size(), 2U);

    // Where to put the [TAB] image.
    const size_t split(content_param_offsets.front());

    NSString* prefix = base::SysWideToNSString(keyword_hint.substr(0, split));
    NSString* suffix = base::SysWideToNSString(keyword_hint.substr(split));

    NSImage* image = GetTabButtonImage();
    const CGFloat availableWidth([field_ availableDecorationWidth]);
    [cell setKeywordHintPrefix:prefix image:image suffix:suffix
                availableWidth:availableWidth];
  } else {
    // Nothing interesting to show, plain old text field.
    [cell clearHint];
  }

  // These need to change anytime the layout changes.
  // TODO(shess): Anytime the field editor might have changed, the
  // cursor rects almost certainly should have changed.  The tooltips
  // might change even when the rects don't change.
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ updateCursorAndToolTipRects];

  [field_ setNeedsDisplay:YES];
}
