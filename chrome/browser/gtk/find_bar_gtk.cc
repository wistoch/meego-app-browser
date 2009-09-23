// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "webkit/api/public/gtk/WebInputEventFactory.h"

namespace {

const GdkColor kFrameBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);
const GdkColor kTextBorderColor = GDK_COLOR_RGB(0xa6, 0xaf, 0xba);
const GdkColor kTextBorderColorAA = GDK_COLOR_RGB(0xee, 0xf4, 0xfb);
// Used as the color of the text in the entry box and the text for the results
// label for failure searches.
const GdkColor kEntryTextColor = gfx::kGdkBlack;
// Used as the color of the background of the entry box and the background of
// the find label for successful searches.
const GdkColor kEntryBackgroundColor = gfx::kGdkWhite;
const GdkColor kFindFailureBackgroundColor = GDK_COLOR_RGB(255, 102, 102);
const GdkColor kFindSuccessTextColor = GDK_COLOR_RGB(178, 178, 178);

// Padding around the container.
const int kBarPaddingTopBottom = 4;
const int kEntryPaddingLeft = 6;
const int kCloseButtonPaddingLeft = 3;
const int kBarPaddingRight = 4;

// The height of the findbar dialog, as dictated by the size of the background
// images.
const int kFindBarHeight = 32;

// The width of the text entry field.
const int kTextEntryWidth = 220;

// The size of the "rounded" corners.
const int kCornerSize = 3;

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

// Returns a list of points that either form the outline of the status bubble
// (|type| == FRAME_MASK) or form the inner border around the inner edge
// (|type| == FRAME_STROKE).
std::vector<GdkPoint> MakeFramePolygonPoints(int width,
                                             int height,
                                             FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  bool ltr = l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT;
  // If we have a stroke, we have to offset some of our points by 1 pixel.
  // We have to inset by 1 pixel when we draw horizontal lines that are on the
  // bottom or when we draw vertical lines that are closer to the end (end is
  // right for ltr).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for LTR.
  int x_off_l = ltr ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !ltr ? -y_off : 0;

  // Top left corner
  points.push_back(MakeBidiGdkPoint(x_off_r, 0, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r, kCornerSize, width, ltr));

  // Bottom left corner
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r, height - kCornerSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      (2 * kCornerSize) + x_off_l, height + y_off,
      width, ltr));

  // Bottom right corner
  points.push_back(MakeBidiGdkPoint(
      width - (2 * kCornerSize) + x_off_r, height + y_off,
      width, ltr));
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_l, height - kCornerSize, width, ltr));

  // Top right corner
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_l, kCornerSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(width + x_off_l, 0, width, ltr));

  return points;
}

// Give the findbar dialog its unique shape using images.
void SetDialogShape(GtkWidget* widget) {
  static NineBox* dialog_shape = NULL;
  if (!dialog_shape) {
    dialog_shape = new NineBox(
      IDR_FIND_DLG_LEFT_BACKGROUND,
      IDR_FIND_DLG_MIDDLE_BACKGROUND,
      IDR_FIND_DLG_RIGHT_BACKGROUND,
      NULL, NULL, NULL, NULL, NULL, NULL);
    dialog_shape->ChangeWhiteToTransparent();
  }

  dialog_shape->ContourWidget(widget);
}

// Return a ninebox that will paint the border of the findbar dialog. This is
// shared across all instances of the findbar. Do not free the returned pointer.
const NineBox* GetDialogBorder() {
  static NineBox* dialog_border = NULL;
  if (!dialog_border) {
    dialog_border = new NineBox(
      IDR_FIND_DIALOG_LEFT,
      IDR_FIND_DIALOG_MIDDLE,
      IDR_FIND_DIALOG_RIGHT,
      NULL, NULL, NULL, NULL, NULL, NULL);
  }

  return dialog_border;
}

