// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/location_bar_view_gtk.h"

#include <string>

#include "app/gfx/canvas_paint.h"
#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/extension_popup_gtk.h"
#include "chrome/browser/gtk/first_run_bubble.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// We are positioned with a little bit of extra space that we don't use now.
const int kTopMargin = 1;
const int kBottomMargin = 1;
const int kLeftMargin = 1;
const int kRightMargin = 1;
// We draw a border on the top and bottom (but not on left or right).
const int kBorderThickness = 1;

// Left margin of first run bubble.
const int kFirstRunBubbleLeftMargin = 8;
// Extra vertical spacing for first run bubble.
const int kFirstRunBubbleTopMargin = 1;

// The padding around the top, bottom, and sides of the location bar hbox.
// We don't want to edit control's text to be right against the edge,
// as well the tab to search box and other widgets need to have the padding on
// top and bottom to avoid drawing larger than the location bar space.
const int kHboxBorder = 4;

// Padding between the elements in the bar.
static const int kInnerPadding = 4;

// TODO(deanm): Eventually this should be painted with the background png
// image, but for now we get pretty close by just drawing a solid border.
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);
const GdkColor kEvTextColor = GDK_COLOR_RGB(0x00, 0x96, 0x14);  // Green.
const GdkColor kKeywordBackgroundColor = GDK_COLOR_RGB(0xf0, 0xf4, 0xfa);
const GdkColor kKeywordBorderColor = GDK_COLOR_RGB(0xcb, 0xde, 0xf7);

// Size of the rounding of the "Search site for:" box.
const int kCornerSize = 3;

