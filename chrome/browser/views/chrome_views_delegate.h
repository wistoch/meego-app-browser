// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CHROME_VIEWS_DELEGATE_H_
#define CHROME_BROWSER_VIEWS_CHROME_VIEWS_DELEGATE_H_
#pragma once

#include "base/logging.h"
#include "views/views_delegate.h"

class ChromeViewsDelegate : public views::ViewsDelegate {
 public:
  ChromeViewsDelegate() {}
  virtual ~ChromeViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual Clipboard* GetClipboard() const;
  virtual void SaveWindowPlacement(const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized);
  virtual bool GetSavedWindowBounds(const std::wstring& window_name,
                                    gfx::Rect* bounds) const;
  virtual bool GetSavedMaximizedState(const std::wstring& window_name,
                                      bool* maximized) const;
  virtual void NotifyAccessibilityEvent(
    views::View* view, AccessibilityTypes::Event event_type) {}
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const;
#endif
  virtual void AddRef();
  virtual void ReleaseRef();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeViewsDelegate);
};

#endif  // CHROME_BROWSER_VIEWS_CHROME_VIEWS_DELEGATE_H_
