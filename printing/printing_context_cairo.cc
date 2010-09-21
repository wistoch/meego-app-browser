// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>

#include "base/logging.h"

namespace printing {

PrintingContext::PrintingContext()
    :
#ifndef NDEBUG
      page_number_(-1),
#endif
      dialog_box_dismissed_(false),
      in_print_job_(false),
      abort_printing_(false) {
}

PrintingContext::~PrintingContext() {
  ResetSettings();
}

void PrintingContext::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    PrintSettingsCallback* callback) {
  NOTIMPLEMENTED();
  callback->Run(OK);
}

PrintingContext::Result PrintingContext::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();

  GtkWidget* dialog = gtk_print_unix_dialog_new(NULL, NULL);
  GtkPrintSettings* settings =
      gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPageSetup* page_setup =
      gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog));

  PageRanges ranges_vector;  // Nothing to initialize for default settings.
  settings_.Init(settings, page_setup, ranges_vector, false);

  g_object_unref(settings);
  // |page_setup| is owned by dialog, so it does not need to be unref'ed.
  gtk_widget_destroy(dialog);

  return OK;
}

PrintingContext::Result PrintingContext::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);
  settings_ = settings;

  NOTIMPLEMENTED();

  return FAILED;
}

void PrintingContext::ResetSettings() {
#ifndef NDEBUG
  page_number_ = -1;
#endif
  dialog_box_dismissed_ = false;
  abort_printing_ = false;
  in_print_job_ = false;
}

PrintingContext::Result PrintingContext::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);

  NOTIMPLEMENTED();

#ifndef NDEBUG
  page_number_ = 0;
#endif

  return FAILED;
}

PrintingContext::Result PrintingContext::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

#ifndef NDEBUG
  ++page_number_;
#endif

  return FAILED;
}

PrintingContext::Result PrintingContext::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContext::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

  ResetSettings();
  return FAILED;
}

void PrintingContext::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;

  NOTIMPLEMENTED();
}

void PrintingContext::DismissDialog() {
  NOTIMPLEMENTED();
}

PrintingContext::Result PrintingContext::OnError() {
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

}  // namespace printing
