// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/customize_sync_window_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "app/gtk_signal.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk
//
// The contents of the Customize Sync dialog window.

class CustomizeSyncWindowGtk {
 public:
  explicit CustomizeSyncWindowGtk(Profile* profile);
  ~CustomizeSyncWindowGtk();

  void Show();
  bool ClickOk();
  void ClickCancel();

 private:
  // The pixel width we wrap labels at.
  static const int kWrapWidth = 475;

  GtkWidget* AddCheckbox(GtkWidget* parent, int label_id, bool checked);
  bool Accept();

  static void OnWindowDestroy(GtkWidget* widget,
                              CustomizeSyncWindowGtk* window);

  static void OnResponse(GtkDialog* dialog, gint response_id,
                         CustomizeSyncWindowGtk* customize_sync_window);

  CHROMEGTK_CALLBACK_0(CustomizeSyncWindowGtk, void, OnCheckboxClicked);

  // The customize sync dialog.
  GtkWidget *dialog_;

  Profile* profile_;

  GtkWidget* description_label_;
  GtkWidget* bookmarks_check_box_;
  GtkWidget* preferences_check_box_;
  GtkWidget* themes_check_box_;
  GtkWidget* autofill_check_box_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(CustomizeSyncWindowGtk);
};

// The singleton customize sync window object.
static CustomizeSyncWindowGtk* customize_sync_window = NULL;

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk, public:

CustomizeSyncWindowGtk::CustomizeSyncWindowGtk(Profile* profile)
    : profile_(profile),
      description_label_(NULL),
      bookmarks_check_box_(NULL),
      preferences_check_box_(NULL),
      themes_check_box_(NULL),
      autofill_check_box_(NULL) {
  syncable::ModelTypeSet registered_types;
  profile_->GetProfileSyncService()->GetRegisteredDataTypes(&registered_types);
  syncable::ModelTypeSet preferred_types;
  profile_->GetProfileSyncService()->GetPreferredDataTypes(&preferred_types);

  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_CUSTOMIZE_SYNC_WINDOW_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      // Customize sync window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  description_label_ = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_CUSTOMIZE_SYNC_DESCRIPTION).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(description_label_), TRUE);
  gtk_widget_set_size_request(description_label_, kWrapWidth, -1);
  gtk_box_pack_start(GTK_BOX(vbox), description_label_, FALSE, FALSE, 0);

  accessible_widget_helper_->SetWidgetName(description_label_,
      IDS_CUSTOMIZE_SYNC_DESCRIPTION);

  DCHECK(registered_types.count(syncable::BOOKMARKS));
  bool bookmarks_checked = preferred_types.count(syncable::BOOKMARKS) != 0;
  bookmarks_check_box_ = AddCheckbox(vbox, IDS_SYNC_DATATYPE_BOOKMARKS,
                                     bookmarks_checked);

  if (registered_types.count(syncable::PREFERENCES)) {
    bool prefs_checked = preferred_types.count(syncable::PREFERENCES) != 0;
    preferences_check_box_ = AddCheckbox(vbox, IDS_SYNC_DATATYPE_PREFERENCES,
                                         prefs_checked);
  }

  if (registered_types.count(syncable::THEMES)) {
    bool themes_checked = preferred_types.count(syncable::THEMES) != 0;
    themes_check_box_ = AddCheckbox(vbox, IDS_SYNC_DATATYPE_THEMES,
                                    themes_checked);
  }

  if (registered_types.count(syncable::AUTOFILL)) {
    bool autofill_checked = preferred_types.count(syncable::AUTOFILL) != 0;
    autofill_check_box_ = AddCheckbox(vbox, IDS_SYNC_DATATYPE_AUTOFILL,
      autofill_checked);
  }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), vbox, FALSE, FALSE, 0);

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_CUSTOMIZE_SYNC_DIALOG_WIDTH_CHARS,
                                       IDS_CUSTOMIZE_SYNC_DIALOG_HEIGHT_LINES,
                                       true);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  gtk_widget_show_all(dialog_);
}

CustomizeSyncWindowGtk::~CustomizeSyncWindowGtk() {
}