// Like gtk_util::CreateGtkBorderBin, but allows control over the alignment and
// returns both the event box and the alignment so we can modify it during its
// lifetime (i.e. during a theme change).
void BuildBorder(GtkWidget* child,
                 bool center,
                 int padding_top, int padding_bottom, int padding_left,
                 int padding_right,
                 GtkWidget** ebox, GtkWidget** alignment) {
  *ebox = gtk_event_box_new();
  if (center)
    *alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
  else
    *alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(*alignment),
                            padding_top, padding_bottom, padding_left,
                            padding_right);
  gtk_container_add(GTK_CONTAINER(*alignment), child);
  gtk_container_add(GTK_CONTAINER(*ebox), *alignment);
}

}  // namespace

FindBarGtk::FindBarGtk(Browser* browser)
    : browser_(browser),
      window_(static_cast<BrowserWindowGtk*>(browser->window())),
      theme_provider_(GtkThemeProvider::GetFrom(browser->profile())),
      container_width_(-1),
      container_height_(-1),
      match_label_failure_(false),
      ignore_changed_signal_(false),
      current_fixed_width_(-1) {
  InitWidgets();
  ViewIDUtil::SetID(text_entry_, VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);

  // Insert the widget into the browser gtk hierarchy.
  window_->AddFindBar(this);

  // Hook up signals after the widget has been added to the hierarchy so the
  // widget will be realized.
  g_signal_connect(text_entry_, "changed",
                   G_CALLBACK(OnChanged), this);
  g_signal_connect(text_entry_, "key-press-event",
                   G_CALLBACK(OnKeyPressEvent), this);
  g_signal_connect(text_entry_, "key-release-event",
                   G_CALLBACK(OnKeyReleaseEvent), this);
  // When the user tabs to us or clicks on us, save where the focus used to
  // be.
  g_signal_connect(text_entry_, "focus",
                   G_CALLBACK(OnFocus), this);
  gtk_widget_add_events(text_entry_, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(text_entry_, "button-press-event",
                   G_CALLBACK(OnButtonPress), this);
  g_signal_connect(widget(), "size-allocate",
                   G_CALLBACK(OnFixedSizeAllocate), this);
  g_signal_connect(container_, "expose-event",
                   G_CALLBACK(OnExpose), this);
}

FindBarGtk::~FindBarGtk() {
  fixed_.Destroy();
}

void FindBarGtk::InitWidgets() {
  // The find bar is basically an hbox with a gtkentry (text box) followed by 3
  // buttons (previous result, next result, close).  We wrap the hbox in a gtk
  // alignment and a gtk event box to get the padding and light blue
  // background. We put that event box in a fixed in order to control its
  // lateral position. We put that fixed in a SlideAnimatorGtk in order to get
  // the slide effect.
  GtkWidget* hbox = gtk_hbox_new(false, 0);
  container_ = gtk_util::CreateGtkBorderBin(hbox, NULL,
      kBarPaddingTopBottom, kBarPaddingTopBottom,
      kEntryPaddingLeft, kBarPaddingRight);
  ViewIDUtil::SetID(container_, VIEW_ID_FIND_IN_PAGE);
  gtk_widget_set_app_paintable(container_, TRUE);

  slide_widget_.reset(new SlideAnimatorGtk(container_,
                                           SlideAnimatorGtk::DOWN,
                                           0, false, false, NULL));

  // |fixed_| has to be at least one pixel tall. We color this pixel the same
  // color as the border that separates the toolbar from the tab contents.
  fixed_.Own(gtk_fixed_new());
  border_ = gtk_event_box_new();
  gtk_widget_set_size_request(border_, 1, 1);

  gtk_fixed_put(GTK_FIXED(widget()), border_, 0, 0);
  gtk_fixed_put(GTK_FIXED(widget()), slide_widget(), 0, 0);
  gtk_widget_set_size_request(widget(), -1, 0);

  close_button_.reset(CustomDrawButton::CloseButton(theme_provider_));
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true,
                               kCloseButtonPaddingLeft);
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(close_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP).c_str());

  find_next_button_.reset(new CustomDrawButton(theme_provider_,
      IDR_FINDINPAGE_NEXT, IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_H,
      IDR_FINDINPAGE_NEXT_P, GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU));
  g_signal_connect(G_OBJECT(find_next_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(find_next_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_NEXT_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_next_button_->widget(),
                   FALSE, FALSE, 0);

  find_previous_button_.reset(new CustomDrawButton(theme_provider_,
      IDR_FINDINPAGE_PREV, IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_H,
      IDR_FINDINPAGE_PREV_P, GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
  g_signal_connect(G_OBJECT(find_previous_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(find_previous_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_previous_button_->widget(),
                   FALSE, FALSE, 0);

  // Make a box for the edit and match count widgets. This is fixed size since
  // we want the widgets inside to resize themselves rather than making the
  // dialog bigger.
  GtkWidget* content_hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_size_request(content_hbox, kTextEntryWidth, -1);

  text_entry_ = gtk_entry_new();
  gtk_entry_set_has_frame(GTK_ENTRY(text_entry_), FALSE);

  match_count_label_ = gtk_label_new(NULL);
  // This line adds padding on the sides so that the label has even padding on
  // all edges.
  gtk_misc_set_padding(GTK_MISC(match_count_label_), 2, 0);
  match_count_event_box_ = gtk_event_box_new();
  GtkWidget* match_count_centerer = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(match_count_centerer), match_count_event_box_,
                     TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(match_count_centerer), 1);
  gtk_container_add(GTK_CONTAINER(match_count_event_box_), match_count_label_);

  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(text_entry_, 13.4);  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(match_count_centerer, 13.4);

  gtk_box_pack_end(GTK_BOX(content_hbox), match_count_centerer,
                   FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(content_hbox), text_entry_, TRUE, TRUE, 0);

  // This event box is necessary to color in the area above and below the match
  // count label, and is where we draw the entry background onto in GTK mode.
  BuildBorder(content_hbox, true, 0, 0, 0, 0,
              &content_event_box_, &content_alignment_);
  gtk_widget_set_app_paintable(content_event_box_, TRUE);
  g_signal_connect(content_event_box_, "expose-event",
                   G_CALLBACK(OnContentEventBoxExpose), this);

  // We fake anti-aliasing by having two borders.
  BuildBorder(content_event_box_, false, 1, 1, 1, 0,
              &border_bin_, &border_bin_alignment_);
  BuildBorder(border_bin_, false, 1, 1, 1, 0,
              &border_bin_aa_, &border_bin_aa_alignment_);
  gtk_util::CenterWidgetInHBox(hbox, border_bin_aa_, true, 0);

  theme_provider_->InitThemesFor(this);
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());

  // We take care to avoid showing the slide animator widget.
  gtk_widget_show_all(container_);
  gtk_widget_show(widget());
  gtk_widget_show(border_);
}

GtkWidget* FindBarGtk::slide_widget() {
  return slide_widget_->widget();
}

void FindBarGtk::Show() {
  slide_widget_->Open();
  Reposition();
  if (container_->window)
    gdk_window_raise(container_->window);
}

void FindBarGtk::Hide(bool animate) {
  if (animate)
    slide_widget_->Close();
  else
    slide_widget_->CloseWithoutAnimation();
}

void FindBarGtk::SetFocusAndSelection() {
  StoreOutsideFocus();
  gtk_widget_grab_focus(text_entry_);
  // Select all the text.
  gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
}

void FindBarGtk::ClearResults(const FindNotificationDetails& results) {
  UpdateUIForFindResult(results, string16());
}

void FindBarGtk::StopAnimation() {
  slide_widget_->End();
}

void FindBarGtk::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // Not moving the window on demand, so do nothing.
}

void FindBarGtk::SetFindText(const string16& find_text) {
  std::string text_entry_utf8 = UTF16ToUTF8(find_text);

  // Ignore the "changed" signal handler because programatically setting the
  // text should not fire a "changed" event.
  ignore_changed_signal_ = true;
  gtk_entry_set_text(GTK_ENTRY(text_entry_), text_entry_utf8.c_str());
  ignore_changed_signal_ = false;
}

void FindBarGtk::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  if (!result.selection_rect().IsEmpty()) {
    int xposition = GetDialogPosition(result.selection_rect()).x();
    if (xposition != slide_widget()->allocation.x)
      gtk_fixed_move(GTK_FIXED(widget()), slide_widget(), xposition, 0);
  }

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_store_.Store(NULL);

  std::string text_entry_utf8 = UTF16ToUTF8(find_text);
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // If we don't have any results and something was passed in, then that means
  // someone pressed F3 while the Find box was closed. In that case we need to
  // repopulate the Find box with what was passed in.
  std::string search_string(gtk_entry_get_text(GTK_ENTRY(text_entry_)));
  if (search_string.empty() && !text_entry_utf8.empty()) {
    SetFindText(find_text);
    gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
  }

  if (!search_string.empty() && have_valid_range) {
    gtk_label_set_text(GTK_LABEL(match_count_label_),
        l10n_util::GetStringFUTF8(IDS_FIND_IN_PAGE_COUNT,
            IntToString16(result.active_match_ordinal()),
            IntToString16(result.number_of_matches())).c_str());
    UpdateMatchLabelAppearance(result.number_of_matches() == 0);
  } else {
    // If there was no text entered, we don't show anything in the result count
    // area.
    gtk_label_set_text(GTK_LABEL(match_count_label_), "");
    UpdateMatchLabelAppearance(false);
  }

  // TODO(brettw) enable or disable the find next/previous buttons depending
  // on whether any matches were found.
}

void FindBarGtk::AudibleAlert() {
  gtk_widget_error_bell(widget());
}

gfx::Rect FindBarGtk::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // TODO(estade): Logic for the positioning of the find bar might do better
  // to share more code with Windows. Currently though they do some things we
  // don't worry about, such as considering the state of the bookmark bar on
  // the NTP. I've tried to stick as close to the windows function as possible
  // here to make it easy to possibly unfork this down the road.

  bool ltr = l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT;
  // 15 is the size of the scrollbar, copied from ScrollbarThemeChromium.
  // The height is not used.
  // At very low browser widths we can wind up with a negative |dialog_bounds|
  // width, so clamp it to 0.
  gfx::Rect dialog_bounds = gfx::Rect(ltr ? 0 : 15, 0,
                                      std::max(0, widget()->allocation.width -
                                                  (ltr ? 15 : 0)),
                                      0);

  GtkRequisition req;
  gtk_widget_size_request(container_, &req);
  gfx::Size prefsize(req.width, req.height);

  gfx::Rect view_location(
      ltr ? dialog_bounds.width() - prefsize.width() : dialog_bounds.x(),
      dialog_bounds.y(), prefsize.width(), prefsize.height());
  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_location, dialog_bounds, avoid_overlapping_rect);

  return new_pos;
}

