// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/page_usage_data.h"

#include <algorithm>

#include "third_party/skia/include/core/SkBitmap.h"

PageUsageData::~PageUsageData() {
  delete thumbnail_;
  delete favicon_;
}

void PageUsageData::SetThumbnail(SkBitmap* img) {
  if (thumbnail_ && thumbnail_ != img)
    delete thumbnail_;

  thumbnail_ = img;
  thumbnail_set_ = true;
}

void PageUsageData::SetFavIcon(SkBitmap* img) {
  if (favicon_ && favicon_ != img)
    delete favicon_;
  favicon_ = img;
  favicon_set_ = true;
}

// static
bool PageUsageData::Predicate(const PageUsageData* lhs,
                              const PageUsageData* rhs) {
  return lhs->GetScore() > rhs->GetScore();
}
