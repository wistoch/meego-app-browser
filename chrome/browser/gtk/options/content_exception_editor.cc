// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/content_exception_editor.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/common/gtk_util.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

namespace {

// The settings shown in the combobox if show_ask_ is false;
static const ContentSetting kNoAskSettings[] = { CONTENT_SETTING_ALLOW,
                                                 CONTENT_SETTING_BLOCK };

// The settings shown in the combobox if show_ask_ is true;
static const ContentSetting kAskSettings[] = { CONTENT_SETTING_ALLOW,
                                               CONTENT_SETTING_ASK,
                                               CONTENT_SETTING_BLOCK };

// Returns true if the host name is valid.
bool ValidHost(const std::string& host) {
  if (host.empty())
    return false;

  url_canon::CanonHostInfo host_info;
  return !net::CanonicalizeHost(host, &host_info).empty();
}

}  // namespace

ContentExceptionEditor::ContentExceptionEditor(
    GtkWindow* parent,
    ContentExceptionEditor::Delegate* delegate,
    ContentExceptionsTableModel* model,
    int index,
    const std::string& host,
    ContentSetting setting)
    : delegate_(delegate),
      model_(model),
      show_ask_(model->content_type() == CONTENT_SETTINGS_TYPE_COOKIES),
      index_(index),
      host_(host),
      setting_(setting) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(is_new() ?
                               IDS_EXCEPTION_EDITOR_NEW_TITLE :
                               IDS_EXCEPTION_EDITOR_TITLE).c_str(),
      parent,
      // Non-modal.
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);

  entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_), host_.c_str());
  g_signal_connect(entry_, "changed", G_CALLBACK(OnEntryChanged), this);
  gtk_entry_set_activates_default(GTK_ENTRY(entry_), TRUE);

  action_combo_ = gtk_combo_box_new_text();
  for (int i = 0; i < GetItemCount(); ++i) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(action_combo_),
                              GetTitleFor(i).c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(action_combo_),
                           IndexForSetting(setting_));

  GtkWidget* table = gtk_util::CreateLabeledControlsGroup(
      NULL,
      l10n_util::GetStringUTF8(IDS_EXCEPTION_EDITOR_HOST_TITLE).c_str(),
      entry_,
      l10n_util::GetStringUTF8(IDS_EXCEPTION_EDITOR_ACTION_TITLE).c_str(),
      action_combo_,
      NULL);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), table);

  // Prime the state of the buttons.
  OnEntryChanged(GTK_EDITABLE(entry_), this);

  gtk_widget_show_all(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

int ContentExceptionEditor::GetItemCount() {
  return show_ask_ ? arraysize(kAskSettings) : arraysize(kNoAskSettings);
}

std::string ContentExceptionEditor::GetTitleFor(int index) {
  switch (SettingForIndex(index)) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ALLOW_BUTTON);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringUTF8(IDS_EXCEPTIONS_BLOCK_BUTTON);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ASK_BUTTON);
    default:
      NOTREACHED();
  }
  return std::string();
}

ContentSetting ContentExceptionEditor::SettingForIndex(int index) {
  return show_ask_ ? kAskSettings[index] : kNoAskSettings[index];
}

int ContentExceptionEditor::IndexForSetting(ContentSetting setting) {
  for (int i = 0; i < GetItemCount(); ++i)
    if (SettingForIndex(i) == setting)
      return i;
  NOTREACHED();
  return 0;
}

// static
void ContentExceptionEditor::OnEntryChanged(GtkEditable* entry,
                                            ContentExceptionEditor* window) {
  bool can_accept = false;
  std::string new_host = gtk_entry_get_text(GTK_ENTRY(window->entry_));
  if (window->is_new()) {
    can_accept = ValidHost(new_host) &&
                 (window->model_->IndexOfExceptionByHost(new_host) == -1);
  } else {
    can_accept = !new_host.empty() &&
        (window->host_ == new_host ||
         (ValidHost(new_host) &&
          window->model_->IndexOfExceptionByHost(new_host) == -1));
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(window->dialog_),
                                    GTK_RESPONSE_OK, can_accept);
}

// static
void ContentExceptionEditor::OnResponse(
    GtkWidget* sender,
    int response_id,
    ContentExceptionEditor* window) {
  if (response_id == GTK_RESPONSE_OK) {
    // Notify our delegate to update everything.
    std::string new_host = gtk_entry_get_text(GTK_ENTRY(window->entry_));
    ContentSetting setting = window->SettingForIndex(gtk_combo_box_get_active(
        GTK_COMBO_BOX(window->action_combo_)));
    window->delegate_->AcceptExceptionEdit(new_host, setting, window->index_,
                                           window->is_new());
  }

  gtk_widget_destroy(window->dialog_);
}

// static
void ContentExceptionEditor::OnWindowDestroy(
    GtkWidget* widget,
    ContentExceptionEditor* editor) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, editor);
}
