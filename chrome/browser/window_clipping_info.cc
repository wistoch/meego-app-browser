// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/window_clipping_info.h"

#define DEBUG_WINDOW_CLIPPING_INFO 0

WindowClippingInfo::WindowClippingInfo(HWND aWindow,
                                       HWND ignore_wnd) : hwnd_(aWindow),
                                                          ignore_(ignore_wnd),
                                                          rgn_(NULL) {
  ::GetWindowRect(hwnd_, &hwnd_rect_);
  ComputeScreenClipping();
  ComputeWindowsClipping();
  if (rgn_) {
    ::OffsetRgn(rgn_, -hwnd_rect_.left, -hwnd_rect_.top);
#if DEBUG_WINDOW_CLIPPING_INFO
    RECT r;
    ::GetRgnBox(rgn_, &r);
    LOG(INFO) << "Window clip rect is (" << r.left << ", " << r.top << ", "
              << r.right << ", " << r.bottom << ")";
#endif
  }
}

WindowClippingInfo::~WindowClippingInfo() {
  if (rgn_) {
    DeleteObject(rgn_);
  }
}

HRGN WindowClippingInfo::GetClippedRegion() const {
  return rgn_;
}

BOOL WindowClippingInfo::IsClipped() const {
  if (rgn_ == NULL) {
    return FALSE;
  } else {
    CRect r;
    ::GetRgnBox(rgn_, &r);
    if (r.Width() > 0 && r.Height() > 0) {
      return TRUE;
    } else {
      return FALSE;
    }
  }
}

HRGN WindowClippingInfo::CombineRegions(HRGN existing, HRGN additional) const {
  if (existing == NULL) {
    return additional;
  } else {
    HRGN new_dest = CreateRectRgn(0, 0, 0, 0);
    ::CombineRgn(new_dest, existing, additional, RGN_OR);
    DeleteObject(existing);
    DeleteObject(additional);
    return new_dest;
  }
}

void WindowClippingInfo::ComputeScreenClipping() {
  int screen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  CRect c;

  if (hwnd_rect_.left < 0) {
    c.left = 0;
    c.right = -hwnd_rect_.left;
    c.top = 0;
    c.bottom = hwnd_rect_.Height();
    rgn_ = CombineRegions(rgn_,
                          CreateRectRgn(c.left, c.top, c.right, c.bottom));
  }

  if (hwnd_rect_.top < 0) {
    c.left = 0;
    c.right = hwnd_rect_.Width();
    c.top = 0;
    c.bottom = -hwnd_rect_.top;
    rgn_ = CombineRegions(rgn_,
                          CreateRectRgn(c.left, c.top, c.right, c.bottom));
  }

  if (hwnd_rect_.right > screen_width) {
    c.left = screen_width - hwnd_rect_.left;
    c.right = hwnd_rect_.right - screen_width + c.left;
    c.top = 0;
    c.bottom = hwnd_rect_.Height();
    rgn_ = CombineRegions(rgn_,
                          CreateRectRgn(c.left, c.top, c.right, c.bottom));
  }

  if (hwnd_rect_.bottom > screen_height) {
    c.left = 0;
    c.right = hwnd_rect_.Width();
    c.top = screen_height - hwnd_rect_.top;
    c.bottom = hwnd_rect_.bottom - screen_height + c.top;
    rgn_ = CombineRegions(rgn_,
                          CreateRectRgn(c.left, c.top, c.right, c.bottom));
  }

  if (rgn_) {
    // Convert the region in screen coordinate system to be compatible with
    // the the windows clipping regions
    ::OffsetRgn(rgn_, hwnd_rect_.left, hwnd_rect_.top);
#if DEBUG_WINDOW_CLIPPING_INFO
    RECT cr;
    ::GetRgnBox(rgn_, &cr);
    LOG(INFO) << "Screen Clip is (" << cr.left << ", " << cr.top << ", " <<
      cr.right << ", " << cr.bottom << ")";
#endif
  } else {
#if DEBUG_WINDOW_CLIPPING_INFO
    LOG(INFO) << "Screen Clip is null";
#endif
  }
}


// WindowEnumProc is called for every top level windows until we
// return FALSE or all top level windows have been enumerated.
//
// Windows are ordered per Z-Order with higher level windows first and
// lower level windows last.
//
//static
BOOL CALLBACK WindowClippingInfo::WindowEnumProc(HWND hwnd, LPARAM lParam) {
  WindowClippingInfo* wci =
    reinterpret_cast<WindowClippingInfo*>(lParam);

  if (hwnd == wci->ignore_) {
    return TRUE;
  }

  if (hwnd == wci->hwnd_) {
    // We have enumerated all the windows above us so we are done
    return FALSE;
  }

  if (::IsWindowVisible(hwnd)) {
    RECT r;
    ::GetWindowRect(hwnd, &r);
    RECT intersection;
    if (::IntersectRect(&intersection, &r, &wci->hwnd_rect_)) {
      HRGN rgn = CreateRectRgn(intersection.left,
                               intersection.top,
                               intersection.right,
                               intersection.bottom);
      if (wci->rgn_ == NULL) {
        wci->rgn_ = rgn;
      } else {
        ::CombineRgn(wci->rgn_, wci->rgn_, rgn, RGN_OR);
        ::DeleteObject(rgn);
      }
    }
  }
  return TRUE;
}

void WindowClippingInfo::ComputeWindowsClipping() {
  ::EnumWindows(WindowEnumProc, reinterpret_cast<LPARAM>(this));
}

