// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_bridge.h"

#include "base/message_loop.h"
#include "chrome/browser/importer/importer.h"
#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
#endif
#include "webkit/glue/password_form.h"

InProcessImporterBridge::InProcessImporterBridge(ProfileWriter* writer,
                                                 MessageLoop* delegate_loop,
                                                 ImporterHost* host)
    : ImporterBridge(writer, delegate_loop, host),
      main_loop_(MessageLoop::current()),
      delegate_loop_(NULL) {
}

void InProcessImporterBridge::AddBookmarkEntries(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
    const std::wstring& first_folder_name,
    int options) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
    &ProfileWriter::AddBookmarkEntry, bookmarks, first_folder_name,
    options));
}

void InProcessImporterBridge::AddHomePage(const GURL &home_page) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHomepage, home_page));
}

#if defined(OS_WIN)
void InProcessImporterBridge::AddIE7PasswordInfo(
    const IE7PasswordInfo password_info) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddIE7PasswordInfo, password_info));
}
#endif  // OS_WIN

void InProcessImporterBridge::SetFavIcons(
    const std::vector<history::ImportedFavIconUsage>& fav_icons) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddFavicons, fav_icons));
}

void InProcessImporterBridge::SetHistoryItems(
    const std::vector<history::URLRow> &rows) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddHistoryPage, rows));
}

void InProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddKeywords, template_urls, default_keyword_index,
      unique_on_host_and_path));
}

void InProcessImporterBridge::SetPasswordForm(
    const webkit_glue::PasswordForm& form) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddPasswordForm, form));
}

void InProcessImporterBridge::NotifyItemStarted(ImportItem item) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(host_,
      &ImporterHost::ImportItemStarted, item));
}

void InProcessImporterBridge::NotifyItemEnded(ImportItem item) {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(host_,
      &ImporterHost::ImportItemEnded, item));
}

void InProcessImporterBridge::NotifyStarted() {
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(host_,
      &ImporterHost::ImportStarted));
}

void InProcessImporterBridge::NotifyEnded() {
  main_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(host_, &ImporterHost::ImportEnded));
}