// Returns the short name for a keyword.
std::wstring GetKeywordName(Profile* profile,
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

// If widget is visible, increment the int pointed to by count.
// Suitible for use with gtk_container_foreach.
void CountVisibleWidgets(GtkWidget* widget, gpointer count) {
  if (GTK_WIDGET_VISIBLE(widget))
    *static_cast<int*>(count) += 1;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LocationBarViewGtk

// static
const GdkColor LocationBarViewGtk::kBackgroundColorByLevel[3] = {
  GDK_COLOR_RGB(255, 245, 195),  // SecurityLevel SECURE: Yellow.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel NORMAL: White.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel INSECURE: White.
};

LocationBarViewGtk::LocationBarViewGtk(
    CommandUpdater* command_updater,
    ToolbarModel* toolbar_model,
    const BubblePositioner* bubble_positioner,
    Browser* browser)
    : security_icon_event_box_(NULL),
      security_lock_icon_image_(NULL),
      security_warning_icon_image_(NULL),
      info_label_(NULL),
      tab_to_search_box_(NULL),
      tab_to_search_label_(NULL),
      tab_to_search_hint_(NULL),
      tab_to_search_hint_leading_label_(NULL),
      tab_to_search_hint_icon_(NULL),
      tab_to_search_hint_trailing_label_(NULL),
      profile_(NULL),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      browser_(browser),
      bubble_positioner_(bubble_positioner),
      disposition_(CURRENT_TAB),
      transition_(PageTransition::TYPED),
      first_run_bubble_(this),
      popup_window_mode_(false),
      theme_provider_(NULL) {
}

LocationBarViewGtk::~LocationBarViewGtk() {
  // All of our widgets should have be children of / owned by the alignment.
  hbox_.Destroy();
  page_action_hbox_.Destroy();
}

void LocationBarViewGtk::Init(bool popup_window_mode) {
  popup_window_mode_ = popup_window_mode;
  location_entry_.reset(new AutocompleteEditViewGtk(this,
                                                    toolbar_model_,
                                                    profile_,
                                                    command_updater_,
                                                    popup_window_mode_,
                                                    bubble_positioner_));
  location_entry_->Init();

  hbox_.Own(gtk_hbox_new(FALSE, kInnerPadding));
  gtk_container_set_border_width(GTK_CONTAINER(hbox_.get()), kHboxBorder);
  // We will paint for the alignment, to paint the background and border.
  gtk_widget_set_app_paintable(hbox_.get(), TRUE);
  // Redraw the whole location bar when it changes size (e.g., when toggling
  // the home button on/off.
  gtk_widget_set_redraw_on_allocate(hbox_.get(), TRUE);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  security_lock_icon_image_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_LOCK));
  gtk_widget_set_name(security_lock_icon_image_, "chrome-security-lock-icon");
  gtk_widget_hide(GTK_WIDGET(security_lock_icon_image_));
  security_warning_icon_image_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_WARNING));
  gtk_widget_set_name(security_warning_icon_image_,
                      "chrome-security-warning-icon");
  gtk_widget_hide(GTK_WIDGET(security_warning_icon_image_));

  info_label_ = gtk_label_new(NULL);
  gtk_widget_modify_base(info_label_, GTK_STATE_NORMAL,
      &LocationBarViewGtk::kBackgroundColorByLevel[0]);
  gtk_widget_hide(GTK_WIDGET(info_label_));
  gtk_widget_set_name(info_label_,
                      "chrome-location-bar-info-label");

  g_signal_connect(hbox_.get(), "expose-event",
                   G_CALLBACK(&HandleExposeThunk), this);

  // Tab to search (the keyword box on the left hand side).
  tab_to_search_label_ = gtk_label_new(NULL);

  // This creates a box around the keyword text with a border, background color,
  // and padding around the text.
  tab_to_search_box_ = gtk_util::CreateGtkBorderBin(
      tab_to_search_label_, NULL, 1, 1, 2, 2);
  gtk_widget_set_name(tab_to_search_box_, "chrome-tab-to-search-box");
  gtk_util::ActAsRoundedWindow(tab_to_search_box_, kBorderColor, kCornerSize,
                               gtk_util::ROUNDED_ALL, gtk_util::BORDER_ALL);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), tab_to_search_box_, FALSE, FALSE, 0);

  GtkWidget* align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  // TODO(erg): Like in BrowserToolbarGtk, this used to have a code path on
  // construction for with GTK themes and without. Doing that only on
  // construction was wrong, and I can't see a difference between the two ways
  // anyway... Investigate more later.
  if (popup_window_mode_) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                              kTopMargin + kBorderThickness,
                              kBottomMargin + kBorderThickness,
                              kBorderThickness,
                              kBorderThickness);
  } else {
    gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                              kTopMargin + kBorderThickness,
                              kBottomMargin + kBorderThickness,
                              0, 0);
  }
  gtk_container_add(GTK_CONTAINER(align), location_entry_->widget());
  gtk_box_pack_start(GTK_BOX(hbox_.get()), align, TRUE, TRUE, 0);

  // Tab to search notification (the hint on the right hand side).
  tab_to_search_hint_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_name(tab_to_search_hint_, "chrome-tab-to-search-hint");
  tab_to_search_hint_leading_label_ = gtk_label_new(NULL);
  tab_to_search_hint_icon_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB));
  tab_to_search_hint_trailing_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_leading_label_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_icon_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_trailing_label_,
                     FALSE, FALSE, 0);
  // tab_to_search_hint_ gets hidden initially in OnChanged.  Hiding it here
  // doesn't work, someone is probably calling show_all on our parent box.
  gtk_box_pack_end(GTK_BOX(hbox_.get()), tab_to_search_hint_, FALSE, FALSE, 4);

  // Pack info_label_ and security icons in hbox.  We hide/show them
  // by SetSecurityIcon() and SetInfoText().
  gtk_box_pack_end(GTK_BOX(hbox_.get()), info_label_, FALSE, FALSE, 0);

  GtkWidget* security_icon_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(security_icon_box),
                     security_lock_icon_image_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(security_icon_box),
                     security_warning_icon_image_, FALSE, FALSE, 0);

  // GtkImage is a "no window" widget and requires a GtkEventBox to receive
  // events.
  security_icon_event_box_ = gtk_event_box_new();
  // Make the event box not visible so it does not paint a background.
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(security_icon_event_box_),
                                   FALSE);
  g_signal_connect(security_icon_event_box_, "button-press-event",
                   G_CALLBACK(&OnSecurityIconPressed), this);

  gtk_container_add(GTK_CONTAINER(security_icon_event_box_), security_icon_box);
  gtk_widget_set_name(security_icon_event_box_,
                      "chrome-security-icon-eventbox");
  gtk_box_pack_end(GTK_BOX(hbox_.get()), security_icon_event_box_,
                   FALSE, FALSE, 0);

  page_action_hbox_.Own(gtk_hbox_new(FALSE, kInnerPadding));
  gtk_widget_set_name(page_action_hbox_.get(),
                      "chrome-page-action-hbox");
  gtk_box_pack_end(GTK_BOX(hbox_.get()), page_action_hbox_.get(),
                   FALSE, FALSE, 0);

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_ = GtkThemeProvider::GetFrom(profile_);
  theme_provider_->InitThemesFor(this);
}