void FindBarGtk::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
  gtk_fixed_move(GTK_FIXED(widget()), slide_widget(), new_pos.x(), 0);
  slide_widget_->OpenWithoutAnimation();
}

bool FindBarGtk::IsFindBarVisible() {
  return GTK_WIDGET_VISIBLE(slide_widget());
}

void FindBarGtk::RestoreSavedFocus() {
  // This function sometimes gets called when we don't have focus. We should do
  // nothing in this case.
  if (!gtk_widget_is_focus(text_entry_))
    return;

  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    find_bar_controller_->tab_contents()->Focus();
}

FindBarTesting* FindBarGtk::GetFindBarTesting() {
  return this;
}

void FindBarGtk::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::BROWSER_THEME_CHANGED);

  // Force reshapings of the find bar window.
  container_width_ = -1;
  container_height_ = -1;
  current_fixed_width_ = -1;

  if (theme_provider_->UseGtkTheme()) {
    GdkColor color = theme_provider_->GetBorderColor();
    gtk_widget_modify_bg(border_, GTK_STATE_NORMAL, &color);

    gtk_widget_modify_base(text_entry_, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_text(text_entry_, GTK_STATE_NORMAL, NULL);

    gtk_widget_set_size_request(content_event_box_, -1, -1);
    gtk_widget_modify_bg(content_event_box_, GTK_STATE_NORMAL, NULL);

    // Replicate the normal GtkEntry behaviour by drawing the entry
    // background. We set the fake alignment to be the frame thickness.
    GtkStyle* style = gtk_rc_get_style(text_entry_);
    gint xborder = style->xthickness;
    gint yborder = style->ythickness;
    gtk_alignment_set_padding(GTK_ALIGNMENT(content_alignment_),
                              yborder, yborder, xborder, xborder);

    gtk_widget_modify_bg(border_bin_, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_bg(border_bin_aa_, GTK_STATE_NORMAL, NULL);

    // We leave left padding on the left, even in GTK mode, as it's required
    // for the left margin to be equivalent to the bottom margin.
    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_alignment_),
                              0, 0, 1, 0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_aa_alignment_),
                              0, 0, 0, 0);

    gtk_misc_set_alignment(GTK_MISC(match_count_label_), 0.5, 0.5);
  } else {
    gtk_widget_modify_bg(border_, GTK_STATE_NORMAL, &kFrameBorderColor);

    gtk_widget_modify_base(text_entry_, GTK_STATE_NORMAL,
                           &kEntryBackgroundColor);
    gtk_widget_modify_text(text_entry_, GTK_STATE_NORMAL,
                           &kEntryTextColor);

    // Force the text widget height so it lines up with the buttons regardless
    // of font size.
    gtk_widget_set_size_request(content_event_box_, -1, 20);
    gtk_widget_modify_bg(content_event_box_, GTK_STATE_NORMAL,
                         &kEntryBackgroundColor);

    gtk_alignment_set_padding(GTK_ALIGNMENT(content_alignment_),
                              0.0, 0.0, 0.0, 0.0);

    gtk_widget_modify_bg(border_bin_, GTK_STATE_NORMAL, &kTextBorderColor);
    gtk_widget_modify_bg(border_bin_aa_, GTK_STATE_NORMAL, &kTextBorderColorAA);

    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_alignment_),
                              1, 1, 1, 0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_aa_alignment_),
                              1, 1, 1, 0);

    gtk_misc_set_alignment(GTK_MISC(match_count_label_), 0.5, 1.0);
  }

  UpdateMatchLabelAppearance(match_label_failure_);
}

