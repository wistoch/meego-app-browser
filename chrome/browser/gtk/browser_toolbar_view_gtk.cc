// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_view_gtk.h"

#include "base/logging.h"
#include "base/base_paths_linux.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"

#include "chromium_strings.h"
#include "generated_resources.h"
#include "grit/theme_resources.h"

const int BrowserToolbarGtk::kToolbarHeight = 38;

// CustomDrawButton manages the lifetimes of some resources used to make a
// custom-drawn Gtk button.  We use them on the toolbar.
class BrowserToolbarGtk::CustomDrawButton {
 public:
  // The constructor takes 4 resource ids.  If a resource doesn't exist for a
  // button, pass in 0.
  CustomDrawButton(int normal_id,
                   int active_id,
                   int highlight_id,
                   int depressed_id);
  explicit CustomDrawButton(const std::string& filename);
  ~CustomDrawButton();

  GtkWidget* widget() const { return widget_; }

 private:
  // Load an image given a resource id.
  GdkPixbuf* LoadImage(int resource_id);

  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           CustomDrawButton* obj);

  // The actual button widget.
  GtkWidget* widget_;

  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];
};

BrowserToolbarGtk::CustomDrawButton::CustomDrawButton(int normal_id,
    int active_id, int highlight_id, int depressed_id) {
  widget_ = gtk_button_new();

  // Load the button images from the theme resources .pak file.
  pixbufs_[GTK_STATE_NORMAL] = LoadImage(normal_id);
  pixbufs_[GTK_STATE_ACTIVE] = LoadImage(active_id);
  pixbufs_[GTK_STATE_PRELIGHT] = LoadImage(highlight_id);
  pixbufs_[GTK_STATE_SELECTED] = NULL;
  pixbufs_[GTK_STATE_INSENSITIVE] = LoadImage(depressed_id);

  gtk_widget_set_size_request(widget_,
                              gdk_pixbuf_get_width(pixbufs_[0]),
                              gdk_pixbuf_get_height(pixbufs_[0]));

  gtk_widget_set_app_paintable(widget_, TRUE);
  g_signal_connect(G_OBJECT(widget_), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

BrowserToolbarGtk::CustomDrawButton::~CustomDrawButton() {
  for (size_t i = 0; i < arraysize(pixbufs_); ++i) {
    if (pixbufs_[i])
      gdk_pixbuf_unref(pixbufs_[i]);
  }
}

GdkPixbuf* BrowserToolbarGtk::CustomDrawButton::LoadImage(int resource_id) {
  if (0 == resource_id)
    return NULL;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::vector<unsigned char> data;
  rb.LoadImageResourceBytes(resource_id, &data);

  GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
  bool ok = gdk_pixbuf_loader_write(loader, static_cast<guint8*>(data.data()),
      data.size(), NULL);
  DCHECK(ok) << "failed to write " << resource_id;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader, NULL);
  DCHECK(ok) << "close failed " << resource_id;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
  DCHECK(pixbuf) << "failed to load " << resource_id << " " << data.size();

  // The pixbuf is owned by the loader, so add a ref so when we delete the
  // loader, the pixbuf still exists.
  g_object_ref(pixbuf);
  g_object_unref(loader);

  return pixbuf;
}

// static
gboolean BrowserToolbarGtk::CustomDrawButton::OnExpose(
    GtkWidget* widget,
    GdkEventExpose* e,
    CustomDrawButton* button) {
  GdkPixbuf* pixbuf = button->pixbufs_[GTK_WIDGET_STATE(widget)];

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf)
    pixbuf = button->pixbufs_[GTK_STATE_NORMAL];

  if (!pixbuf)
    return FALSE;

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);
  return TRUE;
}

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser)
    : toolbar_(NULL),
      model_(browser->toolbar_model()),
      browser_(browser),
      profile_(NULL) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  // TODO(port): Port BackForwardMenuModel
  // back_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  // forward_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
}