void LocationBarViewGtk::SetProfile(Profile* profile) {
  profile_ = profile;
}

void LocationBarViewGtk::Update(const TabContents* contents) {
  SetSecurityIcon(toolbar_model_->GetIcon());
  UpdatePageActions();
  SetInfoText();
  location_entry_->Update(contents);
  // The security level (background color) could have changed, etc.
  if (theme_provider_->UseGtkTheme()) {
    // In GTK mode, we need our parent to redraw, as it draws the text entry
    // border.
    gtk_widget_queue_draw(widget()->parent);
  } else {
    gtk_widget_queue_draw(widget());
  }
}

void LocationBarViewGtk::OnAutocompleteAccept(const GURL& url,
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

void LocationBarViewGtk::OnChanged() {
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint = location_entry_->model()->is_keyword_hint();
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;

  if (show_selected_keyword) {
    SetKeywordLabel(keyword);
    gtk_widget_show_all(tab_to_search_box_);
  } else {
    gtk_widget_hide_all(tab_to_search_box_);
  }

  if (show_keyword_hint) {
    SetKeywordHintLabel(keyword);
    gtk_widget_show_all(tab_to_search_hint_);
  } else {
    gtk_widget_hide_all(tab_to_search_hint_);
  }
}

void LocationBarViewGtk::OnInputInProgress(bool in_progress) {
  // This is identical to the Windows code, except that we don't proxy the call
  // back through the Toolbar, and just access the model here.
  // The edit should make sure we're only notified when something changes.
  DCHECK(toolbar_model_->input_in_progress() != in_progress);

  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL);
}

void LocationBarViewGtk::OnKillFocus() {
}

void LocationBarViewGtk::OnSetFocus() {
}

SkBitmap LocationBarViewGtk::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewGtk::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewGtk::ShowFirstRunBubble(bool use_OEM_bubble) {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarViewGtk::ShowFirstRunBubbleInternal, use_OEM_bubble);
  MessageLoop::current()->PostTask(FROM_HERE, task);
}

std::wstring LocationBarViewGtk::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewGtk::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarViewGtk::GetPageTransition() const {
  return transition_;
}

void LocationBarViewGtk::AcceptInput() {
  AcceptInputWithDisposition(CURRENT_TAB);
}

void LocationBarViewGtk::AcceptInputWithDisposition(
    WindowOpenDisposition disposition) {
  location_entry_->model()->AcceptInput(disposition, false);
}

void LocationBarViewGtk::FocusLocation() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

