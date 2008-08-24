// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/separator.h"

#include "base/logging.h"
#include "chrome/views/hwnd_view.h"

namespace ChromeViews {

static const int kSeparatorSize = 2;

Separator::Separator() {
  SetFocusable(false);
}

Separator::~Separator() {
}

HWND Separator::CreateNativeControl(HWND parent_container) {
  SetFixedHeight(kSeparatorSize, CENTER);

  return ::CreateWindowEx(GetAdditionalExStyle(), L"STATIC", L"",
                          WS_CHILD | SS_ETCHEDHORZ | SS_SUNKEN,
                          0, 0, GetWidth(), GetHeight(),
                          parent_container, NULL, NULL, NULL);
}

LRESULT Separator::OnNotify(int w_param, LPNMHDR l_param) {
  return 0;
}

void Separator::GetPreferredSize(CSize* out) {
  DCHECK(out);
  out->cx = GetWidth();
  out->cy = fixed_height_;
}

}

