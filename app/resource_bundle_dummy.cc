// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include <windows.h>

#include "base/lock.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "gfx/font.h"

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

// NOTE(gregoryd): This is a hack to avoid creating more nacl_win64-specific
// files. The font members of ResourceBundle are never initialized in our code
// so this destructor is never called.
namespace gfx {
  Font::HFontRef::~HFontRef() {
    NOTREACHED();
  }
}


/* static */
std::string ResourceBundle::InitSharedInstance(
    const std::wstring& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();
  return std::string();
}

/* static */
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

/* static */
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

ResourceBundle::ResourceBundle()
    : lock_(new Lock),
      resources_data_(NULL),
      locale_resources_data_(NULL) {
}

ResourceBundle::~ResourceBundle() {
}


string16 ResourceBundle::GetLocalizedString(int message_id) {
  return std::wstring();
}