void LocationBarViewGtk::FocusSearch() {
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarViewGtk::UpdatePageActions() {
  std::vector<ExtensionAction*> page_actions;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  // Find all the page actions.
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // Initialize on the first call, or re-inialize if more extensions have been
  // loaded or added after startup.
  if (page_actions.size() != page_action_views_.size()) {
    page_action_views_.reset();  // Delete the old views (if any).

    for (size_t i = 0; i < page_actions.size(); ++i) {
      page_action_views_.push_back(
          new PageActionViewGtk(this, profile_, page_actions[i]));
      gtk_box_pack_end(GTK_BOX(page_action_hbox_.get()),
                       page_action_views_[i]->widget(), FALSE, FALSE, 0);
    }
  }

  TabContents* contents = browser_->GetSelectedTabContents();
  if (!page_action_views_.empty() && contents) {
    GURL url = GURL(WideToUTF8(toolbar_model_->GetText()));

    for (size_t i = 0; i < page_action_views_.size(); i++)
      page_action_views_[i]->UpdateVisibility(contents, url);
  }

  // If there are no visible page actions, hide the hbox too, so that it does
  // not affect the padding in the location bar.
  if (PageActionVisibleCount())
    gtk_widget_show(page_action_hbox_.get());
  else
    gtk_widget_hide(page_action_hbox_.get());
}

void LocationBarViewGtk::InvalidatePageActions() {
  page_action_views_.reset();
}

void LocationBarViewGtk::SaveStateToContents(TabContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarViewGtk::Revert() {
  location_entry_->RevertAll();
}

int LocationBarViewGtk::PageActionVisibleCount() {
  int count = 0;
  gtk_container_foreach(GTK_CONTAINER(page_action_hbox_.get()),
                        CountVisibleWidgets, &count);
  return count;
}

ExtensionAction* LocationBarViewGtk::GetPageAction(size_t index) {
  if (index >= page_action_views_.size()) {
    NOTREACHED();
    return NULL;
  }

  return page_action_views_[index]->page_action();
}

ExtensionAction* LocationBarViewGtk::GetVisiblePageAction(size_t index) {
  size_t visible_index = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->IsVisible()) {
      if (index == visible_index++)
        return page_action_views_[i]->page_action();
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewGtk::TestPageActionPressed(size_t index) {
  if (index >= page_action_views_.size()) {
    NOTREACHED();
    return;
  }

  page_action_views_[index]->TestActivatePageAction();
}

void LocationBarViewGtk::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK_EQ(type.value,  NotificationType::BROWSER_THEME_CHANGED);

  if (theme_provider_->UseGtkTheme()) {
    gtk_widget_modify_bg(tab_to_search_box_, GTK_STATE_NORMAL, NULL);

    GdkColor border_color = theme_provider_->GetGdkColor(
        BrowserThemeProvider::COLOR_FRAME);
    gtk_util::SetRoundedWindowBorderColor(tab_to_search_box_, border_color);

    gtk_util::SetLabelColor(tab_to_search_label_, NULL);
    gtk_util::SetLabelColor(tab_to_search_hint_leading_label_, NULL);
    gtk_util::SetLabelColor(tab_to_search_hint_trailing_label_, NULL);
  } else {
    gtk_widget_modify_bg(tab_to_search_box_, GTK_STATE_NORMAL,
                         &kKeywordBackgroundColor);
    gtk_util::SetRoundedWindowBorderColor(tab_to_search_box_,
                                          kKeywordBorderColor);

    gtk_util::SetLabelColor(tab_to_search_label_, &gfx::kGdkBlack);
    gtk_util::SetLabelColor(tab_to_search_hint_leading_label_,
                            &gfx::kGdkBlack);
    gtk_util::SetLabelColor(tab_to_search_hint_trailing_label_,
                            &gfx::kGdkBlack);
  }
}

gboolean LocationBarViewGtk::HandleExpose(GtkWidget* widget,
                                          GdkEventExpose* event) {
  GdkRectangle* alloc_rect = &hbox_->allocation;

  // If we're not using GTK theming, draw our own border over the edge pixels
  // of the background.
  if (!profile_ ||
      !GtkThemeProvider::GetFrom(profile_)->UseGtkTheme()) {
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(event->window));
    cairo_rectangle(cr, event->area.x, event->area.y, event->area.width,
                    event->area.height);
    cairo_clip(cr);
    CairoCachedSurface* background = theme_provider_->GetSurfaceNamed(
        popup_window_mode_ ? IDR_LOCATIONBG_POPUPMODE_CENTER : IDR_LOCATIONBG,
        widget);

    // We paint the source to the "outer" rect, which is the size of the hbox's
    // allocation. This image blends with whatever is behind it as the top and
    // bottom fade out.
    background->SetSource(cr, alloc_rect->x, alloc_rect->y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, alloc_rect->x, alloc_rect->y,
                    alloc_rect->width, alloc_rect->height);
    cairo_fill(cr);

    // But on top of that, we also need to draw the "inner" rect, which is all
    // the color that the background should be.
    cairo_rectangle(cr, alloc_rect->x,
                    alloc_rect->y + kTopMargin + kBorderThickness,
                    alloc_rect->width,
                    alloc_rect->height - kTopMargin -
                    kBottomMargin - 2 * kBorderThickness);
    gdk_cairo_set_source_color(cr, const_cast<GdkColor*>(
        &kBackgroundColorByLevel[toolbar_model_->GetSchemeSecurityLevel()]));
    cairo_fill(cr);

    cairo_destroy(cr);
  }

  return FALSE;  // Continue propagating the expose.
}

void LocationBarViewGtk::SetSecurityIcon(ToolbarModel::Icon icon) {
  gtk_widget_hide(GTK_WIDGET(security_lock_icon_image_));
  gtk_widget_hide(GTK_WIDGET(security_warning_icon_image_));
  if (icon != ToolbarModel::NO_ICON)
    gtk_widget_show(GTK_WIDGET(security_icon_event_box_));
  else
    gtk_widget_hide(GTK_WIDGET(security_icon_event_box_));
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      gtk_widget_show(GTK_WIDGET(security_lock_icon_image_));
      break;
    case ToolbarModel::WARNING_ICON:
      gtk_widget_show(GTK_WIDGET(security_warning_icon_image_));
      break;
    case ToolbarModel::NO_ICON:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LocationBarViewGtk::SetInfoText() {
  std::wstring info_text, info_tooltip;
  ToolbarModel::InfoTextType info_text_type =
      toolbar_model_->GetInfoText(&info_text, &info_tooltip);
  if (info_text_type == ToolbarModel::INFO_EV_TEXT) {
    gtk_widget_modify_fg(GTK_WIDGET(info_label_), GTK_STATE_NORMAL,
                         &kEvTextColor);
    gtk_widget_show(GTK_WIDGET(info_label_));
  } else {
    DCHECK_EQ(info_text_type, ToolbarModel::INFO_NO_INFO);
    DCHECK(info_text.empty());
    // Clear info_text.  Should we reset the fg here?
    gtk_widget_hide(GTK_WIDGET(info_label_));
  }
  gtk_label_set_text(GTK_LABEL(info_label_), WideToUTF8(info_text).c_str());
  gtk_widget_set_tooltip_text(GTK_WIDGET(info_label_),
                              WideToUTF8(info_tooltip).c_str());
}

void LocationBarViewGtk::SetKeywordLabel(const std::wstring& keyword) {
  if (keyword.empty())
    return;

  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring short_name = GetKeywordName(profile_, keyword);
  // TODO(deanm): Windows does some measuring of the text here and truncates
  // it if it's too long?
  std::wstring full_name(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                               short_name));
  gtk_label_set_text(GTK_LABEL(tab_to_search_label_),
                     WideToUTF8(full_name).c_str());
}

void LocationBarViewGtk::SetKeywordHintLabel(const std::wstring& keyword) {
  if (keyword.empty())
    return;

  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  const std::wstring keyword_hint(l10n_util::GetStringF(
      IDS_OMNIBOX_KEYWORD_HINT, std::wstring(),
      GetKeywordName(profile_, keyword), &content_param_offsets));

  if (content_param_offsets.size() != 2) {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
    return;
  }

  std::string leading(WideToUTF8(
      keyword_hint.substr(0, content_param_offsets.front())));
  std::string trailing(WideToUTF8(
      keyword_hint.substr(content_param_offsets.front())));
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_leading_label_),
                     leading.c_str());
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_trailing_label_),
                     trailing.c_str());
}