bool FindBarGtk::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  NOTIMPLEMENTED();
  return false;
}

void FindBarGtk::FindEntryTextInContents(bool forward_search) {
  TabContents* tab_contents = find_bar_controller_->tab_contents();
  if (!tab_contents)
    return;

  std::string new_contents(gtk_entry_get_text(GTK_ENTRY(text_entry_)));

  if (new_contents.length() > 0) {
    tab_contents->StartFinding(UTF8ToUTF16(new_contents), forward_search,
                               false);  // Not case sensitive.
  } else {
    // The textbox is empty so we reset.
    tab_contents->StopFinding(true);  // true = clear selection on page.
    UpdateUIForFindResult(find_bar_controller_->tab_contents()->find_result(),
                          string16());
  }
}

void FindBarGtk::UpdateMatchLabelAppearance(bool failure) {
  match_label_failure_ = failure;
  bool use_gtk = theme_provider_->UseGtkTheme();

  if (use_gtk) {
    GtkStyle* style = gtk_rc_get_style(text_entry_);
    GdkColor normal_bg = style->base[GTK_STATE_NORMAL];
    GdkColor normal_text = gtk_util::AverageColors(
        style->text[GTK_STATE_NORMAL], style->base[GTK_STATE_NORMAL]);

    gtk_widget_modify_bg(match_count_event_box_, GTK_STATE_NORMAL,
                         failure ? &kFindFailureBackgroundColor :
                         &normal_bg);
    gtk_widget_modify_fg(match_count_label_, GTK_STATE_NORMAL,
                         failure ? &kEntryTextColor : &normal_text);
  } else {
    gtk_widget_modify_bg(match_count_event_box_, GTK_STATE_NORMAL,
                         failure ? &kFindFailureBackgroundColor :
                         &kEntryBackgroundColor);
    gtk_widget_modify_fg(match_count_label_, GTK_STATE_NORMAL,
                         failure ? &kEntryTextColor : &kFindSuccessTextColor);
  }
}