void BrowserToolbarGtk::Init(Profile* profile) {
  toolbar_ = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_), 4);
  // TODO(evanm): this setting of the x-size to 0 makes it so the window
  // can be resized arbitrarily small.  We should figure out what we want
  // with respect to resizing before engineering around it, though.
  gtk_widget_set_size_request(toolbar_, 0, kToolbarHeight);

  toolbar_tooltips_ = gtk_tooltips_new();

  back_.reset(BuildToolbarButton(IDR_BACK, IDR_BACK_P, IDR_BACK_H, IDR_BACK_D,
      l10n_util::GetString(IDS_TOOLTIP_BACK), false));
  forward_.reset(BuildToolbarButton(IDR_FORWARD, IDR_FORWARD_P, IDR_FORWARD_H,
      IDR_FORWARD_D, l10n_util::GetString(IDS_TOOLTIP_FORWARD), false));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new(" "), FALSE, FALSE, 0);

  reload_.reset(BuildToolbarButton(IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0,
      l10n_util::GetString(IDS_TOOLTIP_RELOAD), false));
  home_.reset(BuildToolbarButton(IDR_HOME, IDR_HOME_P, IDR_HOME_H, 0,
      l10n_util::GetString(IDS_TOOLTIP_HOME), false));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new("  "), FALSE, FALSE, 0);

  star_.reset(BuildToolbarButton(IDR_STAR, IDR_STAR_P, IDR_STAR_H, IDR_STAR_D,
      l10n_util::GetString(IDS_TOOLTIP_STAR), false));

  GtkWidget* entry = gtk_entry_new();
  gtk_widget_set_size_request(entry, 0, 27);
  gtk_box_pack_start(GTK_BOX(toolbar_), entry, TRUE, TRUE, 0);

  go_.reset(BuildToolbarButton(IDR_GO, IDR_GO_P, IDR_GO_H, 0, L"", false));

  // TODO(port): these buttons need even stranger drawing than the others.
  page_menu_button_.reset(BuildToolbarButton(IDR_MENU_PAGE, 0, 0, 0,
      l10n_util::GetString(IDS_PAGEMENU_TOOLTIP), true));

  // TODO(port): Need to get l10n_util::GetStringF working under linux to get
  // the correct string here.
  app_menu_button_.reset(BuildToolbarButton(IDR_MENU_CHROME, 0, 0, 0,
      l10n_util::GetString(IDS_APPMENU_TOOLTIP), true));

  // TODO(erg): wchar_t mismatch on linux. Fix later.
  //  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), toolbar_, FALSE, FALSE, 0);
}

void BrowserToolbarGtk::EnabledStateChangedForCommand(int id, bool enabled) {
  GtkWidget* widget = NULL;
  switch (id) {
    case IDC_BACK:
      widget = back_->widget();
      break;
    case IDC_FORWARD:
      widget = forward_->widget();
      break;
    case IDC_RELOAD:
      widget = reload_->widget();
      break;
    case IDC_HOME:
      widget = home_->widget();
      break;
    case IDC_STAR:
      widget = star_->widget();
      break;
  }
  if (widget)
    gtk_widget_set_sensitive(widget, enabled);
}

bool BrowserToolbarGtk::IsCommandEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserToolbarGtk::IsItemChecked(int id) const {
  if (!profile_)
    return false;
  if (id == IDC_SHOW_BOOKMARK_BAR)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  // TODO(port): Fix this when we get some items that want checking!
  return false;
}

void BrowserToolbarGtk::ExecuteCommand(int id) {
  browser_->ExecuteCommand(id);
}

void BrowserToolbarGtk::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  // TODO(erg): location_bar_ is a normal gtk text box right now. Change this
  // when we get omnibox support.
  //  location_bar_->SetProfile(profile);
}

// TODO(port): This needs to deal with our styled pixmaps.
BrowserToolbarGtk::CustomDrawButton* BrowserToolbarGtk::BuildToolbarButton(
    int normal_id, int active_id, int highlight_id, int depressed_id,
    const std::wstring& localized_tooltip, bool menu_button) {
  CustomDrawButton* button = new CustomDrawButton(normal_id, active_id,
      highlight_id, depressed_id);

  // TODO(erg): Mismatch between wstring and string.
  // gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tooltips_),
  //                      GTK_WIDGET(back_),
  //                      localized_tooltip, localized_tooltip);
  if (menu_button) {
    g_signal_connect(G_OBJECT(button->widget()), "button_press_event",
                     G_CALLBACK(OnMenuButtonPressEvent), this);
  } else {
    g_signal_connect(G_OBJECT(button->widget()), "clicked",
                     G_CALLBACK(OnButtonClick), this);
  }

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);
  return button;
}

/* static */
void BrowserToolbarGtk::OnButtonClick(GtkWidget* button,
                                      BrowserToolbarGtk* toolbar) {
  int tag = -1;
  if (button == toolbar->back_->widget())
    tag = IDC_BACK;
  else if (button == toolbar->forward_->widget())
    tag = IDC_FORWARD;
  else if (button == toolbar->reload_->widget())
    tag = IDC_RELOAD;
  else if (button == toolbar->home_->widget())
    tag = IDC_HOME;
  else if (button == toolbar->star_->widget())
    tag = IDC_STAR;

  DCHECK(tag != -1) << "Impossible button click callback";
  toolbar->browser_->ExecuteCommand(tag);
}

/* static */
gint BrowserToolbarGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                               GdkEvent* event,
                                               BrowserToolbarGtk* toolbar) {
  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      // We have a button press we should respond to.
      if (button == toolbar->page_menu_button_->widget()) {
        toolbar->RunPageMenu(event);
        return TRUE;
      } else if (button == toolbar->app_menu_button_->widget()) {
        toolbar->RunAppMenu(event);
        return TRUE;
      }
    }
  }

  return FALSE;
}

void BrowserToolbarGtk::RunPageMenu(GdkEvent* button_press_event) {
  if (page_menu_ == NULL) {
    page_menu_.reset(new MenuGtk(this, GetStandardPageMenu()));
  }

  page_menu_->Popup(page_menu_button_->widget(), button_press_event);
}

void BrowserToolbarGtk::RunAppMenu(GdkEvent* button_press_event) {
  if (app_menu_ == NULL) {
    app_menu_.reset(new MenuGtk(this, GetStandardAppMenu()));
  }

  app_menu_->Popup(app_menu_button_->widget(), button_press_event);
}