void LocationBarViewGtk::ShowFirstRunBubbleInternal(bool use_OEM_bubble) {
  if (!location_entry_.get() || !widget()->window)
    return;

  gfx::Rect rect = gtk_util::GetWidgetRectRelativeToToplevel(widget());
  rect.set_width(0);
  rect.set_height(0);

  // The bubble needs to be just below the Omnibox and slightly to the right
  // of star button, so shift x and y co-ordinates.
  int y_offset = widget()->allocation.height + kFirstRunBubbleTopMargin;
  int x_offset = 0;
  if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT)
    x_offset = kFirstRunBubbleLeftMargin;
  else
    x_offset = widget()->allocation.width - kFirstRunBubbleLeftMargin;
  rect.Offset(x_offset, y_offset);

  FirstRunBubble::Show(profile_,
                       GTK_WINDOW(gtk_widget_get_toplevel(widget())),
                       rect,
                       use_OEM_bubble);
}

// static
gboolean LocationBarViewGtk::OnSecurityIconPressed(
    GtkWidget* sender,
    GdkEventButton* event,
    LocationBarViewGtk* location_bar) {
  TabContents* tab = BrowserList::GetLastActive()->GetSelectedTabContents();
  NavigationEntry* nav_entry = tab->controller().GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return true;
  }
  tab->ShowPageInfo(nav_entry->url(), nav_entry->ssl(), true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarViewGtk::PageActionViewGtk

LocationBarViewGtk::PageActionViewGtk::PageActionViewGtk(
    LocationBarViewGtk* owner, Profile* profile,
    ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      last_icon_pixbuf_(NULL) {
  event_box_.Own(gtk_event_box_new());
  gtk_widget_set_size_request(event_box_.get(),
                              Extension::kPageActionIconMaxSize,
                              Extension::kPageActionIconMaxSize);

  // Make the event box not visible so it does not paint a background.
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  g_signal_connect(event_box_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressed), this);
  g_signal_connect_after(event_box_.get(), "expose-event",
                         G_CALLBACK(OnExposeEvent), this);

  image_.Own(gtk_image_new());
  gtk_container_add(GTK_CONTAINER(event_box_.get()), image_.get());

  Extension* extension = profile->GetExtensionsService()->GetExtensionById(
      page_action->extension_id(), false);
  DCHECK(extension);

  // Load all the icons declared in the manifest. This is the contents of the
  // icons array, plus the default_icon property, if any.
  std::vector<std::string> icon_paths(*page_action->icon_paths());
  if (!page_action_->default_icon_path().empty())
    icon_paths.push_back(page_action_->default_icon_path());

  tracker_ = new ImageLoadingTracker(this, icon_paths.size());
  for (std::vector<std::string>::iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    tracker_->PostLoadImageTask(
        extension->GetResource(*iter),
        gfx::Size(Extension::kPageActionIconMaxSize,
                  Extension::kPageActionIconMaxSize));
  }
}

LocationBarViewGtk::PageActionViewGtk::~PageActionViewGtk() {
  if (tracker_)
    tracker_->StopTrackingImageLoad();
  image_.Destroy();
  event_box_.Destroy();
  for (PixbufMap::iterator iter = pixbufs_.begin(); iter != pixbufs_.end();
       ++iter) {
    g_object_unref(iter->second);
  }
  if (last_icon_pixbuf_)
    g_object_unref(last_icon_pixbuf_);
}

void LocationBarViewGtk::PageActionViewGtk::UpdateVisibility(
    TabContents* contents, GURL url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = ExtensionTabUtil::GetTabId(contents);
  current_url_ = url;

  bool visible = page_action_->GetIsVisible(current_tab_id_);
  if (visible) {
    // Set the tooltip.
    gtk_widget_set_tooltip_text(
        event_box_.get(),
        page_action_->GetTitle(current_tab_id_).c_str());

    // Set the image.
    // It can come from three places. In descending order of priority:
    // - The developer can set it dynamically by path or bitmap. It will be in
    //   page_action_->GetIcon().
    // - The developer can set it dyanmically by index. It will be in
    //   page_action_->GetIconIndex().
    // - It can be set in the manifest by path. It will be in page_action_->
    //   default_icon_path().

    // First look for a dynamically set bitmap.
    SkBitmap icon = page_action_->GetIcon(current_tab_id_);
    GdkPixbuf* pixbuf = NULL;
    if (!icon.isNull()) {
      if (icon.pixelRef() != last_icon_skbitmap_.pixelRef()) {
        if (last_icon_pixbuf_)
          g_object_unref(last_icon_pixbuf_);
        last_icon_skbitmap_ = icon;
        last_icon_pixbuf_ = gfx::GdkPixbufFromSkBitmap(&icon);
      }
      DCHECK(last_icon_pixbuf_);
      pixbuf = last_icon_pixbuf_;
    } else {
      // Otherwise look for a dynamically set index, or fall back to the
      // default path.
      int icon_index = page_action_->GetIconIndex(current_tab_id_);
      std::string icon_path;
      if (icon_index >= 0)
        icon_path = page_action_->icon_paths()->at(icon_index);
      else
        icon_path = page_action_->default_icon_path();

      if (!icon_path.empty()) {
        PixbufMap::iterator iter = pixbufs_.find(icon_path);
        if (iter != pixbufs_.end())
          pixbuf = iter->second;
      }
    }

    // The pixbuf might not be loaded yet.
    if (pixbuf)
      gtk_image_set_from_pixbuf(GTK_IMAGE(image_.get()), pixbuf);
    else
      visible = false;
  }


  bool old_visible = IsVisible();
  if (visible)
    gtk_widget_show_all(event_box_.get());
  else
    gtk_widget_hide_all(event_box_.get());

  if (visible != old_visible) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        Source<ExtensionAction>(page_action_),
        Details<TabContents>(contents));
  }
}

