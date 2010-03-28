// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/bookmarks_ui.h"

#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"

////////////////////////////////////////////////////////////////////////////////
//
// BookmarksUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

BookmarksUIHTMLSource::BookmarksUIHTMLSource()
    : DataSource(chrome::kChromeUIBookmarksHost, MessageLoop::current()) {
}

void BookmarksUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_off_the_record,
                                             int request_id) {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";
}

std::string BookmarksUIHTMLSource::GetMimeType(const std::string& path) const {
    NOTREACHED() << "We should never get here since the extension should have"
                 << "been triggered";
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// BookmarksUI
//
////////////////////////////////////////////////////////////////////////////////

BookmarksUI::BookmarksUI(TabContents* contents) : DOMUI(contents) {
  BookmarksUIHTMLSource* html_source = new BookmarksUIHTMLSource();

  // Set up the chrome://bookmarks/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

// static
RefCountedMemory* BookmarksUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_BOOKMARKS_FAVICON);
}