void CustomizeSyncWindowGtk::Show() {
  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_window_present(GTK_WINDOW(dialog_));
}

bool CustomizeSyncWindowGtk::ClickOk() {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bookmarks_check_box_)) ||
      (preferences_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
           preferences_check_box_))) ||
      (themes_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(themes_check_box_))) ||
      (autofill_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autofill_check_box_)))) {
    Accept();
    gtk_widget_destroy(GTK_WIDGET(dialog_));
    return true;
  } else {
    // show the user that something's wrong with this dialog (not perfect, but
    // a temporary fix)
    gtk_window_present(GTK_WINDOW(dialog_));
    return false;
  }
}

void CustomizeSyncWindowGtk::ClickCancel() {
  gtk_widget_destroy(GTK_WIDGET(dialog_));
}

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk, private:

GtkWidget* CustomizeSyncWindowGtk::AddCheckbox(GtkWidget* parent, int label_id,
                                               bool checked) {

  GtkWidget* checkbox = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(label_id).c_str());

  gtk_box_pack_start(GTK_BOX(parent), checkbox, FALSE, FALSE, 0);
  accessible_widget_helper_->SetWidgetName(checkbox, label_id);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), checked);

  g_signal_connect(checkbox, "clicked", G_CALLBACK(OnCheckboxClickedThunk),
                   this);

  return checkbox;
}

bool CustomizeSyncWindowGtk::Accept() {
  syncable::ModelTypeSet preferred_types;

  bool bookmarks_enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(bookmarks_check_box_));
  if (bookmarks_enabled) {
    preferred_types.insert(syncable::BOOKMARKS);
  }

  if (preferences_check_box_) {
    bool preferences_enabled = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(preferences_check_box_));
    if (preferences_enabled) {
      preferred_types.insert(syncable::PREFERENCES);
    }
  }
  if (themes_check_box_) {
    bool themes_enabled = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(themes_check_box_));
    if (themes_enabled) {
      preferred_types.insert(syncable::THEMES);
    }
  }
  if (autofill_check_box_) {
    bool autofill_enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(autofill_check_box_));
    if (autofill_enabled) {
      preferred_types.insert(syncable::AUTOFILL);
    }
  }

  profile_->GetProfileSyncService()->ChangePreferredDataTypes(preferred_types);
  return true;
}

// static
void CustomizeSyncWindowGtk::OnWindowDestroy(GtkWidget* widget,
                                             CustomizeSyncWindowGtk* window) {
  customize_sync_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

// static
void CustomizeSyncWindowGtk::OnResponse(
    GtkDialog* dialog, gint response_id,
    CustomizeSyncWindowGtk* customize_sync_window) {
  if (response_id == GTK_RESPONSE_OK) {
    customize_sync_window->ClickOk();
  } else if (response_id == GTK_RESPONSE_CANCEL) {
    customize_sync_window->ClickCancel();
  }
}

// Deactivate the "OK" button if you uncheck all the data types.
void CustomizeSyncWindowGtk::OnCheckboxClicked(GtkWidget* widget) {
  bool any_datatypes_selected =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bookmarks_check_box_)) ||
      (preferences_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
           preferences_check_box_))) ||
      (themes_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(themes_check_box_))) ||
      (autofill_check_box_ &&
       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autofill_check_box_)));
  if (any_datatypes_selected) {
    gtk_dialog_set_response_sensitive(
        GTK_DIALOG(customize_sync_window->dialog_), GTK_RESPONSE_OK, TRUE);
  } else {
    gtk_dialog_set_response_sensitive(
        GTK_DIALOG(customize_sync_window->dialog_), GTK_RESPONSE_OK, FALSE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowCustomizeSyncWindow(Profile* profile) {
  DCHECK(profile);
  // If there's already an existing window, use it.
  if (!customize_sync_window) {
    customize_sync_window = new CustomizeSyncWindowGtk(profile);
  }
  customize_sync_window->Show();
}

bool CustomizeSyncWindowOk() {
  if (customize_sync_window) {
    return customize_sync_window->ClickOk();
  } else {
    return true;
  }
}

void CustomizeSyncWindowCancel() {
  if (customize_sync_window) {
    customize_sync_window->ClickCancel();
  }
}