void LocationBarViewGtk::PageActionViewGtk::OnImageLoaded(SkBitmap* image,
                                                          size_t index) {
  // We loaded icons()->size() icons, plus one extra if the page action had
  // a default icon.
  size_t total_icons = page_action_->icon_paths()->size();
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(image);
    if (index < page_action_->icon_paths()->size())
      pixbufs_[page_action_->icon_paths()->at(index)] = pixbuf;
    else
      pixbufs_[page_action_->default_icon_path()] = pixbuf;
  }

  // If we are done, release the tracker.
  if (index == (total_icons - 1))
    tracker_ = NULL;

  owner_->UpdatePageActions();
}

void LocationBarViewGtk::PageActionViewGtk::TestActivatePageAction() {
  GdkEventButton event;
  event.button = 1;
  OnButtonPressed(widget(), &event, this);
}

// static
gboolean LocationBarViewGtk::PageActionViewGtk::OnButtonPressed(
    GtkWidget* sender,
    GdkEventButton* event,
    LocationBarViewGtk::PageActionViewGtk* page_action_view) {
  ExtensionAction* page_action = page_action_view->page_action_;
  if (page_action->has_popup()) {
    ExtensionPopupGtk::Show(
        page_action->popup_url(),
        page_action_view->owner_->browser_,
        gtk_util::GetWidgetRectRelativeToToplevel(
            page_action_view->event_box_.get()));
  } else {
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        page_action_view->profile_,
        page_action->extension_id(),
        page_action->id(),
        page_action_view->current_tab_id_,
        page_action_view->current_url_.spec(),
        event->button);
  }
  return true;
}

// static
gboolean LocationBarViewGtk::PageActionViewGtk::OnExposeEvent(
    GtkWidget* widget, GdkEventExpose* event, PageActionViewGtk* view) {
  TabContents* contents = view->owner_->browser_->GetSelectedTabContents();
  if (!contents)
    return FALSE;

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_id < 0)
    return FALSE;

  std::string badge_text = view->page_action_->GetBadgeText(tab_id);
  if (badge_text.empty())
    return FALSE;

  gfx::CanvasPaint canvas(event, false);
  gfx::Rect bounding_rect(widget->allocation);
  view->page_action_->PaintBadge(&canvas, bounding_rect, tab_id);
  return FALSE;
}
