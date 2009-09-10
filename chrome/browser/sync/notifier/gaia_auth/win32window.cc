// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Originally from libjingle. Minor alterations to compile it in Chrome.

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/win32window.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

static const wchar_t kWindowBaseClassName[] = L"WindowBaseClass";
HINSTANCE instance_ = GetModuleHandle(NULL);
ATOM window_class_ = 0;

Win32Window::Win32Window() : wnd_(NULL) {
}

Win32Window::~Win32Window() {
  ASSERT(NULL == wnd_);
}

bool Win32Window::Create(HWND parent, const wchar_t* title, DWORD style,
                         DWORD exstyle, int x, int y, int cx, int cy) {
  if (wnd_) {
    // Window already exists.
    return false;
  }

  if (!window_class_) {
    // Class not registered, register it.
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.hInstance = instance_;
    wcex.lpfnWndProc = &Win32Window::WndProc;
    wcex.lpszClassName = kWindowBaseClassName;
    window_class_ = ::RegisterClassEx(&wcex);
    if (!window_class_) {
      LOG_GLE(LS_ERROR) << "RegisterClassEx failed";
      return false;
    }
  }
  wnd_ = ::CreateWindowEx(exstyle, kWindowBaseClassName, title, style,
                          x, y, cx, cy, parent, NULL, instance_, this);
  return (NULL != wnd_);
}

void Win32Window::Destroy() {
  VERIFY(::DestroyWindow(wnd_) != FALSE);
}

#if 0
void Win32Window::SetInstance(HINSTANCE instance) {
  instance_ = instance;
}

void Win32Window::Shutdown() {
  if (window_class_) {
    ::UnregisterClass(MAKEINTATOM(window_class_), instance_);
    window_class_ = 0;
  }
}
#endif

bool Win32Window::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                            LRESULT& result) {
  switch (uMsg) {
  case WM_CLOSE:
    if (!OnClose()) {
      result = 0;
      return true;
    }
    break;
  }
  return false;
}

LRESULT Win32Window::WndProc(HWND hwnd, UINT uMsg,
                             WPARAM wParam, LPARAM lParam) {
  Win32Window* that = reinterpret_cast<Win32Window*>(
      ::GetWindowLongPtr(hwnd, GWL_USERDATA));
  if (!that && (WM_CREATE == uMsg)) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    that = static_cast<Win32Window*>(cs->lpCreateParams);
    that->wnd_ = hwnd;
    ::SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(that));
  }
  if (that) {
    LRESULT result;
    bool handled = that->OnMessage(uMsg, wParam, lParam, result);
    if (WM_DESTROY == uMsg) {
      for (HWND child = ::GetWindow(hwnd, GW_CHILD); child;
           child = ::GetWindow(child, GW_HWNDNEXT)) {
        LOG(LS_INFO) << "Child window: " << static_cast<void*>(child);
      }
    }
    if (WM_NCDESTROY == uMsg) {
      ::SetWindowLongPtr(hwnd, GWL_USERDATA, NULL);
      that->wnd_ = NULL;
      that->OnDestroyed();
    }
    if (handled) {
      return result;
    }
  }
  return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace talk_base
