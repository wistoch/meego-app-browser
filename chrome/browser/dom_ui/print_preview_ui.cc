// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/print_preview_ui.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

void SetLocalizedStrings(DictionaryValue* localized_strings) {
  localized_strings->SetString(std::string("title"),
      l10n_util::GetStringUTF8(IDS_PRINTPREVIEW_TITLE));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// PrintPreviewUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

PrintPreviewUIHTMLSource::PrintPreviewUIHTMLSource()
    : DataSource(chrome::kChromeUIPrintHost, MessageLoop::current()) {
}

PrintPreviewUIHTMLSource::~PrintPreviewUIHTMLSource() {}

void PrintPreviewUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  DictionaryValue localized_strings;
  SetLocalizedStrings(&localized_strings);
  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece print_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PRINTPREVIEW_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      print_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string PrintPreviewUIHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// PrintPreviewUI
//
////////////////////////////////////////////////////////////////////////////////

PrintPreviewUI::PrintPreviewUI(TabContents* contents) : DOMUI(contents) {
  // Set up the chrome://print/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new PrintPreviewUIHTMLSource())));
}

PrintPreviewUI::~PrintPreviewUI() {
}
