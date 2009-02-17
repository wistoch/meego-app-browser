// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/resource_bundle.h"

#include "base/base_paths.h"
#include "base/data_pack.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"

ResourceBundle::~ResourceBundle() {
  FreeImages();

  delete locale_resources_data_;
  locale_resources_data_ = NULL;
  delete theme_data_;
  theme_data_ = NULL;
  delete resources_data_;
  resources_data_ = NULL;
}

void ResourceBundle::LoadResources(const std::wstring& pref_locale) {
  FilePath resources_data_path;
  PathService::Get(base::DIR_EXE, &resources_data_path);
  resources_data_path = resources_data_path.Append(
      FILE_PATH_LITERAL("chrome.pak"));
  DCHECK(resources_data_ == NULL) << "resource data already loaded!";
  resources_data_ = new base::DataPack;
  bool success = resources_data_->Load(resources_data_path);
  DCHECK(success) << "failed to load chrome.pak";

  // TODO(tc): Load the .pak file for locale_resources_data_.
}

FilePath ResourceBundle::GetLocaleFilePath(const std::wstring& pref_locale) {
  FilePath locale_path;
  PathService::Get(chrome::DIR_LOCALES, &locale_path);

  const std::wstring app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (app_locale.empty())
    return FilePath();

  return locale_path.Append(WideToASCII(app_locale + L".pak"));
}

void ResourceBundle::LoadThemeResources() {
  FilePath theme_data_path;
  PathService::Get(chrome::DIR_THEMES, &theme_data_path);
  theme_data_path = theme_data_path.Append(FILE_PATH_LITERAL("default.pak"));
  theme_data_ = new base::DataPack;
  bool success = theme_data_->Load(theme_data_path);
  DCHECK(success) << "failed to load theme data";
}

/* static */
bool ResourceBundle::LoadResourceBytes(DataHandle module, int resource_id,
                                       std::vector<unsigned char>* bytes) {
  DCHECK(module);
  StringPiece data;
  if (!module->Get(resource_id, &data))
    return false;

  bytes->resize(data.length());
  memcpy(&(bytes->front()), data.data(), data.length());

  return true;
}

StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  DCHECK(resources_data_);
  StringPiece data;
  if (!resources_data_->Get(resource_id, &data))
    return StringPiece();
  return data;
}

std::wstring ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return std::wstring();
  }

  StringPiece data;
  if (!locale_resources_data_->Get(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return std::wstring();
    }
  }
  // Copy into a wstring and return.
  return UTF8ToWide(data.as_string());
}
