// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/importer_lock_view.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/label.h"
#include "chrome/views/window.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;

// Default size of the dialog window.
static const int kDefaultWindowWidth = 320;
static const int kDefaultWindowHeight = 100;

ImporterLockView::ImporterLockView(ImporterHost* host)
    : description_label_(NULL),
      importer_host_(host) {
  description_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_IMPORTER_LOCK_TEXT));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  AddChildView(description_label_);
}

ImporterLockView::~ImporterLockView() {
}

void ImporterLockView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_IMPORTLOCK_DIALOG_WIDTH_CHARS,
      IDS_IMPORTLOCK_DIALOG_HEIGHT_LINES).ToSIZE();
}

void ImporterLockView::Layout() {
  description_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
      GetWidth() - 2 * kPanelHorizMargin,
      GetHeight() - 2 * kPanelVertMargin);
}

std::wstring ImporterLockView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_IMPORTER_LOCK_OK);
  } else if (button == DIALOGBUTTON_CANCEL) {
    return l10n_util::GetString(IDS_IMPORTER_LOCK_CANCEL);
  }
  return std::wstring();
}

bool ImporterLockView::IsModal() const {
  return true;
}

std::wstring ImporterLockView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_IMPORTER_LOCK_TITLE);
}

bool ImporterLockView::Accept() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, true));
  return true;
}

bool ImporterLockView::Cancel() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, false));
  return true;
}

ChromeViews::View* ImporterLockView::GetContentsView() {
  return this;
}