void FindBarGtk::Reposition() {
  if (!IsFindBarVisible())
    return;

  int xposition = GetDialogPosition(gfx::Rect()).x();
  if (xposition == slide_widget()->allocation.x) {
    return;
  } else {
    gtk_fixed_move(GTK_FIXED(widget()), slide_widget(), xposition, 0);
  }
}

void FindBarGtk::StoreOutsideFocus() {
  // |text_entry_| is the only widget in the find bar that can be focused,
  // so it's the only one we have to check.
  // TODO(estade): when we make the find bar buttons focusable, we'll have
  // to change this (same above in RestoreSavedFocus).
  if (!gtk_widget_is_focus(text_entry_))
    focus_store_.Store(text_entry_);
}

bool FindBarGtk::MaybeForwardKeyEventToRenderer(GdkEventKey* event) {
  switch (event->keyval) {
    case GDK_Down:
    case GDK_Up:
    case GDK_Page_Up:
    case GDK_Page_Down:
      break;
    case GDK_Home:
    case GDK_End:
      if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
          GDK_CONTROL_MASK) {
        break;
      }
    // Fall through.
    default:
      return false;
  }

  TabContents* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();

  NativeWebKeyboardEvent wke(event);
  render_view_host->ForwardKeyboardEvent(wke);
  return true;
}

