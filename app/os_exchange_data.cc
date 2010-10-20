// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/os_exchange_data.h"

#include "base/pickle.h"
#include "googleurl/src/gurl.h"

OSExchangeData::DownloadFileInfo::DownloadFileInfo(
    const FilePath& filename,
    DownloadFileProvider* downloader)
    : filename(filename),
      downloader(downloader) {
}

OSExchangeData::DownloadFileInfo::~DownloadFileInfo() {}

OSExchangeData::OSExchangeData() : provider_(CreateProvider()) {
}

OSExchangeData::OSExchangeData(Provider* provider) : provider_(provider) {
}

OSExchangeData::~OSExchangeData() {
}

void OSExchangeData::SetString(const std::wstring& data) {
  provider_->SetString(data);
}

void OSExchangeData::SetURL(const GURL& url, const std::wstring& title) {
  provider_->SetURL(url, title);
}

void OSExchangeData::SetFilename(const std::wstring& full_path) {
  provider_->SetFilename(full_path);
}

void OSExchangeData::SetPickledData(CustomFormat format, const Pickle& data) {
  provider_->SetPickledData(format, data);
}

bool OSExchangeData::GetString(std::wstring* data) const {
  return provider_->GetString(data);
}

bool OSExchangeData::GetURLAndTitle(GURL* url, std::wstring* title) const {
  return provider_->GetURLAndTitle(url, title);
}

bool OSExchangeData::GetFilename(std::wstring* full_path) const {
  return provider_->GetFilename(full_path);
}

bool OSExchangeData::GetPickledData(CustomFormat format, Pickle* data) const {
  return provider_->GetPickledData(format, data);
}

bool OSExchangeData::HasString() const {
  return provider_->HasString();
}

bool OSExchangeData::HasURL() const {
  return provider_->HasURL();
}

bool OSExchangeData::HasFile() const {
  return provider_->HasFile();
}

bool OSExchangeData::HasCustomFormat(CustomFormat format) const {
  return provider_->HasCustomFormat(format);
}

bool OSExchangeData::HasAllFormats(
    int formats,
    const std::set<CustomFormat>& custom_formats) const {
  if ((formats & STRING) != 0 && !HasString())
    return false;
  if ((formats & URL) != 0 && !HasURL())
    return false;
#if defined(OS_WIN)
  if ((formats & FILE_CONTENTS) != 0 && !provider_->HasFileContents())
    return false;
  if ((formats & HTML) != 0 && !provider_->HasHtml())
    return false;
#endif
  if ((formats & FILE_NAME) != 0 && !provider_->HasFile())
    return false;
  for (std::set<CustomFormat>::const_iterator i = custom_formats.begin();
       i != custom_formats.end(); ++i) {
    if (!HasCustomFormat(*i))
      return false;
  }
  return true;
}

bool OSExchangeData::HasAnyFormat(
    int formats,
    const std::set<CustomFormat>& custom_formats) const {
  if ((formats & STRING) != 0 && HasString())
    return true;
  if ((formats & URL) != 0 && HasURL())
    return true;
#if defined(OS_WIN)
  if ((formats & FILE_CONTENTS) != 0 && provider_->HasFileContents())
    return true;
  if ((formats & HTML) != 0 && provider_->HasHtml())
    return true;
#endif
  if ((formats & FILE_NAME) != 0 && provider_->HasFile())
    return true;
  for (std::set<CustomFormat>::const_iterator i = custom_formats.begin();
       i != custom_formats.end(); ++i) {
    if (HasCustomFormat(*i))
      return true;
  }
  return false;
}

#if defined(OS_WIN)
void OSExchangeData::SetFileContents(const std::wstring& filename,
                                     const std::string& file_contents) {
  provider_->SetFileContents(filename, file_contents);
}

void OSExchangeData::SetHtml(const std::wstring& html, const GURL& base_url) {
  provider_->SetHtml(html, base_url);
}

bool OSExchangeData::GetFileContents(std::wstring* filename,
                                     std::string* file_contents) const {
  return provider_->GetFileContents(filename, file_contents);
}

bool OSExchangeData::GetHtml(std::wstring* html, GURL* base_url) const {
  return provider_->GetHtml(html, base_url);
}

void OSExchangeData::SetDownloadFileInfo(const DownloadFileInfo& download) {
  return provider_->SetDownloadFileInfo(download);
}
#endif