// static
gboolean FindBarGtk::OnChanged(GtkWindow* window, FindBarGtk* find_bar) {
  if (!find_bar->ignore_changed_signal_)
    find_bar->FindEntryTextInContents(true);
  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyPressEvent(GtkWidget* widget, GdkEventKey* event,
                                     FindBarGtk* find_bar) {
  if (find_bar->MaybeForwardKeyEventToRenderer(event)) {
    return TRUE;
  } else if (GDK_Escape == event->keyval) {
    find_bar->find_bar_controller_->EndFindSession();
    return TRUE;
  } else if (GDK_Return == event->keyval ||
             GDK_KP_Enter == event->keyval) {
    bool forward = (event->state & gtk_accelerator_get_default_mod_mask()) !=
                   GDK_SHIFT_MASK;
    find_bar->FindEntryTextInContents(forward);
    return TRUE;
  }
  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       FindBarGtk* find_bar) {
  return find_bar->MaybeForwardKeyEventToRenderer(event);
}

// static
void FindBarGtk::OnClicked(GtkWidget* button, FindBarGtk* find_bar) {
  if (button == find_bar->close_button_->widget()) {
    find_bar->find_bar_controller_->EndFindSession();
  } else if (button == find_bar->find_previous_button_->widget() ||
             button == find_bar->find_next_button_->widget()) {
    find_bar->FindEntryTextInContents(
        button == find_bar->find_next_button_->widget());
  } else {
    NOTREACHED();
  }
}

// static
gboolean FindBarGtk::OnContentEventBoxExpose(GtkWidget* widget,
                                             GdkEventExpose* event,
                                             FindBarGtk* bar) {
  if (bar->theme_provider_->UseGtkTheme()) {
    // Draw the text entry background around where we input stuff.
    GdkRectangle rec = {
      widget->allocation.x,
      widget->allocation.y,
      widget->allocation.width,
      widget->allocation.height
    };

    gtk_util::DrawTextEntryBackground(bar->text_entry_, widget,
                                      &event->area, &rec);
  }

  return FALSE;
}

// static
void FindBarGtk::OnFixedSizeAllocate(GtkWidget* fixed,
                                     GtkAllocation* allocation,
                                     FindBarGtk* findbar) {
  // Do nothing if our width hasn't changed.
  if (findbar->current_fixed_width_ == allocation->width)
    return;
  findbar->current_fixed_width_ = allocation->width;

  // Set the background widget to the size of |fixed|.
  gtk_widget_set_size_request(findbar->border_,
                              allocation->width, allocation->height);

  findbar->Reposition();
}

// Used to handle custom painting of |container_|.
gboolean FindBarGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                              FindBarGtk* bar) {
  GtkRequisition req;
  gtk_widget_size_request(widget, &req);
  gtk_widget_set_size_request(bar->slide_widget(), req.width, -1);

  if (bar->theme_provider_->UseGtkTheme()) {
    if (bar->container_width_ != widget->allocation.width ||
        bar->container_height_ != widget->allocation.height) {
      std::vector<GdkPoint> mask_points = MakeFramePolygonPoints(
          widget->allocation.width, widget->allocation.height, FRAME_MASK);
      GdkRegion* mask_region = gdk_region_polygon(&mask_points[0],
                                                  mask_points.size(),
                                                  GDK_EVEN_ODD_RULE);
      // Reset the shape.
      gdk_window_shape_combine_region(widget->window, NULL, 0, 0);
      gdk_window_shape_combine_region(widget->window, mask_region, 0, 0);
      gdk_region_destroy(mask_region);

      bar->container_width_ = widget->allocation.width;
      bar->container_height_ = widget->allocation.height;
    }

    GdkDrawable* drawable = GDK_DRAWABLE(e->window);
    GdkGC* gc = gdk_gc_new(drawable);
    gdk_gc_set_clip_rectangle(gc, &e->area);
    GdkColor color = bar->theme_provider_->GetBorderColor();
    gdk_gc_set_rgb_fg_color(gc, &color);

    // Stroke the frame border.
    std::vector<GdkPoint> points = MakeFramePolygonPoints(
        widget->allocation.width, widget->allocation.height, FRAME_STROKE);
    gdk_draw_lines(drawable, gc, &points[0], points.size());

    g_object_unref(gc);
  } else {
    if (bar->container_width_ != widget->allocation.width ||
        bar->container_height_ != widget->allocation.height) {
      // Reset the shape.
      gdk_window_shape_combine_region(widget->window, NULL, 0, 0);
      SetDialogShape(bar->container_);

      bar->container_width_ = widget->allocation.width;
      bar->container_height_ = widget->allocation.height;
    }

    // Draw the background theme image.
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    cairo_rectangle(cr, e->area.x, e->area.y, e->area.width, e->area.height);
    cairo_clip(cr);
    gfx::Point tabstrip_origin =
        bar->window_->tabstrip()->GetTabStripOriginForWidget(widget);
    CairoCachedSurface* background = bar->theme_provider_->GetSurfaceNamed(
        IDR_THEME_TOOLBAR, widget);
    background->SetSource(cr, tabstrip_origin.x(), tabstrip_origin.y());
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, tabstrip_origin.x(), tabstrip_origin.y(),
                        e->area.x + e->area.width - tabstrip_origin.x(),
                        background->Height());
    cairo_fill(cr);
    cairo_destroy(cr);

    // Draw the border.
    GetDialogBorder()->RenderToWidget(widget);
  }

  // Propagate to the container's child.
  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);
  return TRUE;
}

// static
gboolean FindBarGtk::OnFocus(GtkWidget* text_entry, GtkDirectionType focus,
                             FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}

// static
gboolean FindBarGtk::OnButtonPress(GtkWidget* text_entry, GdkEventButton* e,
                                   FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}
